#include <st2110/backends/receive_local_policy.hpp>

#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#if (__linux__)
#include <array>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace st2110 {

#if (__linux__)

namespace {

struct ScopedFd {
    int fd = -1;

    ~ScopedFd() {
        if (fd >= 0) {
            ::close(fd);
        }
    }
};

struct ScopedIfAddrs {
    ifaddrs *value = nullptr;

    ~ScopedIfAddrs() {
        if (value) {
            ::freeifaddrs(value);
        }
    }
};

[[nodiscard]] std::expected<std::string, Error> sockaddr_to_ip_string(const sockaddr *addr,
                                                                      SocketAddressFamily family) {
    char buffer[INET6_ADDRSTRLEN]{};

    switch (family) {
    case SocketAddressFamily::IPv4: {
        if (!addr || addr->sa_family != AF_INET) {
            return std::unexpected(Error::InvalidValue);
        }

        const auto *ipv4 = reinterpret_cast<const sockaddr_in *>(addr);
        if (::inet_ntop(AF_INET, &ipv4->sin_addr, buffer, sizeof(buffer)) == nullptr) {
            return std::unexpected(Error::SystemFailure);
        }

        return std::string(buffer);
    }

    case SocketAddressFamily::IPv6: {
        if (!addr || addr->sa_family != AF_INET6) {
            return std::unexpected(Error::InvalidValue);
        }

        const auto *ipv6 = reinterpret_cast<const sockaddr_in6 *>(addr);
        if (::inet_ntop(AF_INET6, &ipv6->sin6_addr, buffer, sizeof(buffer)) == nullptr) {
            return std::unexpected(Error::SystemFailure);
        }

        return std::string(buffer);
    }
    }

    return std::unexpected(Error::InvalidValue);
}

[[nodiscard]] std::optional<std::string>
find_interface_name_for_local_ip(SocketAddressFamily family, const std::string &local_ip) {
    ifaddrs *interfaces = nullptr;
    if (::getifaddrs(&interfaces) != 0) {
        return std::nullopt;
    }

    ScopedIfAddrs guard{.value = interfaces};

    const int expected_family = family == SocketAddressFamily::IPv4 ? AF_INET : AF_INET6;

    for (const ifaddrs *it = interfaces; it; it = it->ifa_next) {
        if (!it->ifa_addr || !it->ifa_name || it->ifa_addr->sa_family != expected_family) {
            continue;
        }

        auto candidate_ip = sockaddr_to_ip_string(it->ifa_addr, family);
        if (!candidate_ip.has_value()) {
            continue;
        }

        if (*candidate_ip == local_ip) {
            return std::string(it->ifa_name);
        }
    }

    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> resolve_pci_bdf_for_interface(std::string_view interface_name) {
    if (interface_name.empty()) {
        return std::nullopt;
    }

    const std::string device_link = "/sys/class/net/" + std::string(interface_name) + "/device";

    std::array<char, 4096> target{};
    const ssize_t len = ::readlink(device_link.c_str(), target.data(), target.size() - 1);
    if (len <= 0) {
        return std::nullopt;
    }

    target[static_cast<std::size_t>(len)] = '\0';

    const std::string target_path(target.data());
    const std::size_t slash = target_path.find_last_of('/');
    const std::string bdf = slash == std::string::npos ? target_path : target_path.substr(slash + 1);

    if (bdf.empty()) {
        return std::nullopt;
    }

    return bdf;
}

[[nodiscard]] std::expected<sockaddr_storage, Error>
make_remote_sockaddr(SocketAddressFamily family, const std::string &remote_ip, socklen_t &remote_addr_len) {
    sockaddr_storage remote_addr{};

    switch (family) {
    case SocketAddressFamily::IPv4: {
        sockaddr_in ipv4{};
        ipv4.sin_family = AF_INET;
        ipv4.sin_port = htons(9);

        if (::inet_pton(AF_INET, remote_ip.c_str(), &ipv4.sin_addr) != 1) {
            return std::unexpected(Error::InvalidValue);
        }

        std::memcpy(&remote_addr, &ipv4, sizeof(ipv4));
        remote_addr_len = sizeof(ipv4);
        return remote_addr;
    }

    case SocketAddressFamily::IPv6: {
        sockaddr_in6 ipv6{};
        ipv6.sin6_family = AF_INET6;
        ipv6.sin6_port = htons(9);

        if (::inet_pton(AF_INET6, remote_ip.c_str(), &ipv6.sin6_addr) != 1) {
            return std::unexpected(Error::InvalidValue);
        }

        std::memcpy(&remote_addr, &ipv6, sizeof(ipv6));
        remote_addr_len = sizeof(ipv6);
        return remote_addr;
    }
    }

    return std::unexpected(Error::InvalidValue);
}

} // namespace

[[nodiscard]] std::expected<ReceiveLocalLegPolicy, Error>
resolve_receive_local_leg_policy_for_route_target(const ReceiveRouteLookupTarget &target) {
    if (!is_valid_address(target.remote_ip, target.family)) {
        return std::unexpected(Error::InvalidValue);
    }

    const int domain = target.family == SocketAddressFamily::IPv4 ? AF_INET : AF_INET6;
    ScopedFd socket_fd{.fd = ::socket(domain, SOCK_DGRAM, 0)};
    if (socket_fd.fd < 0) {
        return std::unexpected(Error::SystemFailure);
    }

    socklen_t remote_addr_len = 0;
    auto remote_addr = make_remote_sockaddr(target.family, target.remote_ip, remote_addr_len);
    if (!remote_addr.has_value()) {
        return std::unexpected(remote_addr.error());
    }

    if (::connect(socket_fd.fd, reinterpret_cast<const sockaddr *>(&*remote_addr), remote_addr_len) != 0) {
        return std::unexpected(Error::SystemFailure);
    }

    sockaddr_storage local_addr{};
    socklen_t local_addr_len = sizeof(local_addr);
    if (::getsockname(socket_fd.fd, reinterpret_cast<sockaddr *>(&local_addr), &local_addr_len) != 0) {
        return std::unexpected(Error::SystemFailure);
    }

    auto local_ip = sockaddr_to_ip_string(reinterpret_cast<const sockaddr *>(&local_addr), target.family);
    if (!local_ip.has_value()) {
        return std::unexpected(local_ip.error());
    }

    if (!is_valid_address(*local_ip, target.family)) {
        return std::unexpected(Error::SystemFailure);
    }

    auto interface_name = find_interface_name_for_local_ip(target.family, *local_ip);

    std::optional<std::string> pci_bdf{};
    if (interface_name.has_value()) {
        pci_bdf = resolve_pci_bdf_for_interface(*interface_name);
    }

    return ReceiveLocalLegPolicy{
        .family = target.family,
        .local_ip = std::move(*local_ip),
        .local_interface_name = std::move(interface_name),
        .local_pci_bdf = std::move(pci_bdf),
    };
}

[[nodiscard]] std::expected<std::string, Error>
resolve_preferred_local_ip_for_remote_target(SocketAddressFamily family, const std::string &remote_ip) {
    auto local_policy = resolve_receive_local_leg_policy_for_route_target(ReceiveRouteLookupTarget{
        .family = family,
        .remote_ip = remote_ip,
    });

    if (!local_policy.has_value()) {
        return std::unexpected(local_policy.error());
    }

    return std::move(local_policy->local_ip);
}

#else

std::expected<ReceiveLocalLegPolicy, Error>
resolve_receive_local_leg_policy_for_route_target(const ReceiveRouteLookupTarget &) {
    return std::unexpected(Error::Unsupported);
}

std::expected<std::string, Error> resolve_preferred_local_ip_for_remote_target(SocketAddressFamily,
                                                                               const std::string &) {
    return std::unexpected(Error::Unsupported);
}

#endif

[[nodiscard]] std::expected<ReceiveLocalPolicy, Error>
auto_select_receive_local_policy(const ReceiveBootstrap &bootstrap) {
    ReceiveLocalPolicy policy{};
    policy.legs.reserve(bootstrap.legs.size());

    for (const ReceiveRemoteLeg &leg : bootstrap.legs) {
        auto target = determine_receive_route_lookup_target(leg);
        if (!target.has_value()) {
            return std::unexpected(target.error());
        }

        auto local_leg_policy = resolve_receive_local_leg_policy_for_route_target(*target);
        if (!local_leg_policy.has_value()) {
            return std::unexpected(local_leg_policy.error());
        }

        policy.legs.push_back(std::move(*local_leg_policy));
    }

    return policy;
}

} // namespace st2110
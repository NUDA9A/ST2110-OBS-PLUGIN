#include <st2110/backends/receive_local_policy.hpp>

#include <cstring>
#include <utility>

#if (__linux__)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace st2110 {
#if (__linux__)
[[nodiscard]] std::expected<std::string, Error>
resolve_preferred_local_ip_for_remote_target(SocketAddressFamily family, const std::string &remote_ip) {
    if (!is_valid_address(remote_ip, family)) {
        return std::unexpected(Error::InvalidValue);
    }

    struct ScopedFd {
        int fd = -1;

        ~ScopedFd() {
            if (fd >= 0) {
                ::close(fd);
            }
        }
    };

    const int domain = family == SocketAddressFamily::IPv4 ? AF_INET : AF_INET6;
    ScopedFd socket_fd{.fd = ::socket(domain, SOCK_DGRAM, 0)};
    if (socket_fd.fd < 0) {
        return std::unexpected(Error::SystemFailure);
    }

    sockaddr_storage remote_addr{};
    socklen_t remote_addr_len = 0;

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
        break;
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
        break;
    }
    }

    if (::connect(socket_fd.fd, reinterpret_cast<const sockaddr *>(&remote_addr), remote_addr_len) != 0) {
        return std::unexpected(Error::SystemFailure);
    }

    sockaddr_storage local_addr{};
    socklen_t local_addr_len = sizeof(local_addr);
    if (::getsockname(socket_fd.fd, reinterpret_cast<sockaddr *>(&local_addr), &local_addr_len) != 0) {
        return std::unexpected(Error::SystemFailure);
    }

    char local_ip_buffer[INET6_ADDRSTRLEN]{};
    const void *raw_addr = nullptr;
    int local_family = AF_UNSPEC;

    switch (family) {
    case SocketAddressFamily::IPv4: {
        if (local_addr.ss_family != AF_INET) {
            return std::unexpected(Error::SystemFailure);
        }

        const auto *ipv4 = reinterpret_cast<const sockaddr_in *>(&local_addr);
        raw_addr = &ipv4->sin_addr;
        local_family = AF_INET;
        break;
    }
    case SocketAddressFamily::IPv6: {
        if (local_addr.ss_family != AF_INET6) {
            return std::unexpected(Error::SystemFailure);
        }

        const auto *ipv6 = reinterpret_cast<const sockaddr_in6 *>(&local_addr);
        raw_addr = &ipv6->sin6_addr;
        local_family = AF_INET6;
        break;
    }
    }

    if (::inet_ntop(local_family, raw_addr, local_ip_buffer, sizeof(local_ip_buffer)) == nullptr) {
        return std::unexpected(Error::SystemFailure);
    }

    const std::string local_ip(local_ip_buffer);
    if (!is_valid_address(local_ip, family)) {
        return std::unexpected(Error::SystemFailure);
    }

    return local_ip;
}
#else
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

        auto local_ip = resolve_preferred_local_ip_for_remote_target(target->family, target->remote_ip);
        if (!local_ip.has_value()) {
            return std::unexpected(local_ip.error());
        }

        policy.legs.push_back(ReceiveLocalLegPolicy{
            .family = target->family,
            .local_ip = std::move(*local_ip),
        });
    }

    return policy;
}
} // namespace st2110
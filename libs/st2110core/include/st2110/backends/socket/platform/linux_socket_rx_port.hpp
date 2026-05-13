#ifndef ST2110_OBS_PLUGIN_LINUX_SOCKET_RX_PORT_HPP
#define ST2110_OBS_PLUGIN_LINUX_SOCKET_RX_PORT_HPP

#include "socket_runtime.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <expected>
#include <memory>
#include <netinet/in.h>
#include <optional>
#include <span>
#include <sys/socket.h>
#include <unistd.h>

namespace st2110 {
class LinuxSocketRxPort final : public ISocketRxPort {
  public:
    LinuxSocketRxPort() = default;

    LinuxSocketRxPort(const LinuxSocketRxPort &) = delete;
    LinuxSocketRxPort &operator=(const LinuxSocketRxPort &) = delete;

    LinuxSocketRxPort(LinuxSocketRxPort &&) = delete;
    LinuxSocketRxPort &operator=(LinuxSocketRxPort &&) = delete;

    ~LinuxSocketRxPort() override {
        if (is_open()) {
            close();
        }
    }

    [[nodiscard]] bool is_open() const noexcept override {
        return native_socket_is_valid(native_socket_) && open_cfg_.has_value();
    }

    Error open(const SocketRxOpenConfig &cfg) override {
        if (is_open()) {
            return Error::InvalidBackendState;
        }

        if (const Error err = validate_open_request(cfg); err != Error::Ok) {
            return err;
        }

        auto native_socket = create_native_socket(cfg);
        if (!native_socket) {
            return native_socket.error();
        }

        const auto native_socket_val = *native_socket;

        if (const Error err = configure_native_socket_before_bind(native_socket_val, cfg); err != Error::Ok) {
            close_native_socket(native_socket_val);
            return err;
        }

        if (const auto err = bind_native_socket(native_socket_val, cfg); err.has_value()) {
            close_native_socket(native_socket_val);
            return socket_port_error_to_error(*err);
        }

        if (const auto err = join_multicast_membership(native_socket_val, cfg); err.has_value()) {
            close_native_socket(native_socket_val);
            return socket_port_error_to_error(*err);
        }

        open_cfg_ = cfg;
        native_socket_ = native_socket_val;

        return Error::Ok;
    }

    Error close() override {
        if (!is_open()) {
            return Error::Ok;
        }

        Error result = Error::Ok;

        if (const auto err = leave_multicast_membership(native_socket_, *open_cfg_); err.has_value()) {
            result = socket_port_error_to_error(*err);
        }

        if (const Error err = close_native_socket(native_socket_); err != Error::Ok && result == Error::Ok) {
            result = err;
        }

        clear_open_state();
        return result;
    }

    [[nodiscard]] std::expected<SocketReceiveResult, Error> receive(std::span<std::uint8_t> buffer) override {
        if (!is_open()) {
            return std::unexpected(Error::InvalidBackendState);
        }

        if (buffer.empty()) {
            return std::unexpected(Error::InvalidValue);
        }

        while (true) {
            sockaddr_storage sender_addr{};
            socklen_t sender_addr_len = sizeof(sender_addr);

            const auto reply = ::recvfrom(native_socket_, buffer.data(), buffer.size(), 0,
                                          reinterpret_cast<sockaddr *>(&sender_addr), &sender_addr_len);
            if (reply < 0) {
                switch (errno) {
                case EINTR:
                    return std::unexpected(socket_port_error_to_error(SocketPortError::ReceiveInterrupted));
                case EBADF:
                case ENOTSOCK:
                    return std::unexpected(socket_port_error_to_error(SocketPortError::ReceiveAborted));
                default:
                    return std::unexpected(socket_port_error_to_error(SocketPortError::ReceiveFailed));
                }
            }

            const TimestampNs receive_timestamp_ns = local_monotonic_timestamp_ns();

            if (!sender_matches_source_filter(sender_addr, sender_addr_len, *open_cfg_)) {
                continue;
            }

            return SocketReceiveResult{.size_bytes = static_cast<std::size_t>(reply),
                                       .receive_timestamp_ns = receive_timestamp_ns};
        }
    }

  private:
    [[nodiscard]] static TimestampNs local_monotonic_timestamp_ns() noexcept {
        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();

        if (ns <= 0) {
            return 0;
        }

        return static_cast<TimestampNs>(ns);
    }

    [[nodiscard]] static std::optional<SocketPortError> join_multicast_membership(const int native_socket,
                                                                                  const SocketRxOpenConfig &cfg) {
        if (!cfg.multicast_membership) {
            return std::nullopt;
        }

        switch (cfg.multicast_membership->family) {
        case SocketAddressFamily::IPv4: {
            in_addr interface_addr{};
            if (cfg.multicast_membership->interface_address.empty()) {
                interface_addr.s_addr = htonl(INADDR_ANY);
            } else {
                if (::inet_pton(AF_INET, cfg.multicast_membership->interface_address.c_str(), &interface_addr.s_addr) !=
                    1) {
                    return SocketPortError::MulticastJoinFailed;
                }
            }

            in_addr group_addr{};
            if (::inet_pton(AF_INET, cfg.multicast_membership->group_address.c_str(), &group_addr.s_addr) != 1) {
                return SocketPortError::MulticastJoinFailed;
            }

            if (cfg.source_filter && !cfg.source_filter->source_addresses.empty()) {
                for (const std::string &source_address : cfg.source_filter->source_addresses) {
                    ip_mreq_source mreq{};
                    mreq.imr_multiaddr = group_addr;
                    mreq.imr_interface = interface_addr;

                    if (::inet_pton(AF_INET, source_address.c_str(), &mreq.imr_sourceaddr.s_addr) != 1) {
                        return SocketPortError::MulticastJoinFailed;
                    }

                    if (::setsockopt(native_socket, IPPROTO_IP, IP_ADD_SOURCE_MEMBERSHIP, &mreq, sizeof(mreq)) != 0) {
                        return SocketPortError::MulticastJoinFailed;
                    }
                }

                return std::nullopt;
            }

            ip_mreq mreq{};
            mreq.imr_multiaddr = group_addr;
            mreq.imr_interface = interface_addr;

            if (::setsockopt(native_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) != 0) {
                return SocketPortError::MulticastJoinFailed;
            }
            break;
        }
        case SocketAddressFamily::IPv6:
            return std::nullopt;
        default:
            return std::nullopt;
        }

        return std::nullopt;
    }

    [[nodiscard]] static std::optional<SocketPortError>
    leave_multicast_membership(const int native_socket, const SocketRxOpenConfig &cfg) noexcept {
        if (!cfg.multicast_membership) {
            return std::nullopt;
        }

        switch (cfg.multicast_membership->family) {
        case SocketAddressFamily::IPv4: {
            in_addr interface_addr{};
            if (cfg.multicast_membership->interface_address.empty()) {
                interface_addr.s_addr = htonl(INADDR_ANY);
            } else {
                if (::inet_pton(AF_INET, cfg.multicast_membership->interface_address.c_str(), &interface_addr.s_addr) !=
                    1) {
                    return SocketPortError::MulticastLeaveFailed;
                }
            }

            in_addr group_addr{};
            if (::inet_pton(AF_INET, cfg.multicast_membership->group_address.c_str(), &group_addr.s_addr) != 1) {
                return SocketPortError::MulticastLeaveFailed;
            }

            if (cfg.source_filter && !cfg.source_filter->source_addresses.empty()) {
                for (const std::string &source_address : cfg.source_filter->source_addresses) {
                    ip_mreq_source mreq{};
                    mreq.imr_multiaddr = group_addr;
                    mreq.imr_interface = interface_addr;

                    if (::inet_pton(AF_INET, source_address.c_str(), &mreq.imr_sourceaddr.s_addr) != 1) {
                        return SocketPortError::MulticastLeaveFailed;
                    }

                    if (::setsockopt(native_socket, IPPROTO_IP, IP_DROP_SOURCE_MEMBERSHIP, &mreq, sizeof(mreq)) != 0) {
                        return SocketPortError::MulticastLeaveFailed;
                    }
                }

                return std::nullopt;
            }

            ip_mreq mreq{};
            mreq.imr_multiaddr = group_addr;
            mreq.imr_interface = interface_addr;

            if (::setsockopt(native_socket, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) != 0) {
                return SocketPortError::MulticastLeaveFailed;
            }

            return std::nullopt;
        }
        case SocketAddressFamily::IPv6:
            return std::nullopt;
        default:
            return std::nullopt;
        }
    }

    [[nodiscard]] static bool ipv4_sender_matches_source_filter(const sockaddr_in &sender_addr,
                                                                const SocketSourceFilter &source_filter) noexcept {
        for (const std::string &source_address : source_filter.source_addresses) {
            in_addr allowed_addr{};
            if (::inet_pton(AF_INET, source_address.c_str(), &allowed_addr.s_addr) != 1) {
                continue;
            }

            if (sender_addr.sin_addr.s_addr == allowed_addr.s_addr) {
                return true;
            }
        }

        return false;
    }

    [[nodiscard]] static bool ipv6_sender_matches_source_filter(const sockaddr_in6 &sender_addr,
                                                                const SocketSourceFilter &source_filter) noexcept {
        for (const std::string &source_address : source_filter.source_addresses) {
            in6_addr allowed_addr{};
            if (::inet_pton(AF_INET6, source_address.c_str(), &allowed_addr) != 1) {
                continue;
            }

            if (std::memcmp(&sender_addr.sin6_addr, &allowed_addr, sizeof(in6_addr)) == 0) {
                return true;
            }
        }

        return false;
    }

    [[nodiscard]] static bool sender_matches_source_filter(const sockaddr_storage &sender_addr,
                                                           const socklen_t sender_addr_len,
                                                           const SocketRxOpenConfig &cfg) noexcept {
        if (!cfg.source_filter.has_value()) {
            return true;
        }

        switch (cfg.source_filter->family) {
        case SocketAddressFamily::IPv4:
            if (sender_addr.ss_family != AF_INET || sender_addr_len < sizeof(sockaddr_in)) {
                return false;
            }
            return ipv4_sender_matches_source_filter(*reinterpret_cast<const sockaddr_in *>(&sender_addr),
                                                     *cfg.source_filter);

        case SocketAddressFamily::IPv6:
            if (sender_addr.ss_family != AF_INET6 || sender_addr_len < sizeof(sockaddr_in6)) {
                return false;
            }
            return ipv6_sender_matches_source_filter(*reinterpret_cast<const sockaddr_in6 *>(&sender_addr),
                                                     *cfg.source_filter);
        }

        return false;
    }

    [[nodiscard]] static Error validate_open_request(const SocketRxOpenConfig &cfg) {
        if (const Error err = validate_current_platform_support(cfg); err != Error::Ok) {
            return err;
        }

        return Error::Ok;
    }

    [[nodiscard]] static Error validate_current_platform_support(const SocketRxOpenConfig &cfg) noexcept {
        if (cfg.multicast_membership) {
            switch (cfg.multicast_membership->family) {
            case SocketAddressFamily::IPv4:
                break;
            case SocketAddressFamily::IPv6:
                return Error::Unsupported;
            default:
                return Error::InvalidValue;
            }
        }

        return Error::Ok;
    }

    [[nodiscard]] static std::expected<int, Error> create_native_socket(const SocketRxOpenConfig &cfg) {
        int domain;
        switch (cfg.bind_endpoint.family) {
        case SocketAddressFamily::IPv4:
            domain = AF_INET;
            break;
        case SocketAddressFamily::IPv6:
            domain = AF_INET6;
            break;
        default:
            return std::unexpected(Error::InvalidValue);
        }

        const auto fd = ::socket(domain, SOCK_DGRAM, 0);
        if (fd < 0) {
            return std::unexpected(Error::SystemFailure);
        }

        return fd;
    }

    [[nodiscard]] static Error configure_native_socket_before_bind(const int native_socket,
                                                                   const SocketRxOpenConfig &cfg) {
        const int value = cfg.reuse_address ? 1 : 0;
        if (const auto res = ::setsockopt(native_socket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)); res < 0) {
            return Error::SystemFailure;
        }

        return Error::Ok;
    }

    [[nodiscard]] static std::optional<SocketPortError> bind_native_socket(const int native_socket,
                                                                           const SocketRxOpenConfig &cfg) {
        switch (cfg.bind_endpoint.family) {
        case SocketAddressFamily::IPv4: {
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(cfg.bind_endpoint.port);

            if (::inet_pton(AF_INET, cfg.bind_endpoint.address.c_str(), &addr.sin_addr) != 1) {
                return std::nullopt;
            }

            if (::bind(native_socket, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) != 0) {
                return SocketPortError::BindFailed;
            }
            break;
        }
        case SocketAddressFamily::IPv6: {
            sockaddr_in6 addr{};
            addr.sin6_family = AF_INET6;
            addr.sin6_port = htons(cfg.bind_endpoint.port);
            if (::inet_pton(AF_INET6, cfg.bind_endpoint.address.c_str(), &addr.sin6_addr) != 1) {
                return std::nullopt;
            }

            if (::bind(native_socket, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) != 0) {
                return SocketPortError::BindFailed;
            }
            break;
        }
        default:
            return std::nullopt;
        }

        return std::nullopt;
    }

    static Error close_native_socket(const int native_socket) noexcept {
        (void)::shutdown(native_socket, SHUT_RDWR);

        if (::close(native_socket) != 0) {
            return Error::SystemFailure;
        }

        return Error::Ok;
    }

    void clear_open_state() noexcept {
        native_socket_ = -1;
        open_cfg_.reset();
    }

    [[nodiscard]] static bool native_socket_is_valid(const int native_socket) noexcept { return native_socket >= 0; }

    int native_socket_ = -1;
    std::optional<SocketRxOpenConfig> open_cfg_{};
};

class LinuxSocketRxPortFactory final : public ISocketRxPortFactory {
  public:
    [[nodiscard]] std::unique_ptr<ISocketRxPort> create_port() const override {
        return std::make_unique<LinuxSocketRxPort>();
    }
};

[[nodiscard]] inline std::unique_ptr<ISocketRxPortFactory> make_linux_socket_rx_port_factory() {
    return std::make_unique<LinuxSocketRxPortFactory>();
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_LINUX_SOCKET_RX_PORT_HPP
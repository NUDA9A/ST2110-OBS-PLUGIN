#ifndef ST2110_OBS_PLUGIN_LINUX_SOCKET_RX_PORT_HPP
#define ST2110_OBS_PLUGIN_LINUX_SOCKET_RX_PORT_HPP

#include "socket_runtime.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdint>
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
        if (const Error err = bind_native_socket(native_socket_val, cfg); err != Error::Ok) {
            close_native_socket(native_socket_val);
            return err;
        }

        if (const Error err = join_multicast_membership(native_socket_val, cfg); err != Error::Ok) {
            close_native_socket(native_socket_val);
            return err;
        }

        open_cfg_ = cfg;
        native_socket_ = native_socket_val;

        return Error::Ok;
    }

    Error close() override {
        if (!is_open()) {
            return Error::Ok;
        }

        if (const Error err = leave_multicast_membership(native_socket_, *open_cfg_); err != Error::Ok) {
            return err;
        }

        if (const Error err = close_native_socket(native_socket_); err != Error::Ok) {
            return err;
        }

        clear_open_state();

        return Error::Ok;
    }

    [[nodiscard]] std::expected<SocketReceiveResult, Error> receive(std::span<std::uint8_t> buffer) override {
        if (!is_open()) {
            return std::unexpected(Error::InvalidBackendState);
        }

        if (buffer.empty()) {
            return std::unexpected(Error::InvalidValue);
        }

        const auto reply = ::recv(native_socket_, buffer.data(), buffer.size(), 0);
        if (reply < 0) {
            switch (errno) {
            case EINTR:
                return std::unexpected(Error::ReceiveInterrupted);
            case EBADF:
            case ENOTSOCK:
                return std::unexpected(Error::ReceiveAborted);
            default:
                return std::unexpected(Error::ReceiveFailed);
            }
        }

        return SocketReceiveResult{static_cast<std::size_t>(reply)};
    }

  private:
    [[nodiscard]] static Error join_multicast_membership(const int native_socket, const SocketRxOpenConfig &cfg) {
        if (!cfg.multicast_membership) {
            return Error::Ok;
        }

        switch (cfg.multicast_membership->family) {
        case SocketAddressFamily::IPv4: {
            ip_mreq mreq{};
            if (::inet_pton(AF_INET, cfg.multicast_membership->group_address.c_str(), &mreq.imr_multiaddr.s_addr) != 1) {
                return Error::MulticastJoinFailed;
            }
            if (cfg.multicast_membership->interface_address.empty()) {
                mreq.imr_interface.s_addr = htonl(INADDR_ANY);
            } else {
                if (::inet_pton(AF_INET, cfg.multicast_membership->interface_address.c_str(), &mreq.imr_interface.s_addr) != 1) {
                    return Error::MulticastJoinFailed;
                }
            }

            if (::setsockopt(native_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) != 0) {
                return Error::MulticastJoinFailed;
            }
            break;
        }
        case SocketAddressFamily::IPv6:
            return Error::Unsupported;
        default:
            return Error::InvalidValue;
        }

        return Error::Ok;
    }

    [[nodiscard]] static Error leave_multicast_membership(const int native_socket, const SocketRxOpenConfig &cfg) noexcept {
        if (!cfg.multicast_membership) {
            return Error::Ok;
        }

        switch (cfg.multicast_membership->family) {
        case SocketAddressFamily::IPv4: {
            ip_mreq mreq{};
            if (::inet_pton(AF_INET, cfg.multicast_membership->group_address.c_str(), &mreq.imr_multiaddr.s_addr) != 1) {
                return Error::MulticastLeaveFailed;
            }
            if (cfg.multicast_membership->interface_address.empty()) {
                mreq.imr_interface.s_addr = htonl(INADDR_ANY);
            } else {
                if (::inet_pton(AF_INET, cfg.multicast_membership->interface_address.c_str(), &mreq.imr_interface.s_addr) != 1) {
                    return Error::MulticastLeaveFailed;
                }
            }

            if (::setsockopt(native_socket, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) != 0) {
                return Error::MulticastLeaveFailed;
            }

            return Error::Ok;
        }
        case SocketAddressFamily::IPv6:
            return Error::Unsupported;
        default:
            return Error::InvalidValue;
        }
    }

    [[nodiscard]] static Error validate_open_request(const SocketRxOpenConfig &cfg) {
        if (const Error err = validate_socket_rx_open_config(cfg); err != Error::Ok) {
            return err;
        }
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

    [[nodiscard]] static Error bind_native_socket(const int native_socket, const SocketRxOpenConfig &cfg) {
        switch (cfg.bind_endpoint.family) {
        case SocketAddressFamily::IPv4: {
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(cfg.bind_endpoint.port);

            if (::inet_pton(AF_INET, cfg.bind_endpoint.address.c_str(), &addr.sin_addr) != 1) {
                return Error::InvalidValue;
            }

            if (::bind(native_socket, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) != 0) {
                return Error::BindFailed;
            }
            break;
        }
        case SocketAddressFamily::IPv6: {
            sockaddr_in6 addr{};
            addr.sin6_family = AF_INET6;
            addr.sin6_port = htons(cfg.bind_endpoint.port);
            if (::inet_pton(AF_INET6, cfg.bind_endpoint.address.c_str(), &addr.sin6_addr) != 1) {
                return Error::InvalidValue;
            }

            if (::bind(native_socket, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) != 0) {
                return Error::BindFailed;
            }
            break;
        }
        default:
            return Error::InvalidValue;
        }

        return Error::Ok;
    }

    static Error close_native_socket(const int native_socket) noexcept {
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
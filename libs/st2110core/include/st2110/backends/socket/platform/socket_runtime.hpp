#ifndef ST2110_OBS_PLUGIN_SOCKET_RUNTIME_HPP
#define ST2110_OBS_PLUGIN_SOCKET_RUNTIME_HPP

#include "st2110/foundation/error.hpp"
#include "st2110/ingress/shared/packet_parse.hpp"
#include "st2110/rx_config.hpp"
#include <st2110/backends/receive_local_policy.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace st2110 {
enum class SocketPortError {
    BindFailed,
    MulticastJoinFailed,
    MulticastLeaveFailed,
    ReceiveFailed,
    ReceiveInterrupted,
    ReceiveAborted,
};

[[nodiscard]] inline std::string_view socket_port_error_name(SocketPortError error) noexcept {
    switch (error) {
    case SocketPortError::BindFailed:
        return "bind_failed";
    case SocketPortError::MulticastJoinFailed:
        return "multicast_join_failed";
    case SocketPortError::MulticastLeaveFailed:
        return "multicast_leave_failed";
    case SocketPortError::ReceiveFailed:
        return "receive_failed";
    case SocketPortError::ReceiveInterrupted:
        return "receive_interrupted";
    case SocketPortError::ReceiveAborted:
        return "receive_aborted";
    }

    return "";
}

[[nodiscard]] inline Error socket_port_error_to_error(SocketPortError error) noexcept {
    switch (error) {
    case SocketPortError::BindFailed:
    case SocketPortError::MulticastJoinFailed:
    case SocketPortError::MulticastLeaveFailed:
    case SocketPortError::ReceiveFailed:
        return Error::SystemFailure;
    case SocketPortError::ReceiveInterrupted:
        return Error::OperationInterrupted;
    case SocketPortError::ReceiveAborted:
        return Error::OperationAborted;
    }

    return Error::SystemFailure;
}

[[nodiscard]] inline std::string_view socket_address_family_name(SocketAddressFamily family) noexcept {
    switch (family) {
    case SocketAddressFamily::IPv4:
        return "ipv4";
    case SocketAddressFamily::IPv6:
        return "ipv6";
    }

    return "";
}

[[nodiscard]] inline char ascii_to_lower(char c) noexcept {
    if (c >= 'A' && c <= 'Z') {
        return static_cast<char>(c - 'A' + 'a');
    }
    return c;
}

[[nodiscard]] inline bool is_ipv4_multicast_address(std::string_view address) noexcept {
    if (!is_valid_ipv4_address(address)) {
        return false;
    }

    const std::size_t dot = address.find('.');
    const auto first_block = parse_ipv4_block(address.substr(0, dot));

    return first_block && *first_block >= 224 && *first_block <= 239;
}

[[nodiscard]] inline bool is_ipv6_multicast_address(std::string_view address) noexcept {
    if (!is_valid_ipv6_address(address)) {
        return false;
    }

    const std::size_t colon = address.find(':');
    const std::string_view first_hextet = (colon == std::string_view::npos) ? address : address.substr(0, colon);

    return first_hextet.size() >= 2 && ascii_to_lower(first_hextet[0]) == 'f' && ascii_to_lower(first_hextet[1]) == 'f';
}

struct SocketEndpoint {
    SocketAddressFamily family = SocketAddressFamily::IPv4;
    std::string address{};
    uint16_t port = 0;
};

[[nodiscard]] inline Error validate_socket_endpoint(const SocketEndpoint &endpoint) {
    if (!is_valid_address(endpoint.address, endpoint.family)) {
        return Error::InvalidValue;
    }
    if (endpoint.port == 0) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

struct SocketMulticastMembership {
    SocketAddressFamily family = SocketAddressFamily::IPv4;
    std::string group_address{};
    std::string interface_address{};
};

[[nodiscard]] inline Error validate_socket_multicast_membership(const SocketMulticastMembership &membership) {
    if (!is_valid_address(membership.group_address, membership.family)) {
        return Error::InvalidValue;
    }

    if (!membership.interface_address.empty() && !is_valid_address(membership.interface_address, membership.family)) {
        return Error::InvalidValue;
    }

    switch (membership.family) {
    case SocketAddressFamily::IPv4:
        if (!is_ipv4_multicast_address(membership.group_address)) {
            return Error::InvalidValue;
        }
        break;
    case SocketAddressFamily::IPv6:
        if (!is_ipv6_multicast_address(membership.group_address)) {
            return Error::InvalidValue;
        }
    }

    return Error::Ok;
}

struct SocketRxOpenConfig {
    SocketEndpoint bind_endpoint{};
    std::optional<SocketMulticastMembership> multicast_membership{};
    bool reuse_address = true;
};

[[nodiscard]] inline Error validate_socket_rx_open_config(const SocketRxOpenConfig &cfg) {
    if (Error err = validate_socket_endpoint(cfg.bind_endpoint); err != Error::Ok) {
        return err;
    }
    if (cfg.multicast_membership) {
        if (Error err = validate_socket_multicast_membership(*cfg.multicast_membership); err != Error::Ok) {
            return err;
        }
        if (cfg.multicast_membership->family != cfg.bind_endpoint.family) {
            return Error::InvalidValue;
        }
    }

    return Error::Ok;
}

struct SocketRxOperationalCommonConfig {
    SocketRxOpenConfig open_config{};
    PacketParsePolicy packet_parse_policy{};
};

[[nodiscard]] inline bool bind_endpoint_equal(const SocketEndpoint &lhs, const SocketEndpoint &rhs) noexcept {
    return lhs.family == rhs.family && lhs.address == rhs.address && lhs.port == rhs.port;
}

[[nodiscard]] inline bool
socket_multicast_membership_equal(const SocketMulticastMembership &lhs,
                                  const SocketMulticastMembership &rhs) noexcept {
    return lhs.family == rhs.family && lhs.group_address == rhs.group_address &&
           lhs.interface_address == rhs.interface_address;
}

[[nodiscard]] inline bool socket_rx_open_config_equal(const SocketRxOpenConfig &lhs,
                                                      const SocketRxOpenConfig &rhs) noexcept {
    if (!bind_endpoint_equal(lhs.bind_endpoint, rhs.bind_endpoint)) {
        return false;
    }

    if (lhs.multicast_membership.has_value() != rhs.multicast_membership.has_value()) {
        return false;
    }

    if (lhs.multicast_membership.has_value() &&
        !socket_multicast_membership_equal(*lhs.multicast_membership, *rhs.multicast_membership)) {
        return false;
        }

    return lhs.reuse_address == rhs.reuse_address;
}

[[nodiscard]] inline Error
validate_socket_rx_operational_common_config(const SocketRxOperationalCommonConfig &cfg) {
    if (const Error err = validate_socket_rx_open_config(cfg.open_config); err != Error::Ok) {
        return err;
    }

    return Error::Ok;
}

[[nodiscard]] inline bool socket_rx_uses_multicast(const SocketRxOpenConfig &cfg) noexcept {
    return cfg.multicast_membership.has_value();
}

[[nodiscard]] inline std::expected<SocketRxOpenConfig, Error>
build_socket_rx_open_config(uint16_t udp_port, const std::string& local_ip, const std::string& dest_ip) noexcept {
    SocketRxOpenConfig res{};
    res.bind_endpoint.port = udp_port;

    auto address_to_id_family = local_ip;
    if (local_ip.empty()) {
        address_to_id_family = dest_ip;
    }

    if (is_valid_ipv4_address(address_to_id_family)) {
        res.bind_endpoint.family = SocketAddressFamily::IPv4;
    } else if (is_valid_ipv6_address(address_to_id_family)) {
        res.bind_endpoint.family = SocketAddressFamily::IPv6;
    } else {
        return std::unexpected(Error::InvalidValue);
    }

    if (!is_valid_address(dest_ip, res.bind_endpoint.family)) {
        return std::unexpected(Error::InvalidValue);
    }

    res.bind_endpoint.address = local_ip;

    if (local_ip.empty()) {
        switch (res.bind_endpoint.family) {
        case SocketAddressFamily::IPv4:
            res.bind_endpoint.address = "0.0.0.0";
            break;
        case SocketAddressFamily::IPv6:
            res.bind_endpoint.address = "::";
            break;
        }
    }

    if (is_ipv4_multicast_address(dest_ip)) {
        res.bind_endpoint.address = "0.0.0.0";
        res.multicast_membership = SocketMulticastMembership{
            .family = SocketAddressFamily::IPv4,
            .group_address = dest_ip,
        };
        if (!local_ip.empty()) {
            res.multicast_membership->interface_address = local_ip;
        }
    } else if (is_ipv6_multicast_address(dest_ip)) {
        res.bind_endpoint.address = "::";
        res.multicast_membership = SocketMulticastMembership{
            .family = SocketAddressFamily::IPv6,
            .group_address = dest_ip,
        };
        if (!local_ip.empty()) {
            res.multicast_membership->interface_address = local_ip;
        }
    }

    if (Error err = validate_socket_rx_open_config(res); err != Error::Ok) {
        return std::unexpected(err);
    }

    return res;
}

[[nodiscard]] inline std::expected<SocketRxOpenConfig, Error>
socket_rx_open_config_from_video_config(const RxVideoConfig &cfg) {
    if (Error err = validate_rx_video_config(cfg); err != Error::Ok) {
        return std::unexpected(err);
    }

    return build_socket_rx_open_config(cfg.udp_port, cfg.local_ip, cfg.dest_ip);
}

[[nodiscard]] inline std::expected<SocketRxOpenConfig, Error>
socket_rx_open_config_from_audio_config(const RxAudioConfig &cfg) {
    if (Error err = validate_rx_audio_config(cfg); err != Error::Ok) {
        return std::unexpected(err);
    }

    return build_socket_rx_open_config(cfg.udp_port, cfg.local_ip, cfg.dest_ip);
}

struct SocketReceiveResult {
    std::size_t size_bytes = 0;
};

class ISocketRxPort {
  public:
    [[nodiscard]] virtual bool is_open() const noexcept = 0;

    virtual Error open(const SocketRxOpenConfig &cfg) = 0;

    virtual Error close() = 0;

    [[nodiscard]] virtual std::expected<SocketReceiveResult, Error> receive(std::span<std::uint8_t> buffer) = 0;

    virtual ~ISocketRxPort() = default;
};

class ISocketRxPortFactory {
  public:
    [[nodiscard]] virtual std::unique_ptr<ISocketRxPort> create_port() const = 0;

    virtual ~ISocketRxPortFactory() = default;
};
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_SOCKET_RUNTIME_HPP

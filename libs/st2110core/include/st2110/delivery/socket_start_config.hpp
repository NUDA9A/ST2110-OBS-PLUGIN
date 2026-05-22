#ifndef ST2110_OBS_SOCKET_START_CONFIG_HPP
#define ST2110_OBS_SOCKET_START_CONFIG_HPP

#include <st2110/backends/receive_local_policy.hpp>
#include <st2110/backends/socket/platform/socket_runtime.hpp>
#include <st2110/receive/shared/receive_bootstrap.hpp>
#include <st2110/receive/shared/receive_reorder_tolerance_policy.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace st2110 {
struct SocketMediaLegConfig {
    SocketAddressFamily family = SocketAddressFamily::IPv4;
    std::string local_ip{};

    std::string dest_ip{};
    std::uint16_t udp_port = 0;

    SourceFilterSignaling source_filter{};
    std::size_t max_udp_datagram_bytes = 1460;

    SocketRxOpenConfig open_config{};
};

struct SocketStartConfig {
    ReceiveTopologyKind topology = ReceiveTopologyKind::SingleStream;
    ReorderBufferConfig reorder_buffer_config{};

    std::vector<SocketMediaLegConfig> legs{};
};

[[nodiscard]] inline std::optional<SocketSourceFilter>
make_socket_source_filter(const SourceFilterSignaling &source_filter, const SocketAddressFamily family) {
    if (source_filter.source_addresses.empty()) {
        return std::nullopt;
    }

    return SocketSourceFilter{
        .family = family,
        .source_addresses = source_filter.source_addresses,
    };
}

[[nodiscard]] inline SocketMediaLegConfig make_socket_media_leg_config(const ReceiveRemoteLeg &remote_leg,
                                                                       const ReceiveLocalLegPolicy &local_leg) {
    const auto socket_source_filter = make_socket_source_filter(remote_leg.source_filter, local_leg.family);

    const auto open_config = build_socket_rx_open_config(
        remote_leg.udp_port, local_leg.local_ip, remote_leg.destination.destination_address, socket_source_filter);
    if (!open_config) {
        throw std::runtime_error("Can not build socket open config");
    }

    return SocketMediaLegConfig{
        .family = local_leg.family,
        .local_ip = local_leg.local_ip,
        .dest_ip = remote_leg.destination.destination_address,
        .udp_port = remote_leg.udp_port,
        .source_filter = remote_leg.source_filter,
        .max_udp_datagram_bytes = remote_leg.max_udp_datagram_bytes,
        .open_config = *open_config,
    };
}

[[nodiscard]] inline std::vector<SocketMediaLegConfig>
make_socket_media_leg_configs(const ReceiveBootstrap &bootstrap, const ReceiveLocalPolicy &local_policy) {
    if (bootstrap.legs.size() != local_policy.legs.size()) {
        throw std::runtime_error("Socket receive local policy leg count mismatch");
    }

    std::vector<SocketMediaLegConfig> legs;
    legs.reserve(bootstrap.legs.size());

    for (std::size_t i = 0; i < bootstrap.legs.size(); ++i) {
        legs.emplace_back(make_socket_media_leg_config(bootstrap.legs[i], local_policy.legs[i]));
    }

    return legs;
}

} // namespace st2110

#endif // ST2110_OBS_SOCKET_START_CONFIG_HPP
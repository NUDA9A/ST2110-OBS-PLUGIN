#ifndef ST2110_OBS_SOCKET_START_CONFIG_HPP
#define ST2110_OBS_SOCKET_START_CONFIG_HPP

#include <st2110/backends/receive_local_policy.hpp>
#include <st2110/backends/socket/platform/socket_runtime.hpp>
#include <st2110/receive/shared/receive_reorder_tolerance_policy.hpp>

namespace st2110 {
struct SocketMediaLegConfig {
    SocketAddressFamily family;
    std::string local_ip;

    std::string dest_ip;
    std::uint16_t udp_port;

    SourceFilterSignaling source_filter{};
    std::size_t max_udp_datagram_bytes = 1460;

    SocketRxOpenConfig open_config{};
};

struct SocketStartConfig {
    ReceiveTopologyKind topology = ReceiveTopologyKind::SingleStream;
    ReorderBufferConfig reorder_buffer_config{};

    std::vector<SocketMediaLegConfig> legs{};
};
} // namespace st2110

#endif // ST2110_OBS_SOCKET_START_CONFIG_HPP

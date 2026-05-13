#ifndef ST2110_OBS_SOCKET_VIDEO_START_CONFIG_HPP
#define ST2110_OBS_SOCKET_VIDEO_START_CONFIG_HPP

#include <st2110/backends/receive_local_policy.hpp>
#include <st2110/backends/socket/platform/socket_runtime.hpp>
#include <st2110/contracts/settings.hpp>
#include <st2110/contracts/video/video_receice_pipeline_config.hpp>
#include <st2110/receive/shared/receive_bootstrap.hpp>
#include <st2110/receive/shared/receive_reorder_tolerance_policy.hpp>
#include <st2110/receive/shared/receive_start_request.hpp>
#include <st2110/receive/video/video_receive_bootstrap.hpp>
#include <st2110/delivery/socket_start_config.hpp>

namespace st2110 {
struct SocketVideoStreamConfig {
    std::uint8_t expected_payload_type = 0;

    VideoMediaDescription media{};
    VideoScanMode scan_mode = VideoScanMode::Progressive;
    VideoPackingMode packing_mode = VideoPackingMode::Gpm;
};

struct SocketVideoStartConfig : SocketStartConfig {
    SocketVideoStreamConfig stream{};
    VideoReceivePipelineConfig video_receive_pipeline_config{};
};

inline SocketVideoStartConfig project_receive_start_request_to_socket_video_start(const ReceiveStartRequest &request,
                                                                                  const Settings settings) {
    SocketVideoStartConfig res{};
    const auto &bootstrap = std::get<VideoReceiveBootstrap>(request.media);
    res.topology = bootstrap.receive_bootstrap.topology;
    res.stream =
        SocketVideoStreamConfig{.expected_payload_type = bootstrap.stream.receive_signaled_stream.expected_payload_type,
                                .media = bootstrap.stream.media,
                                .scan_mode = bootstrap.stream.scan_mode,
                                .packing_mode = bootstrap.stream.packing_mode};
    res.reorder_buffer_config = settings.reorder_buffer_config;
    res.video_receive_pipeline_config =
        make_video_receive_pipeline_config(res.stream.scan_mode, res.stream.media, settings.partial_unit_policy);

    for (std::size_t i = 0; i < bootstrap.receive_bootstrap.legs.size(); ++i) {
        SocketSourceFilter filter{.family = request.local.legs[i].family,
                                  .source_addresses =
                                      bootstrap.receive_bootstrap.legs[i].source_filter.source_addresses};
        const auto open_config =
            build_socket_rx_open_config(bootstrap.receive_bootstrap.legs[i].udp_port, request.local.legs[i].local_ip,
                                        bootstrap.receive_bootstrap.legs[i].destination.destination_address, filter);
        if (!open_config) {
            throw std::runtime_error("Can not build socket open config");
        }
        res.legs.emplace_back(request.local.legs[i].family, request.local.legs[i].local_ip,
                              bootstrap.receive_bootstrap.legs[i].destination.destination_address,
                              bootstrap.receive_bootstrap.legs[i].udp_port,
                              bootstrap.receive_bootstrap.legs[i].source_filter,
                              bootstrap.receive_bootstrap.legs[i].max_udp_datagram_bytes, *open_config);
    }

    return res;
}

} // namespace st2110

#endif // ST2110_OBS_SOCKET_VIDEO_START_CONFIG_HPP

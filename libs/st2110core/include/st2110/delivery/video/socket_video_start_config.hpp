#ifndef ST2110_OBS_SOCKET_VIDEO_START_CONFIG_HPP
#define ST2110_OBS_SOCKET_VIDEO_START_CONFIG_HPP

#include <st2110/contracts/settings.hpp>
#include <st2110/contracts/video/video_receice_pipeline_config.hpp>
#include <st2110/delivery/socket_start_config.hpp>
#include <st2110/receive/shared/receive_start_request.hpp>
#include <st2110/receive/video/video_receive_bootstrap.hpp>

#include <cstdint>
#include <variant>

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

[[nodiscard]] inline SocketVideoStreamConfig make_socket_video_stream_config(const VideoReceiveBootstrap &bootstrap) {
    return SocketVideoStreamConfig{
        .expected_payload_type = bootstrap.stream.receive_signaled_stream.expected_payload_type,
        .media = bootstrap.stream.media,
        .scan_mode = bootstrap.stream.scan_mode,
        .packing_mode = bootstrap.stream.packing_mode,
    };
}

inline SocketVideoStartConfig project_receive_start_request_to_socket_video_start(const ReceiveStartRequest &request,
                                                                                  const Settings settings) {
    SocketVideoStartConfig res{};

    const auto &bootstrap = std::get<VideoReceiveBootstrap>(request.media);

    res.topology = bootstrap.receive_bootstrap.topology;
    res.reorder_buffer_config = settings.reorder_buffer_config;
    res.stream = make_socket_video_stream_config(bootstrap);
    res.video_receive_pipeline_config =
        make_video_receive_pipeline_config(res.stream.scan_mode, res.stream.media, settings.partial_unit_policy);
    res.legs = make_socket_media_leg_configs(bootstrap.receive_bootstrap, request.local);

    return res;
}

} // namespace st2110

#endif // ST2110_OBS_SOCKET_VIDEO_START_CONFIG_HPP
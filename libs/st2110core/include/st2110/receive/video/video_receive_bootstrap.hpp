#ifndef ST2110_OBS_VIDEO_RECEIVE_BOOTSTRAP_HPP
#define ST2110_OBS_VIDEO_RECEIVE_BOOTSTRAP_HPP

#include <st2110/ingress/shared/parsed_sdp.hpp>
#include <st2110/receive/shared/receive_bootstrap.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace st2110 {
struct VideoReceiveSignaledStream {
    ReceiveSignaledStream receive_signaled_stream{};

    VideoMediaDescription media{};

    VideoScanMode scan_mode = VideoScanMode::Progressive;
    VideoPackingMode packing_mode = VideoPackingMode::Gpm;

    VideoSenderType sender_type = VideoSenderType::Narrow;
    std::optional<std::uint32_t> troff_us{};
    std::optional<std::uint32_t> cmax{};
};

struct VideoReceiveBootstrap {
    ReceiveBootstrap receive_bootstrap;
    VideoReceiveSignaledStream stream{};
};

inline VideoReceiveBootstrap project_parsed_video_sdp_to_receive_bootstrap(const ParsedSdpStreamSet &parsed) {
    return VideoReceiveBootstrap{
        .receive_bootstrap = project_video_receive_bootstrap(parsed),
        .stream = VideoReceiveSignaledStream{.receive_signaled_stream = project_receive_signaled_stream(
                                                 parsed.legs[0].expected_payload_type,
                                                 parsed.legs[0].video_stream_signaling->timing),
                                             .media = parsed.legs[0].video_stream_signaling->media,
                                             .scan_mode = parsed.legs[0].video_stream_signaling->scan_mode,
                                             .packing_mode = parsed.legs[0].video_stream_signaling->packing_mode,
                                             .sender_type = parsed.legs[0].video_stream_signaling->sender_type,
                                             .troff_us = parsed.legs[0].video_stream_signaling->troff_us,
                                             .cmax = parsed.legs[0].video_stream_signaling->cmax},
    };
}
} // namespace st2110

#endif // ST2110_OBS_VIDEO_RECEIVE_BOOTSTRAP_HPP

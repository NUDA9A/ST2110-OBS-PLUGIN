#ifndef ST2110_OBS_DEPACKETIZER_CONFIG_HPP
#define ST2110_OBS_DEPACKETIZER_CONFIG_HPP

#include <st2110/delivery/video/pixel_format.hpp>
#include <st2110/model/video/video_media_types.hpp>
#include <st2110/contracts/video/partial_unit_policy.hpp>

namespace st2110 {
enum class VideoAssemblyUnitKind { Frame, Field, Segment };

struct VideoReceiveCompletionPolicy {
    VideoAssemblyUnitKind unit_kind = VideoAssemblyUnitKind::Frame;
    bool marker_terminates_current_unit = false;
};

struct DepacketizerConfig {
    VideoReceiveCompletionPolicy video_receive_completion_policy{};
    VideoScanMode scan_mode = VideoScanMode::Progressive;
    PixelFormat format = PixelFormat::UYVY;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    PartialUnitPolicy policy = PartialUnitPolicy::EmitWithFlag;
};

[[nodiscard]] inline VideoReceiveCompletionPolicy video_receive_completion_policy(const VideoScanMode mode) noexcept {
    switch (mode) {
    case VideoScanMode::Progressive:
        return VideoReceiveCompletionPolicy{
            .unit_kind = VideoAssemblyUnitKind::Frame,
            .marker_terminates_current_unit = true,
        };

    case VideoScanMode::Interlaced:
        return VideoReceiveCompletionPolicy{
            .unit_kind = VideoAssemblyUnitKind::Field,
            .marker_terminates_current_unit = true,
        };

    case VideoScanMode::PsF:
        return VideoReceiveCompletionPolicy{
            .unit_kind = VideoAssemblyUnitKind::Segment,
            .marker_terminates_current_unit = false,
        };
    }

    return {};
}

[[nodiscard]] inline DepacketizerConfig make_depacketizer_config(const VideoScanMode mode, const PixelFormat format, const std::uint32_t width, const std::uint32_t height, const PartialUnitPolicy policy) {
    return DepacketizerConfig{.video_receive_completion_policy = video_receive_completion_policy(mode),
                              .scan_mode = mode, .format = format, .width = width, .height = height, .policy = policy};
}

} // namespace st2110

#endif // ST2110_OBS_DEPACKETIZER_CONFIG_HPP

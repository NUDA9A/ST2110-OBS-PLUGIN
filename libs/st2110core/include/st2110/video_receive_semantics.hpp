#ifndef ST2110_OBS_PLUGIN_VIDEO_RECEIVE_SEMANTICS_HPP
#define ST2110_OBS_PLUGIN_VIDEO_RECEIVE_SEMANTICS_HPP

#include <expected>

#include "error.hpp"
#include "video_scan_mode.hpp"

namespace st2110 {
    enum class VideoAssemblyUnitKind {
        Frame,
        Field,
        Segment
    };

    struct VideoReceiveCompletionPolicy {
        VideoAssemblyUnitKind unit_kind = VideoAssemblyUnitKind::Frame;
        bool marker_terminates_current_unit = false;
        bool timestamp_change_terminates_previous_unit = false;
    };

    struct VideoAssemblyKey {
        VideoAssemblyUnitKind unit_kind = VideoAssemblyUnitKind::Frame;
        uint32_t rtp_timestamp = 0;
        uint8_t sub_unit_index = 0;
    };

    [[nodiscard]] inline std::expected<VideoAssemblyUnitKind, Error>
    video_assembly_unit_kind(VideoScanMode mode) {
        switch (mode) {
            case VideoScanMode::Progressive:
                return VideoAssemblyUnitKind::Frame;
            case VideoScanMode::Interlaced:
                return VideoAssemblyUnitKind::Field;
            case VideoScanMode::PsF:
                return VideoAssemblyUnitKind::Segment;
            default:
                return std::unexpected(Error::InvalidValue);
        }
    }

    [[nodiscard]] inline std::expected<VideoReceiveCompletionPolicy, Error>
    video_receive_completion_policy(VideoScanMode mode) {
        switch (mode) {
            case VideoScanMode::Progressive:
                return VideoReceiveCompletionPolicy{
                        .unit_kind = VideoAssemblyUnitKind::Frame,
                        .marker_terminates_current_unit = true,
                        .timestamp_change_terminates_previous_unit = true
                };
            case VideoScanMode::Interlaced:
            case VideoScanMode::PsF:
                return std::unexpected(Error::Unsupported);
            default:
                return std::unexpected(Error::InvalidValue);
        }
    }

    [[nodiscard]] inline std::expected<VideoAssemblyKey, Error>
    video_packet_assembly_key(VideoScanMode mode, const PacketView& packet);

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_VIDEO_RECEIVE_SEMANTICS_HPP
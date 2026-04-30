#ifndef ST2110_OBS_PLUGIN_VIDEO_RECEIVE_SEMANTICS_HPP
#define ST2110_OBS_PLUGIN_VIDEO_RECEIVE_SEMANTICS_HPP

#include <expected>

#include "error.hpp"
#include "video_scan_mode.hpp"
#include "packet_view.hpp"

namespace st2110 {
enum class VideoAssemblyUnitKind { Frame, Field, Segment };

struct VideoReceiveCompletionPolicy {
    VideoAssemblyUnitKind unit_kind = VideoAssemblyUnitKind::Frame;
    bool marker_terminates_current_unit = false;
    bool key_change_terminates_previous_unit = false;
};

struct VideoAssemblyKey {
    VideoAssemblyUnitKind unit_kind = VideoAssemblyUnitKind::Frame;
    uint32_t rtp_timestamp = 0;
    uint8_t sub_unit_index = 0;

    bool operator==(const VideoAssemblyKey &) const = default;
};

[[nodiscard]] inline std::expected<VideoAssemblyUnitKind, Error> video_assembly_unit_kind(VideoScanMode mode) {
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
        return VideoReceiveCompletionPolicy{.unit_kind = VideoAssemblyUnitKind::Frame,
                                            .marker_terminates_current_unit = true,
                                            .key_change_terminates_previous_unit = true};
    case VideoScanMode::Interlaced:
    case VideoScanMode::PsF:
        return std::unexpected(Error::Unsupported);
    default:
        return std::unexpected(Error::InvalidValue);
    }
}

[[nodiscard]] inline std::expected<VideoAssemblyKey, Error> video_packet_assembly_key(VideoScanMode mode,
                                                                                      const PacketView &packet) {
    switch (mode) {
    case VideoScanMode::Progressive:
        return VideoAssemblyKey{
            .unit_kind = VideoAssemblyUnitKind::Frame, .rtp_timestamp = packet.rtp.timestamp, .sub_unit_index = 0};
    case VideoScanMode::Interlaced:
    case VideoScanMode::PsF:
        return std::unexpected(Error::Unsupported);
    default:
        return std::unexpected(Error::InvalidValue);
    }
}

[[nodiscard]] inline bool same_video_assembly_key(const VideoAssemblyKey &a, const VideoAssemblyKey &b) {
    return a == b;
}

[[nodiscard]] inline Error validate_video_packet_scan_mode_semantics(VideoScanMode scan_mode,
                                                                     const PacketView &packet) {
    switch (scan_mode) {
    case VideoScanMode::Progressive:
        for (std::size_t i = 0; i < packet.segment_count; ++i) {
            if (packet.segments[i].header.field_id) {
                return Error::InvalidValue;
            }
        }
        return Error::Ok;
    case VideoScanMode::Interlaced:
    case VideoScanMode::PsF:
        return Error::Ok;
    default:
        return Error::InvalidValue;
    }
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_VIDEO_RECEIVE_SEMANTICS_HPP
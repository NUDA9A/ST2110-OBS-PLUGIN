#ifndef ST2110_OBS_PLUGIN_FRAME_ASSEMBLER_HPP
#define ST2110_OBS_PLUGIN_FRAME_ASSEMBLER_HPP

#include <st2110/contracts/video/depacketizer_config.hpp>
#include <st2110/contracts/video/partial_unit_policy.hpp>
#include <st2110/delivery/video/video_frame.hpp>
#include <st2110/foundation/bytes.hpp>
#include <st2110/foundation/timestamp.hpp>
#include <st2110/receive/video/frame_write_coverage.hpp>

#include <cstdint>
#include <cstring>
#include <optional>
#include <utility>

namespace st2110 {
enum class FrameAssemblerEndStatus { NotEmittable, EmittedComplete, EmittedPartial, DroppedPartial };

struct AssembledVideoUnit {
    VideoFrame frame;
    VideoAssemblyUnitKind unit_kind = VideoAssemblyUnitKind::Frame;
    std::uint32_t rtp_timestamp = 0;
    TimestampNs receive_timestamp_ns = 0;
    std::uint8_t sub_unit_index = 0;
    bool marker_seen = false;
    bool can_emit = false;
    bool complete = false;

    [[nodiscard]] bool partial() const { return can_emit && !complete; }
};

struct FrameAssemblerEndResult {
    std::optional<AssembledVideoUnit> unit{};
    FrameAssemblerEndStatus status = FrameAssemblerEndStatus::NotEmittable;
};

class FrameAssembler {
  public:
    FrameAssembler(const std::uint32_t width, const std::uint32_t height, const PixelFormat format,
                   const PartialUnitPolicy partial_policy)
        : width_(width), height_(height), format_(format), current_frame_(width_, height_, format_),
          coverage_(current_frame_), partial_policy_(partial_policy) {}

    void begin(std::uint32_t rtp_timestamp, TimestampNs receive_timestamp_ns) {
        current_rtp_timestamp_ = rtp_timestamp;
        current_receive_timestamp_ns_ = receive_timestamp_ns;
        coverage_.reset_from(current_frame_);
    }

    void write_segment(std::size_t plane, std::uint32_t row, std::size_t byte_offset, ByteSpan bytes) {
        auto *start = current_frame_.row_data(row, plane);
        start += byte_offset;
        std::memcpy(start, bytes.data(), bytes.size());

        coverage_.mark_written(plane, row, byte_offset, bytes.size());
    }

    [[nodiscard]] FrameAssemblerEndResult end(bool marker, bool can_emit) {
        if (!can_emit) {
            current_frame_ = VideoFrame(width_, height_, format_);
            return {std::nullopt, FrameAssemblerEndStatus::NotEmittable};
        }

        const bool fully_written = coverage_.is_complete();
        if (!fully_written && partial_policy_ == PartialUnitPolicy::Drop) {
            current_frame_ = VideoFrame(width_, height_, format_);
            return {std::nullopt, FrameAssemblerEndStatus::DroppedPartial};
        }

        AssembledVideoUnit res{.frame = std::move(current_frame_),
                               .rtp_timestamp = current_rtp_timestamp_,
                               .receive_timestamp_ns = current_receive_timestamp_ns_,
                               .marker_seen = marker,
                               .can_emit = true,
                               .complete = fully_written};
        current_frame_ = VideoFrame(width_, height_, format_);
        if (fully_written) {
            return {std::move(res), FrameAssemblerEndStatus::EmittedComplete};
        }

        return {std::move(res), FrameAssemblerEndStatus::EmittedPartial};
    }

  private:
    std::uint32_t width_;
    std::uint32_t height_;
    PixelFormat format_;

    std::uint32_t current_rtp_timestamp_ = 0;
    TimestampNs current_receive_timestamp_ns_ = 0;
    VideoFrame current_frame_;
    FrameWriteCoverage coverage_;
    PartialUnitPolicy partial_policy_;
};
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_FRAME_ASSEMBLER_HPP

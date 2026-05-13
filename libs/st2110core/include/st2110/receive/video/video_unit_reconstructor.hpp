#ifndef ST2110_OBS_PLUGIN_VIDEO_UNIT_RECONSTRUCTOR_HPP
#define ST2110_OBS_PLUGIN_VIDEO_UNIT_RECONSTRUCTOR_HPP

#include <st2110/contracts/video/depacketizer_config.hpp>
#include <st2110/contracts/video/video_unit_reconstructor_config.hpp>
#include <st2110/delivery/video/video_frame.hpp>
#include <st2110/foundation/timestamp.hpp>
#include <st2110/receive/video/frame_assembler.hpp>

#include <cstring>
#include <memory>
#include <optional>

namespace st2110 {
struct ReconstructedVideoFrame {
    VideoFrame frame;
    std::uint32_t rtp_timestamp = 0;
    TimestampNs receive_timestamp_ns = 0;
    bool complete = false;

    [[nodiscard]] bool partial() const { return !complete; }
};

struct PendingUnitPair {
    std::optional<AssembledVideoUnit> first{};
    std::optional<AssembledVideoUnit> second{};
};

class VideoUnitReconstructor {
  public:
    explicit VideoUnitReconstructor(const VideoUnitReconstructorConfig cfg) : cfg_(cfg) {}
    std::optional<ReconstructedVideoFrame> push(AssembledVideoUnit unit) {
        switch (unit.unit_kind) {
        case VideoAssemblyUnitKind::Frame:
            return ReconstructedVideoFrame{.frame = std::move(unit.frame),
                                           .rtp_timestamp = unit.rtp_timestamp,
                                           .receive_timestamp_ns = unit.receive_timestamp_ns,
                                           .complete = unit.complete};
        case VideoAssemblyUnitKind::Field:
            if (unit.sub_unit_index == 0) {
                unit_pair_.first = std::move(unit);
            } else {
                unit_pair_.second = std::move(unit);
            }

            if (unit_pair_.first && unit_pair_.second) {
                VideoFrame frame(cfg_.width, cfg_.height, cfg_.format);
                copy_sub_unit_rows(frame, unit_pair_.first->frame, 0);
                copy_sub_unit_rows(frame, unit_pair_.second->frame, 1);
                ReconstructedVideoFrame res{.frame = std::move(frame),
                                            .rtp_timestamp = unit_pair_.first->rtp_timestamp,
                                            .receive_timestamp_ns = unit_pair_.first->receive_timestamp_ns,
                                            .complete = unit_pair_.first->complete && unit_pair_.second->complete};
                unit_pair_ = {};
                return res;
            }
            return std::nullopt;
        case VideoAssemblyUnitKind::Segment:
            if (unit.sub_unit_index == 0) {
                if (unit_pair_.second && unit_pair_.second->rtp_timestamp != unit.rtp_timestamp) {
                    unit_pair_.second = {};
                }
                unit_pair_.first = std::move(unit);
            } else {
                if (unit_pair_.first && unit_pair_.first->rtp_timestamp != unit.rtp_timestamp) {
                    unit_pair_.first = {};
                }
                unit_pair_.second = std::move(unit);
            }

            if (unit_pair_.first && unit_pair_.second) {
                VideoFrame frame(cfg_.width, cfg_.height, cfg_.format);
                copy_sub_unit_rows(frame, unit_pair_.first->frame, 0);
                copy_sub_unit_rows(frame, unit_pair_.second->frame, 1);
                ReconstructedVideoFrame res{.frame = std::move(frame),
                                            .rtp_timestamp = unit_pair_.first->rtp_timestamp,
                                            .receive_timestamp_ns = unit_pair_.first->receive_timestamp_ns,
                                            .complete = unit_pair_.first->complete && unit_pair_.second->complete};
                unit_pair_ = {};
                return res;
            }
            return std::nullopt;
        default:
            std::unreachable();
        }
    }

  private:
    static void copy_sub_unit_rows(VideoFrame &dst, const VideoFrame &src, std::uint8_t sub_unit_index) {
        for (std::size_t plane = 0; plane < dst.plane_count(); ++plane) {
            const std::size_t row_bytes = dst.active_row_bytes(plane);

            for (std::uint32_t src_row = 0; src_row < src.plane_height_rows(plane); ++src_row) {
                const std::uint32_t dst_row = src_row * 2 + sub_unit_index;

                std::memcpy(dst.row_data(dst_row, plane), src.row_data(src_row, plane), row_bytes);
            }
        }
    }

    VideoUnitReconstructorConfig cfg_;
    PendingUnitPair unit_pair_{};
};
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_VIDEO_UNIT_RECONSTRUCTOR_HPP

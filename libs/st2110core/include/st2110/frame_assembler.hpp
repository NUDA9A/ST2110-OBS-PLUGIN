#ifndef ST2110_OBS_PLUGIN_FRAME_ASSEMBLER_HPP
#define ST2110_OBS_PLUGIN_FRAME_ASSEMBLER_HPP

#include "video_frame.hpp"
#include "bytes.hpp"
#include "frame_write_coverage.hpp"
#include "video_receive_semantics.hpp"

#include <cstring>
#include <cstdint>
#include <utility>
#include <stdexcept>
#include <optional>

namespace st2110 {
    enum class PartialFramePolicy {
        EmitWithFlag,
        Drop
    };

    enum class FrameAssemblerEndStatus {
        NotEmittable,
        EmittedComplete,
        EmittedPartial,
        DroppedPartial
    };

    struct AssembledVideoUnit {
        VideoFrame frame;
        VideoAssemblyUnitKind unit_kind = VideoAssemblyUnitKind::Frame;
        uint32_t rtp_timestamp = 0;
        bool marker_seen = false;
        bool can_emit = false;
        bool complete = false;

        [[nodiscard]] bool partial() const {
            return can_emit && !complete;
        }
    };

    struct FrameAssemblerEndResult {
        std::optional<AssembledVideoUnit> unit{};
        FrameAssemblerEndStatus status = FrameAssemblerEndStatus::NotEmittable;
    };

    class FrameAssembler {
    public:
        FrameAssembler(uint32_t width, uint32_t height, PixelFormat format, PartialFramePolicy partial_policy = PartialFramePolicy::EmitWithFlag) : width_(width), height_(height), format_(format),
                                                                              current_frame_(width_, height_, format_), coverage_(current_frame_), partial_policy_(partial_policy) {}

        void begin(uint32_t rtp_timestamp) {
            if (in_progress_) {
                throw std::logic_error("FrameAssembler is currently in progress and begin() was called");
            }
            current_rtp_timestamp_ = rtp_timestamp;
            in_progress_ = true;
            coverage_.reset_from(current_frame_);
        }

        void write_segment(std::size_t plane, uint32_t row, std::size_t byte_offset, ByteSpan bytes) {
            if (!in_progress_) {
                throw std::logic_error("begin() wasn't called before write_segment()");
            }

            const auto active_row_bytes = current_frame_.active_row_bytes(plane);

            if (byte_offset > active_row_bytes) {
                throw std::out_of_range("byte_offset > active_row_bytes");
            }
            if (bytes.size() > active_row_bytes - byte_offset) {
                throw std::out_of_range("bytes.size() + byte_offset > active_row_bytes");
            }

            auto* start = current_frame_.row_data(row, plane);
            start += byte_offset;
            std::memcpy(start, bytes.data(), bytes.size());

            coverage_.mark_written(plane, row, byte_offset, bytes.size());
        }

        [[nodiscard]] FrameAssemblerEndResult end(bool marker) {
            if (!in_progress_) {
                throw std::logic_error("end() was called while Assembler is not in progress");
            }
            in_progress_ = false;

            const bool fully_written = coverage_.is_complete();
            if (!marker) {
                current_frame_ = VideoFrame(width_, height_, format_);
                return {std::nullopt, FrameAssemblerEndStatus::NotEmittable};
            }
            if (!fully_written && partial_policy_ == PartialFramePolicy::Drop) {
                current_frame_ = VideoFrame(width_, height_, format_);
                return {std::nullopt, FrameAssemblerEndStatus::DroppedPartial};
            }
            AssembledVideoUnit res{
                    .frame = std::move(current_frame_),
                    .rtp_timestamp = current_rtp_timestamp_,
                    .marker_seen = marker,
                    .can_emit = marker,
                    .complete = marker && fully_written
            };
            current_frame_ = VideoFrame(width_, height_, format_);
            if (fully_written) {
                return {std::move(res), FrameAssemblerEndStatus::EmittedComplete};
            } else {
                return {std::move(res), FrameAssemblerEndStatus::EmittedPartial};
            }
        }

        [[nodiscard]] bool in_progress() const {
            return in_progress_;
        }

        [[nodiscard]] uint32_t current_rtp_timestamp() const {
            if (!in_progress_) {
                throw std::logic_error("Assembler is not currently in progress");
            }
            return current_rtp_timestamp_;
        }

        [[nodiscard]] PartialFramePolicy partial_frame_policy() const {
            return partial_policy_;
        }

    private:
        uint32_t width_;
        uint32_t height_;
        PixelFormat format_;

        bool in_progress_ = false;
        uint32_t current_rtp_timestamp_ = 0;
        VideoFrame current_frame_;
        FrameWriteCoverage coverage_;
        PartialFramePolicy partial_policy_;
    };
}

#endif //ST2110_OBS_PLUGIN_FRAME_ASSEMBLER_HPP

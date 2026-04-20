#ifndef ST2110_OBS_PLUGIN_DEPACKETIZER_HPP
#define ST2110_OBS_PLUGIN_DEPACKETIZER_HPP

#include "packet_view.hpp"
#include "frame_assembler.hpp"
#include "stats.hpp"

#include <cstdint>
#include <optional>
#include <vector>
#include <stdexcept>
#include <utility>

namespace st2110 {

    struct DepacketizerConfig {
        uint32_t width = 0;
        uint32_t height = 0;
        PixelFormat format = PixelFormat::UYVY;
        PartialFramePolicy partial_frame_policy = PartialFramePolicy::EmitWithFlag;
    };

    struct FrameWriteOp {
        std::size_t plane = 0;
        uint32_t row = 0;
        std::size_t byte_offset = 0;
        ByteSpan bytes{};
    };

    class Depacketizer {
    public:
        explicit Depacketizer(const DepacketizerConfig& cfg) : cfg_(cfg), assembler_(cfg_.width, cfg_.height, cfg_.format, cfg_.partial_frame_policy) {}

        [[nodiscard]] std::vector<AssembledVideoFrame> push(const PacketView& packet) {
            ++stats_.packets_in;
            std::vector<AssembledVideoFrame> res;

            if (!has_current_timestamp_) {
                begin_frame(packet.rtp.timestamp);
                write_packet_segments(packet);
                ++stats_.packets_used;
                if (packet.rtp.marker) {
                    auto end_res = assembler_.end(true);
                    handle_end_result(std::move(end_res), res);
                }
                return res;
            }

            if (current_rtp_timestamp_ == packet.rtp.timestamp) {
                write_packet_segments(packet);
                ++stats_.packets_used;
                if (packet.rtp.marker) {
                    auto end_res = assembler_.end(true);
                    handle_end_result(std::move(end_res), res);
                }
                return res;
            }

            auto end_res = assembler_.end(false);
            handle_end_result(std::move(end_res), res);
            begin_frame(packet.rtp.timestamp);
            write_packet_segments(packet);
            ++stats_.packets_used;
            if (packet.rtp.marker) {
                auto end_res = assembler_.end(true);
                handle_end_result(std::move(end_res), res);
            }
            return res;
        }

        void reset() {
            assembler_ = FrameAssembler(cfg_.width, cfg_.height, cfg_.format, cfg_.partial_frame_policy);
            has_current_timestamp_ = false;
            current_rtp_timestamp_ = 0;
            stats_ = {};
        }

        [[nodiscard]] bool has_frame_in_progress() const {
            return assembler_.in_progress();
        }

        [[nodiscard]] std::optional<uint32_t> current_rtp_timestamp() const {
            if (has_current_timestamp_) {
                return current_rtp_timestamp_;
            }
            return std::nullopt;
        }

        [[nodiscard]] const DepacketizerStats& stats() const {
            return stats_;
        }

    private:
        void begin_frame(uint32_t rtp_timestamp) {
            assembler_.begin(rtp_timestamp);
            has_current_timestamp_ = true;
            current_rtp_timestamp_ = rtp_timestamp;
        }

        void handle_end_result(FrameAssemblerEndResult&& end_res,
                               std::vector<AssembledVideoFrame>& out) {
            switch (end_res.status) {
                case FrameAssemblerEndStatus::EmittedComplete:
                    ++stats_.frames_ok;
                    break;
                case FrameAssemblerEndStatus::EmittedPartial:
                    ++stats_.frames_partial;
                    break;
                case FrameAssemblerEndStatus::DroppedPartial:
                case FrameAssemblerEndStatus::NotEmittable:
                    ++stats_.frames_dropped;
                    break;
            }

            if (end_res.frame.has_value()) {
                out.push_back(std::move(*end_res.frame));
            }

            has_current_timestamp_ = false;
            current_rtp_timestamp_ = 0;
        }

        void write_packet_segments(const PacketView& packet) {
            for (std::size_t i = 0; i < packet.segment_count; ++i) {
                FrameWriteOp op = map_segment_to_frame_write(cfg_.format, packet.segments[i]);
                assembler_.write_segment(op.plane, op.row, op.byte_offset, op.bytes);
            }
        }

        [[nodiscard]] static FrameWriteOp map_segment_to_frame_write(
                PixelFormat format,
                const SrdSegmentView& segment) {
            switch (format) {
                case PixelFormat::UYVY:
                    return {
                        .plane = 0,
                        .row = segment.header.row_number,
                        .byte_offset = segment.header.offset,
                        .bytes = segment.data
                    };
                default:
                    throw std::logic_error("Current format is not supported yet");
            }
        }

        DepacketizerConfig cfg_;
        FrameAssembler assembler_;
        bool has_current_timestamp_ = false;
        uint32_t current_rtp_timestamp_ = 0;
        DepacketizerStats stats_{};
    };

}

#endif //ST2110_OBS_PLUGIN_DEPACKETIZER_HPP
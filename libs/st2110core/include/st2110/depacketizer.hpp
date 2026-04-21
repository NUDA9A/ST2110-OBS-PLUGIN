#ifndef ST2110_OBS_PLUGIN_DEPACKETIZER_HPP
#define ST2110_OBS_PLUGIN_DEPACKETIZER_HPP

#include "packet_view.hpp"
#include "frame_assembler.hpp"
#include "stats.hpp"
#include "video_scan_mode.hpp"
#include "video_receive_semantics.hpp"

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
        VideoScanMode scan_mode = VideoScanMode::Progressive;
    };

    struct FrameWriteOp {
        std::size_t plane = 0;
        uint32_t row = 0;
        std::size_t byte_offset = 0;
        ByteSpan bytes{};
    };

    struct DepacketizerAssemblyState {
        bool in_progress = false;
        uint32_t rtp_timestamp = 0;
        VideoAssemblyUnitKind unit_kind = VideoAssemblyUnitKind::Frame;
    };

    class Depacketizer {
    public:
        explicit Depacketizer(const DepacketizerConfig& cfg) : cfg_(cfg), assembler_(cfg_.width, cfg_.height, cfg_.format, cfg_.partial_frame_policy) {}

        [[nodiscard]] std::vector<AssembledVideoUnit> push(const PacketView& packet) {
            ++stats_.packets_in;
            const auto policy = configured_completion_policy();
            std::vector<AssembledVideoUnit> res;

            if (!has_unit_in_progress()) {
                begin_unit(packet.rtp.timestamp);
                write_packet_segments(packet);
                ++stats_.packets_used;
                if (packet.rtp.marker && policy.marker_terminates_current_unit) {
                    auto end_res = assembler_.end(true);
                    handle_end_result(std::move(end_res), res);
                }
                return res;
            }

            if (current_unit_rtp_timestamp() == packet.rtp.timestamp) {
                write_packet_segments(packet);
                ++stats_.packets_used;
                if (packet.rtp.marker && policy.marker_terminates_current_unit) {
                    auto end_res = assembler_.end(true);
                    handle_end_result(std::move(end_res), res);
                }
                return res;
            }

            if (policy.timestamp_change_terminates_previous_unit) {
                auto end_res = assembler_.end(false);
                handle_end_result(std::move(end_res), res);
            }
            begin_unit(packet.rtp.timestamp);
            write_packet_segments(packet);
            ++stats_.packets_used;
            if (packet.rtp.marker && policy.marker_terminates_current_unit) {
                auto end_res = assembler_.end(true);
                handle_end_result(std::move(end_res), res);
            }
            return res;
        }

        void reset() {
            assembler_ = FrameAssembler(cfg_.width, cfg_.height, cfg_.format, cfg_.partial_frame_policy);
            assembly_state_ = {};
            stats_ = {};
        }

        [[nodiscard]] const DepacketizerStats& stats() const {
            return stats_;
        }

        [[nodiscard]] VideoScanMode scan_mode() const {
            return cfg_.scan_mode;
        }

        [[nodiscard]] bool has_unit_in_progress() const {
            return assembly_state_.in_progress;
        }

        [[nodiscard]] std::optional<uint32_t> current_unit_rtp_timestamp() const {
            if (!assembly_state_.in_progress) {
                return std::nullopt;
            }
            return assembly_state_.rtp_timestamp;
        }

        [[nodiscard]] VideoAssemblyUnitKind assembly_unit_kind() const {
            auto kind = video_assembly_unit_kind(cfg_.scan_mode);
            if (!kind.has_value()) {
                throw std::logic_error("Invalid scan mode for depacketizer assembly unit kind");
            }
            return *kind;
        }

    private:
        [[nodiscard]] VideoReceiveCompletionPolicy configured_completion_policy() const {
            // MVP runtime currently implements only Progressive completion behavior.
            // Interlaced and PsF are modeled through VideoScanMode / VideoAssemblyUnitKind
            // but remain rejected here until their mode-specific policies are implemented.
            auto policy = video_receive_completion_policy(cfg_.scan_mode);
            if (!policy.has_value()) {
                throw std::logic_error("Current scan mode is not implemented in depacketizer yet");
            }
            return *policy;
        }

        void begin_unit(uint32_t rtp_timestamp) {
            assembler_.begin(rtp_timestamp);
            assembly_state_.in_progress = true;
            assembly_state_.rtp_timestamp = rtp_timestamp;
            assembly_state_.unit_kind = assembly_unit_kind();
        }

        void handle_end_result(FrameAssemblerEndResult&& end_res,
                               std::vector<AssembledVideoUnit>& out) {
            switch (end_res.status) {
                case FrameAssemblerEndStatus::EmittedComplete:
                    ++stats_.units_ok;
                    break;
                case FrameAssemblerEndStatus::EmittedPartial:
                    ++stats_.units_partial;
                    break;
                case FrameAssemblerEndStatus::DroppedPartial:
                case FrameAssemblerEndStatus::NotEmittable:
                    ++stats_.units_dropped;
                    break;
            }

            if (end_res.unit.has_value()) {
                end_res.unit->unit_kind = assembly_state_.unit_kind;
                out.push_back(std::move(*end_res.unit));
            }

            assembly_state_ = {};
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
        DepacketizerAssemblyState assembly_state_{};
        DepacketizerStats stats_{};
    };

}

#endif //ST2110_OBS_PLUGIN_DEPACKETIZER_HPP
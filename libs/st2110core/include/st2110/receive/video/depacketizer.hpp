#ifndef ST2110_OBS_PLUGIN_DEPACKETIZER_HPP
#define ST2110_OBS_PLUGIN_DEPACKETIZER_HPP

#include "st2110/ingress/shared/packet_view.hpp"
#include "st2110/model/video/video_packing_mode.hpp"
#include "st2110/model/video/video_scan_mode.hpp"
#include "depacketizer_stats.hpp"
#include "frame_assembler.hpp"
#include "video_packet_padding.hpp"
#include "video_receive_semantics.hpp"
#include "video_segment_placement.hpp"

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace st2110 {

struct DepacketizerConfig {
    uint32_t width = 0;
    uint32_t height = 0;
    PixelFormat format = PixelFormat::UYVY;
    PartialFramePolicy partial_frame_policy = PartialFramePolicy::EmitWithFlag;
    VideoScanMode scan_mode = VideoScanMode::Progressive;
    VideoPackingMode packing_mode = VideoPackingMode::Gpm;
};

struct DepacketizerAssemblyState {
    std::optional<VideoAssemblyKey> current_key{};
    std::optional<VideoAssemblyCursor> write_cursor{};
};

class Depacketizer {
  public:
    explicit Depacketizer(const DepacketizerConfig &cfg)
        : cfg_(cfg), assembler_(cfg_.width, cfg_.height, cfg_.format, cfg_.partial_frame_policy) {}

    [[nodiscard]] std::vector<AssembledVideoUnit> push(const PacketView &packet) {
        ++stats_.packets_in;
        if (Error err = validate_video_packet_scan_mode_semantics(cfg_.scan_mode, packet); err != Error::Ok) {
            throw std::invalid_argument("Invalid packet scan-mode semantics for current video scan mode");
        }

        const auto policy = configured_completion_policy();
        const auto packet_key_expected = video_packet_assembly_key(cfg_.scan_mode, packet);
        if (!packet_key_expected.has_value()) {
            throw std::logic_error("Current scan mode packet-to-unit grouping is not implemented yet");
        }
        const VideoAssemblyKey &packet_key = *packet_key_expected;

        if (packet_key.unit_kind != policy.unit_kind) {
            throw std::logic_error("Inconsistent video receive semantics: packet assembly key unit kind does not match "
                                   "completion policy unit kind");
        }

        std::vector<AssembledVideoUnit> res;

        if (!has_unit_in_progress()) {
            validate_padding(packet);
            begin_unit(packet_key);
            write_packet_segments(packet);
            ++stats_.packets_used;

            if (packet.rtp.marker && policy.marker_terminates_current_unit) {
                auto end_res = assembler_.end(true);
                handle_end_result(std::move(end_res), res);
            }
            return res;
        }

        if (same_video_assembly_key(*assembly_state_.current_key, packet_key)) {
            validate_padding(packet);
            write_packet_segments(packet);
            ++stats_.packets_used;

            if (packet.rtp.marker && policy.marker_terminates_current_unit) {
                auto end_res = assembler_.end(true);
                handle_end_result(std::move(end_res), res);
            }
            return res;
        }

        if (!policy.key_change_terminates_previous_unit) {
            throw std::logic_error("Current completion policy does not support assembly-key transition yet");
        }

        validate_padding(packet);

        {
            auto end_res = assembler_.end(false);
            handle_end_result(std::move(end_res), res);
        }

        begin_unit(packet_key);
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

    [[nodiscard]] const DepacketizerStats &stats() const { return stats_; }

    [[nodiscard]] VideoScanMode scan_mode() const { return cfg_.scan_mode; }

    [[nodiscard]] bool has_unit_in_progress() const { return assembly_state_.current_key.has_value(); }

    [[nodiscard]] std::optional<uint32_t> current_unit_rtp_timestamp() const {
        if (!has_unit_in_progress()) {
            return std::nullopt;
        }
        return assembly_state_.current_key->rtp_timestamp;
    }

    [[nodiscard]] VideoAssemblyUnitKind assembly_unit_kind() const {
        auto kind = video_assembly_unit_kind(cfg_.scan_mode);
        if (!kind.has_value()) {
            throw std::logic_error("Invalid scan mode for depacketizer assembly unit kind");
        }
        return *kind;
    }

    [[nodiscard]] std::optional<VideoAssemblyKey> current_unit_key() const { return assembly_state_.current_key; }

  private:
    void validate_padding(const PacketView &packet) {
        Error err = validate_video_packet_trailing_padding(cfg_.packing_mode, cfg_.scan_mode, packet);
        if (err == Error::InvalidValue) {
            throw std::invalid_argument("Invalid trailing payload padding for current video scan mode");
        }
        if (err == Error::Unsupported) {
            throw std::logic_error(
                "Trailing payload padding validation is not implemented for current video scan mode");
        }
    }

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

    void begin_unit(const VideoAssemblyKey &key) {
        assembler_.begin(key.rtp_timestamp);
        assembly_state_.current_key = key;
        assembly_state_.write_cursor.reset();
    }

    void handle_end_result(FrameAssemblerEndResult &&end_res, std::vector<AssembledVideoUnit> &out) {
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
            if (assembly_state_.current_key.has_value()) {
                end_res.unit->unit_kind = assembly_state_.current_key->unit_kind;
            }
            out.push_back(std::move(*end_res.unit));
        }

        assembly_state_ = {};
    }

    void write_packet_segments(const PacketView &packet) {
        std::vector<VideoFrameWriteOp> write_ops{};
        write_ops.reserve(packet.segment_count);

        auto cursor = assembly_state_.write_cursor;

        for (std::size_t i = 0; i < packet.segment_count; ++i) {
            auto expected_op =
                map_video_segment_to_frame_write(cfg_.packing_mode, cfg_.format, cfg_.scan_mode, packet.segments[i]);
            if (!expected_op.has_value()) {
                Error err = expected_op.error();
                if (err == Error::InvalidValue) {
                    throw std::invalid_argument("Invalid video segment placement for current format/scan mode");
                }
                throw std::logic_error("Unsupported video segment placement for current format/scan mode");
            }

            if (Error err =
                    validate_and_advance_video_assembly_cursor(cfg_.scan_mode, cursor, packet.segments[i], *expected_op);
                err != Error::Ok) {
                if (err == Error::InvalidValue) {
                    throw std::invalid_argument("Invalid cross-packet video segment order for current assembly unit");
                }
                throw std::logic_error("Unsupported cross-packet video segment order for current video scan mode");
                }

            write_ops.push_back(*expected_op);
        }

        for (const auto &op : write_ops) {
            assembler_.write_segment(op.plane, op.row, op.byte_offset, op.bytes);
        }

        assembly_state_.write_cursor = std::move(cursor);
    }

    DepacketizerConfig cfg_;
    FrameAssembler assembler_;
    DepacketizerAssemblyState assembly_state_{};
    DepacketizerStats stats_{};
};

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_DEPACKETIZER_HPP
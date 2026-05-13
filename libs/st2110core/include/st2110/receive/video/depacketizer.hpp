#ifndef ST2110_OBS_PLUGIN_DEPACKETIZER_HPP
#define ST2110_OBS_PLUGIN_DEPACKETIZER_HPP

#include <st2110/contracts/video/depacketizer_config.hpp>
#include <st2110/receive/video/video_packet_view.hpp>
#include <st2110/receive/video/depacketizer_stats.hpp>
#include <st2110/receive/video/frame_assembler.hpp>

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace st2110 {
struct VideoFrameWriteOp {
    std::size_t plane = 0;
    std::uint32_t row = 0;
    std::size_t byte_offset = 0;
    ByteSpan bytes{};
};

struct VideoAssemblyKey {
    VideoAssemblyUnitKind unit_kind = VideoAssemblyUnitKind::Frame;
    std::uint32_t rtp_timestamp = 0;
    std::uint8_t sub_unit_index = 0;

    bool operator==(const VideoAssemblyKey &) const = default;
};

struct DepacketizerAssemblyState {
    std::optional<VideoAssemblyKey> current_key{};
};

class Depacketizer {
  public:
    explicit Depacketizer(const DepacketizerConfig &cfg)
        : cfg_(cfg), assembler_(cfg_.width, cfg_.height, cfg_.format, cfg_.policy) {}

    [[nodiscard]] std::vector<AssembledVideoUnit> push(std::unique_ptr<VideoPacketView> packet) {
        const VideoPacketView &pkt = *packet;

        ++stats_.packets_in;

        const auto packet_key = video_packet_assembly_key(cfg_.scan_mode, pkt);

        std::vector<AssembledVideoUnit> res;

        if (!has_unit_in_progress()) {
            begin_unit(packet_key);
            write_packet_segments(pkt);
            ++stats_.packets_used;

            if (pkt.rtp.marker && cfg_.video_receive_completion_policy.marker_terminates_current_unit) {
                end_current_unit(true, res);
            }
            return res;
        }

        if (*assembly_state_.current_key != packet_key) {
            end_current_unit(false, res);
            begin_unit(packet_key);
        }

        write_packet_segments(pkt);
        ++stats_.packets_used;

        if (pkt.rtp.marker && cfg_.video_receive_completion_policy.marker_terminates_current_unit) {
            end_current_unit(true, res);
        }

        return res;
    }

    void reset() {
        assembler_ = FrameAssembler(cfg_.width, cfg_.height, cfg_.format, cfg_.policy);
        assembly_state_ = {};
        stats_ = {};
    }

    [[nodiscard]] const DepacketizerStats &stats() const { return stats_; }

    [[nodiscard]] bool has_unit_in_progress() const { return assembly_state_.current_key.has_value(); }

  private:
    void end_current_unit(bool marker_seen, std::vector<AssembledVideoUnit> &out) {
        auto end_res = assembler_.end(marker_seen, true);
        handle_end_result(std::move(end_res), out);
    }

    [[nodiscard]] static std::uint32_t unit_height_for_key(VideoScanMode scan_mode, std::uint32_t full_height,
                                                           std::uint8_t sub_unit_index) noexcept {
        switch (scan_mode) {
        case VideoScanMode::Progressive:
            return full_height;
        case VideoScanMode::Interlaced:
        case VideoScanMode::PsF:
            return (sub_unit_index == 0) ? ((full_height + 1) / 2) : (full_height / 2);
        }

        return full_height;
    }

    [[nodiscard]] static VideoAssemblyKey video_packet_assembly_key(VideoScanMode mode,
                                                                    const VideoPacketView &packet) noexcept {
        switch (mode) {
        case VideoScanMode::Progressive:
            return VideoAssemblyKey{
                .unit_kind = VideoAssemblyUnitKind::Frame,
                .rtp_timestamp = packet.rtp.timestamp,
                .sub_unit_index = 0,
            };

        case VideoScanMode::Interlaced: {
            std::uint8_t sub_index = 0;
            if (packet.segments[0].header.field_id) {
                sub_index = 1;
            }
            return VideoAssemblyKey{.unit_kind = VideoAssemblyUnitKind::Field,
                                    .rtp_timestamp = packet.rtp.timestamp,
                                    .sub_unit_index = sub_index};
        }

        case VideoScanMode::PsF: {
            std::uint8_t sub_index = 0;
            if (packet.segments[0].header.field_id) {
                sub_index = 1;
            }

            return VideoAssemblyKey{.unit_kind = VideoAssemblyUnitKind::Segment,
                                    .rtp_timestamp = packet.rtp.timestamp,
                                    .sub_unit_index = sub_index};
        }
        }

        return {};
    }

    void begin_unit(const VideoAssemblyKey &key) {
        assembler_ = FrameAssembler(cfg_.width, unit_height_for_key(cfg_.scan_mode, cfg_.height, key.sub_unit_index),
                                    cfg_.format, cfg_.policy);
        assembler_.begin(key.rtp_timestamp);
        assembly_state_.current_key = key;
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
                end_res.unit->sub_unit_index = assembly_state_.current_key->sub_unit_index;
            }
            out.push_back(std::move(*end_res.unit));
        }

        assembly_state_ = {};
    }

    [[nodiscard]] static std::size_t pgroup_byte_offset(std::uint16_t sample_offset, std::size_t samples_per_pgroup,
                                                        std::size_t bytes_per_pgroup) noexcept {
        return (static_cast<std::size_t>(sample_offset) / samples_per_pgroup) * bytes_per_pgroup;
    }

    [[nodiscard]] static VideoFrameWriteOp map_segment_to_unit_local_write(PixelFormat target_format,
                                                                           const SrdSegmentView &segment) noexcept {
        switch (target_format) {
        case PixelFormat::UYVY:
            return VideoFrameWriteOp{
                .plane = 0,
                .row = segment.header.row_number,
                .byte_offset = static_cast<std::size_t>(segment.header.offset) * 2uz,
                .bytes = segment.data,
            };

        case PixelFormat::RGB8:
            return VideoFrameWriteOp{
                .plane = 0,
                .row = segment.header.row_number,
                .byte_offset = static_cast<std::size_t>(segment.header.offset) * 3uz,
                .bytes = segment.data,
            };

        case PixelFormat::YUV422RFC4175PG2BE10:
            return VideoFrameWriteOp{
                .plane = 0,
                .row = segment.header.row_number,
                .byte_offset = pgroup_byte_offset(segment.header.offset, 2uz, 5uz),
                .bytes = segment.data,
            };

        case PixelFormat::YUV422RFC4175PG2BE12:
            return VideoFrameWriteOp{
                .plane = 0,
                .row = segment.header.row_number,
                .byte_offset = pgroup_byte_offset(segment.header.offset, 2uz, 6uz),
                .bytes = segment.data,
            };

        case PixelFormat::YUV444RFC4175PG4BE10:
        case PixelFormat::RGBRFC4175PG4BE10:
            return VideoFrameWriteOp{
                .plane = 0,
                .row = segment.header.row_number,
                .byte_offset = pgroup_byte_offset(segment.header.offset, 4uz, 15uz),
                .bytes = segment.data,
            };

        case PixelFormat::YUV444RFC4175PG2BE12:
        case PixelFormat::RGBRFC4175PG2BE12:
            return VideoFrameWriteOp{
                .plane = 0,
                .row = segment.header.row_number,
                .byte_offset = pgroup_byte_offset(segment.header.offset, 2uz, 9uz),
                .bytes = segment.data,
            };
        }

        std::unreachable();
    }

    void write_packet_segments(const VideoPacketView &packet) {
        for (std::size_t i = 0; i < packet.segment_count; ++i) {
            auto op = map_segment_to_unit_local_write(cfg_.format, packet.segments[i]);
            assembler_.write_segment(op.plane, op.row, op.byte_offset, op.bytes);
        }
    }

    DepacketizerConfig cfg_;
    FrameAssembler assembler_;
    DepacketizerAssemblyState assembly_state_{};
    DepacketizerStats stats_{};
};

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_DEPACKETIZER_HPP
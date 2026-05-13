#ifndef ST2110_OBS_SOCKET_RX_AUDIO_BACKEND_HPP
#define ST2110_OBS_SOCKET_RX_AUDIO_BACKEND_HPP

#include "st2110/contracts/audio/audio_receiver_bootstrap.hpp"
#include "st2110/contracts/backend/backend.hpp"
#include "st2110/contracts/backend/backend_factory.hpp"
#include "st2110/foundation/bytes.hpp"
#include "st2110/ingress/shared/packet_parse.hpp"
#include "st2110/model/audio/audio_channel_order.hpp"
#include "st2110/receive/audio/audio_frame_assembler.hpp"
#include "st2110/receive/audio/audio_packet.hpp"
#include "st2110/receive/shared/receive_reorder_tolerance_policy.hpp"
#include "socket_rx_single_media_backend_base.hpp"
#include "st2110/receive/audio/audio_reorder_buffer.hpp"
#include "st2110/receive/audio/audio_timestamp_mapping.hpp"
#include "platform/socket_runtime.hpp"

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <utility>

namespace st2110 {
class SocketRxAudioBackend final : public SocketRxSingleMediaBackendBase {
  public:
    explicit SocketRxAudioBackend(const SocketAudioStartConfig &cfg)
        : SocketRxSingleMediaBackendBase(make_default_port_factory(), cfg.legs[0].max_udp_datagram_bytes,
                                         cfg.reorder_buffer_config, collect_open_configs(cfg),
                                         cfg.stream.expected_payload_type),
          audio_frame_assembler_(cfg.audio_frame_assembler_config),
          audio_timestamp_mapper_(cfg.rtp_timestamp_mapper_config) {}

    RxBackendLifecycleResult start(IFrameSink *sink) override {
        return start_common_runtime(sink);
    }

  protected:
    void process_received_datagram(ByteSpan udp_payload) noexcept override {
        auto packet = parse_audio_rtp_packet_view(udp_payload, *audio_packet_policy_);
        if (!packet) {
            record_rejected_media_packet();
            return;
        }
    }
  private:
    RxBackendLifecycleResult start_audio_runtime(IAudioFrameSink &sink) {
        auto receive_buffer = make_receive_buffer(cfg.common.packet_parse_policy);

        auto reorder_buffer = std::make_unique<AudioFixedWindowReorderBuffer>(cfg.reorder_buffer_config);
        auto audio_frame_assembler = std::make_unique<AudioFrameAssembler>(cfg.frame_assembler_config);

        std::optional<AudioRtpTimestampMapper> audio_timestamp_mapper;
        audio_timestamp_mapper.emplace(cfg.timestamp_mapper_config);

        packet_parse_policy_ = cfg.common.packet_parse_policy;
        audio_packet_policy_ = cfg.audio_packet_policy;
        reorder_tolerance_policy_ = cfg.reorder_buffer_config.reorder_tolerance_policy;
        reorder_buffer_ = std::move(reorder_buffer);
        audio_frame_assembler_ = std::move(audio_frame_assembler);
        audio_timestamp_mapper_ = std::move(audio_timestamp_mapper);
        audio_sink_ = &sink;
        configured_audio_payload_type_ = cfg.rx_config.payload_type;
        configured_sampling_rate_hz_ = cfg.rx_config.sampling_rate_hz;
        configured_packet_time_us_ = cfg.rx_config.packet_time_us;
        configured_samples_per_packet_ = cfg.rx_config.samples_per_packet;
        configured_channel_count_ = cfg.rx_config.channel_count;

        auto started = start_common_runtime(std::move(port), cfg.common.open_config, std::move(receive_buffer));
        if (!started) {
            clear_media_runtime_objects();
            return std::unexpected(started.error());
        }

        return started;
    }

    void drain_reorder_buffer_to_sink() noexcept {
        bool gap_flush_used = false;

        while (true) {
            auto stored_packet = reorder_buffer_->pop_next();
            if (!stored_packet) {
                if (gap_flush_used) {
                    return;
                }

                if (!receive_reorder_policy_allows_gap_flush_once(reorder_tolerance_policy_)) {
                    return;
                }

                if (!reorder_buffer_->flush_missing_once()) {
                    return;
                }

                gap_flush_used = true;
                continue;
            }

            auto block = audio_frame_assembler_->push(stored_packet->view());
            if (!block) {
                record_rejected_media_packet();
                continue;
            }

            deliver_assembled_audio_block(std::move(*block));
        }
    }

    void deliver_assembled_audio_block(AssembledAudioBlock &&block) noexcept {
        const TimestampNs timestamp_ns = map_block_timestamp_ns(block.rtp_timestamp);
        sink_->on_audio_frame(block.buffer, timestamp_ns);
    }

    [[nodiscard]] TimestampNs map_block_timestamp_ns(uint32_t rtp_timestamp) noexcept {
        auto mapped = audio_timestamp_mapper_->map(rtp_timestamp);
        if (!mapped.has_value()) {
            return 0;
        }

        return *mapped;
    }


    std::unique_ptr<AudioFrameAssembler> audio_frame_assembler_{};
    std::optional<AudioRtpTimestampMapper> audio_timestamp_mapper_{};

    std::optional<std::uint32_t> configured_sampling_rate_hz_{};
    std::optional<std::uint32_t> configured_packet_time_us_{};
    std::optional<std::uint32_t> configured_samples_per_packet_{};
    std::optional<std::uint16_t> configured_channel_count_{};
};

} // namespace st2110

#endif // ST2110_OBS_SOCKET_RX_AUDIO_BACKEND_HPP
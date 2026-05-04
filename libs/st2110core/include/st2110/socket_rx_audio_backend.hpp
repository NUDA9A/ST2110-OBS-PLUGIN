#ifndef ST2110_OBS_SOCKET_RX_AUDIO_BACKEND_HPP
#define ST2110_OBS_SOCKET_RX_AUDIO_BACKEND_HPP

#include "audio_channel_order.hpp"
#include "audio_frame_assembler.hpp"
#include "audio_packet.hpp"
#include "audio_receiver_bootstrap.hpp"
#include "audio_reorder_buffer.hpp"
#include "audio_timestamp_mapping.hpp"
#include "backend.hpp"
#include "backend_factory.hpp"
#include "bytes.hpp"
#include "packet_parse.hpp"
#include "receive_reorder_tolerance_policy.hpp"
#include "socket_runtime.hpp"
#include "socket_rx_single_media_backend_base.hpp"

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <utility>

namespace st2110 {
struct SocketRxAudioOperationalConfig {
    SocketRxOperationalCommonConfig common{};
    RxAudioConfig rx_config{};
    AudioRtpPacketPolicy audio_packet_policy{};
    AudioFrameAssemblerConfig frame_assembler_config{};
    AudioReorderBufferConfig reorder_buffer_config{};
    AudioRtpTimestampMapperConfig timestamp_mapper_config{};
    ParsedAudioChannelOrder channel_order{};
};

[[nodiscard]] inline Error validate_socket_rx_audio_operational_config(const SocketRxAudioOperationalConfig &cfg) {
    if (const Error err = validate_socket_rx_operational_common_config(cfg.common); err != Error::Ok) {
        return err;
    }

    if (const Error err = validate_rx_audio_config(cfg.rx_config); err != Error::Ok) {
        return err;
    }

    if (const Error err = validate_audio_rtp_packet_policy(cfg.audio_packet_policy); err != Error::Ok) {
        return err;
    }

    if (const Error err = validate_audio_frame_assembler_config(cfg.frame_assembler_config); err != Error::Ok) {
        return err;
    }

    if (const Error err = validate_audio_reorder_buffer_config(cfg.reorder_buffer_config); err != Error::Ok) {
        return err;
    }

    if (const Error err = validate_audio_rtp_timestamp_mapper_config(cfg.timestamp_mapper_config); err != Error::Ok) {
        return err;
    }

    if (const Error err =
            validate_parsed_audio_channel_order_against_channel_count(cfg.channel_order, cfg.rx_config.channel_count);
        err != Error::Ok) {
        return err;
    }

    auto expected_open_config = socket_rx_open_config_from_audio_config(cfg.rx_config);
    if (!expected_open_config) {
        return expected_open_config.error();
    }

    if (!socket_rx_open_config_equal(cfg.common.open_config, *expected_open_config)) {
        return Error::InvalidValue;
    }

    auto expected_audio_packet_policy = audio_rtp_packet_policy_from_rx_audio_config(cfg.rx_config);
    if (!expected_audio_packet_policy) {
        return expected_audio_packet_policy.error();
    }

    if (cfg.audio_packet_policy.sampling_rate_hz != expected_audio_packet_policy->sampling_rate_hz ||
        cfg.audio_packet_policy.channel_count != expected_audio_packet_policy->channel_count ||
        cfg.audio_packet_policy.samples_per_packet != expected_audio_packet_policy->samples_per_packet ||
        cfg.audio_packet_policy.payload_type != expected_audio_packet_policy->payload_type ||
        cfg.audio_packet_policy.wire_format != expected_audio_packet_policy->wire_format) {
        return Error::InvalidValue;
    }

    if (cfg.frame_assembler_config.storage_format != AudioSampleStorageFormat::InterleavedS32) {
        return Error::InvalidValue;
    }

    if (cfg.timestamp_mapper_config.rtp_clock_rate != cfg.rx_config.sampling_rate_hz) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

[[nodiscard]] inline std::expected<SocketRxAudioOperationalConfig, Error>
socket_rx_audio_operational_config_from_audio_receiver_bootstrap(const AudioReceiverBootstrapConfig &bootstrap) {
    auto open_config = socket_rx_open_config_from_audio_config(bootstrap.rx_config);
    if (!open_config) {
        return std::unexpected(open_config.error());
    }

    SocketRxAudioOperationalConfig operational{
        .common =
            SocketRxOperationalCommonConfig{
                .open_config = *open_config,
                .packet_parse_policy = bootstrap.packet_parse_policy,
            },
        .rx_config = bootstrap.rx_config,
        .audio_packet_policy = bootstrap.audio_packet_policy,
        .frame_assembler_config = bootstrap.frame_assembler_config,
        .reorder_buffer_config = bootstrap.reorder_buffer_config,
        .timestamp_mapper_config = bootstrap.timestamp_mapper_config,
        .channel_order = bootstrap.channel_order,
    };

    if (const Error err = validate_socket_rx_audio_operational_config(operational); err != Error::Ok) {
        return std::unexpected(err);
    }

    return operational;
}

[[nodiscard]] inline std::expected<SocketRxAudioOperationalConfig, Error>
socket_rx_audio_operational_config_from_rx_audio_config(const RxAudioConfig &cfg,
                                                        const PacketParsePolicy &packet_parse_policy,
                                                        const AudioFrameAssemblerConfig &frame_assembler_config,
                                                        const AudioReorderBufferConfig &reorder_buffer_config,
                                                        const AudioRtpTimestampMapperConfig &timestamp_mapper_config,
                                                        const ParsedAudioChannelOrder &channel_order) {
    auto open_config = socket_rx_open_config_from_audio_config(cfg);
    if (!open_config) {
        return std::unexpected(open_config.error());
    }

    auto audio_packet_policy = audio_rtp_packet_policy_from_rx_audio_config(cfg);
    if (!audio_packet_policy) {
        return std::unexpected(audio_packet_policy.error());
    }

    SocketRxAudioOperationalConfig operational{
        .common =
            SocketRxOperationalCommonConfig{
                .open_config = *open_config,
                .packet_parse_policy = packet_parse_policy,
            },
        .rx_config = cfg,
        .audio_packet_policy = *audio_packet_policy,
        .frame_assembler_config = frame_assembler_config,
        .reorder_buffer_config = reorder_buffer_config,
        .timestamp_mapper_config = timestamp_mapper_config,
        .channel_order = channel_order,
    };

    if (const Error err = validate_socket_rx_audio_operational_config(operational); err != Error::Ok) {
        return std::unexpected(err);
    }

    return operational;
}

class SocketRxAudioBackend final : public SocketRxSingleMediaBackendBase, public ISocketRxAudioBackend {
  public:
    SocketRxAudioBackend()
        : SocketRxSingleMediaBackendBase(RxMediaKind::Audio, RxBackendCapabilities{.video_rx = false, .audio_rx = true},
                                         make_default_port_factory()) {}

    explicit SocketRxAudioBackend(std::unique_ptr<ISocketRxPortFactory> port_factory)
        : SocketRxSingleMediaBackendBase(RxMediaKind::Audio, RxBackendCapabilities{.video_rx = false, .audio_rx = true},
                                         std::move(port_factory)) {}

    RxBackendLifecycleResult start_audio(const SocketRxAudioOperationalConfig &cfg, IAudioFrameSink &sink) override {
        if (Error err = validate_common_start_preconditions(); err != Error::Ok) {
            return std::unexpected(err);
        }

        if (Error err = validate_socket_rx_audio_operational_config(cfg); err != Error::Ok) {
            return std::unexpected(err);
        }

        auto port = create_port();
        if (port == nullptr) {
            return std::unexpected(Error::InvalidValue);
        }

        return start_audio_runtime(cfg, sink, std::move(port));
    }

  protected:
    void clear_media_runtime_objects() noexcept override {
        clear_common_runtime_objects();

        audio_packet_policy_.reset();
        reorder_buffer_.reset();
        audio_frame_assembler_.reset();
        audio_timestamp_mapper_.reset();
        configured_audio_payload_type_.reset();
        configured_sampling_rate_hz_.reset();
        configured_packet_time_us_.reset();
        configured_samples_per_packet_.reset();
        configured_channel_count_.reset();
        packet_parse_policy_ = {};
        reorder_tolerance_policy_ = {};
        audio_sink_ = nullptr;
    }

    void process_received_datagram(ByteSpan udp_payload) noexcept override {
        if (!audio_sink_ || !audio_packet_policy_ || !reorder_buffer_ || !audio_frame_assembler_ ||
            !audio_timestamp_mapper_ || !configured_audio_payload_type_) {
            return;
        }

        if (is_rtcp_like_datagram(udp_payload)) {
            record_ignored_control_datagram();
            return;
        }

        if (const Error err = validate_packet_parse_policy(udp_payload, packet_parse_policy_); err != Error::Ok) {
            record_rejected_packet(err, PacketParseStage::PacketPolicy);
            return;
        }

        if (!datagram_matches_configured_payload_type(udp_payload, *configured_audio_payload_type_)) {
            record_ignored_nonmedia_datagram();
            return;
        }

        auto packet = parse_audio_rtp_packet_view(udp_payload, *audio_packet_policy_);
        if (!packet) {
            record_rejected_media_packet();
            return;
        }

        if (Error err = reorder_buffer_->push(*packet); err != Error::Ok) {
            (void)err;
            record_rejected_media_packet();
            return;
        }

        record_accepted_media_packet();
        drain_reorder_buffer_to_sink();
    }

    void augment_stats_snapshot_locked(BackendStats &snapshot) const noexcept override {
        if (reorder_buffer_ != nullptr) {
            const auto &audio_reorder = reorder_buffer_->stats();
            snapshot.reorder.packets_pushed = audio_reorder.packets_pushed;
            snapshot.reorder.packets_popped = audio_reorder.packets_popped;
            snapshot.reorder.duplicates = audio_reorder.duplicates;
            snapshot.reorder.late_packets = audio_reorder.late_packets;
            snapshot.reorder.out_of_window = audio_reorder.out_of_window;
            snapshot.reorder.missing_seq_flushed = audio_reorder.missing_packets_flushed;
        }
    }

  private:
    RxBackendLifecycleResult start_audio_runtime(const SocketRxAudioOperationalConfig &cfg, IAudioFrameSink &sink,
                                                 std::unique_ptr<ISocketRxPort> port) {
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
        if (!reorder_buffer_ || !audio_frame_assembler_) {
            return;
        }

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
        if (audio_sink_ == nullptr || !block.complete) {
            return;
        }

        const TimestampNs timestamp_ns = map_block_timestamp_ns(block.rtp_timestamp);
        audio_sink_->on_audio_frame(block.buffer.view(timestamp_ns));
        record_delivered_media_unit();
    }

    [[nodiscard]] TimestampNs map_block_timestamp_ns(uint32_t rtp_timestamp) noexcept {
        if (!audio_timestamp_mapper_.has_value()) {
            return 0;
        }

        auto mapped = audio_timestamp_mapper_->map(rtp_timestamp);
        if (!mapped.has_value()) {
            return 0;
        }

        return *mapped;
    }

    PacketParsePolicy packet_parse_policy_{};
    ReceiveReorderTolerancePolicy reorder_tolerance_policy_{};
    std::optional<AudioRtpPacketPolicy> audio_packet_policy_{};
    std::unique_ptr<AudioFixedWindowReorderBuffer> reorder_buffer_{};
    std::unique_ptr<AudioFrameAssembler> audio_frame_assembler_{};
    std::optional<AudioRtpTimestampMapper> audio_timestamp_mapper_{};
    IAudioFrameSink *audio_sink_ = nullptr;

    std::optional<std::uint8_t> configured_audio_payload_type_{};
    std::optional<std::uint32_t> configured_sampling_rate_hz_{};
    std::optional<std::uint32_t> configured_packet_time_us_{};
    std::optional<std::uint32_t> configured_samples_per_packet_{};
    std::optional<std::uint16_t> configured_channel_count_{};
};

class SocketRxAudioBackendFactory final : public IRxBackendFactory {
  public:
    [[nodiscard]] RxBackendDescriptor descriptor() const override {
        return RxBackendDescriptor{.kind = RxBackendKind::Socket,
                                   .name = "socket",
                                   .capabilities = RxBackendCapabilities{.video_rx = false, .audio_rx = true},
                                   .available = true};
    }

    [[nodiscard]] std::unique_ptr<IRxBackend> create_backend() const override {
        return std::unique_ptr<IRxBackend>(new SocketRxAudioBackend());
    }
};

} // namespace st2110

#endif // ST2110_OBS_SOCKET_RX_AUDIO_BACKEND_HPP
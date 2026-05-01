#ifndef ST2110_OBS_SOCKET_RX_AUDIO_BACKEND_HPP
#define ST2110_OBS_SOCKET_RX_AUDIO_BACKEND_HPP

#include "audio_frame_assembler.hpp"
#include "audio_packet.hpp"
#include "audio_reorder_buffer.hpp"
#include "audio_timestamp_mapping.hpp"
#include "backend.hpp"
#include "backend_factory.hpp"
#include "bytes.hpp"
#include "packet_parse.hpp"
#include "socket_runtime.hpp"
#include "socket_rx_single_media_backend_base.hpp"

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <utility>

namespace st2110 {

class SocketRxAudioBackend final : public SocketRxSingleMediaBackendBase, public IRxAudioBackend {
  public:
    SocketRxAudioBackend()
        : SocketRxSingleMediaBackendBase(RxMediaKind::Audio, RxBackendCapabilities{.video_rx = false, .audio_rx = true},
                                         make_default_port_factory()) {}

    explicit SocketRxAudioBackend(std::unique_ptr<ISocketRxPortFactory> port_factory)
        : SocketRxSingleMediaBackendBase(RxMediaKind::Audio, RxBackendCapabilities{.video_rx = false, .audio_rx = true},
                                         std::move(port_factory)) {}

    RxBackendLifecycleResult start_audio(const RxAudioConfig &cfg, IAudioFrameSink &sink) override {
        if (Error err = validate_common_start_preconditions(); err != Error::Ok) {
            return std::unexpected(err);
        }

        auto open_cfg = build_open_config(cfg);
        if (!open_cfg) {
            return std::unexpected(open_cfg.error());
        }

        auto port = create_port();
        if (port == nullptr) {
            return std::unexpected(Error::InvalidValue);
        }

        return start_audio_runtime(cfg, sink, *open_cfg, std::move(port));
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

  private:
    [[nodiscard]] static PacketParsePolicy build_packet_parse_policy(const RxAudioConfig &cfg) noexcept {
        (void)cfg;
        return PacketParsePolicy{};
    }

    [[nodiscard]] static std::expected<SocketRxOpenConfig, Error> build_open_config(const RxAudioConfig &cfg) {
        auto res = socket_rx_open_config_from_audio_config(cfg);
        if (!res) {
            return std::unexpected(res.error());
        }

        return res;
    }

    [[nodiscard]] static std::expected<AudioRtpPacketPolicy, Error>
    build_audio_packet_policy(const RxAudioConfig &cfg) {
        return audio_rtp_packet_policy_from_rx_audio_config(cfg);
    }

    [[nodiscard]] static AudioFrameAssemblerConfig
    build_audio_frame_assembler_config(const RxAudioConfig &cfg) noexcept {
        (void)cfg;
        return AudioFrameAssemblerConfig{.storage_format = AudioSampleStorageFormat::InterleavedS32};
    }

    RxBackendLifecycleResult start_audio_runtime(const RxAudioConfig &cfg, IAudioFrameSink &sink,
                                                 const SocketRxOpenConfig &open_cfg,
                                                 std::unique_ptr<ISocketRxPort> port) {
        PacketParsePolicy packet_parse_policy = build_packet_parse_policy(cfg);
        auto receive_buffer = make_receive_buffer(packet_parse_policy);

        auto audio_packet_policy = build_audio_packet_policy(cfg);
        if (!audio_packet_policy) {
            return std::unexpected(audio_packet_policy.error());
        }

        const AudioFrameAssemblerConfig audio_frame_assembler_cfg = build_audio_frame_assembler_config(cfg);
        const AudioReorderBufferConfig audio_reorder_cfg = build_audio_reorder_buffer_config(cfg);
        const AudioRtpTimestampMapperConfig audio_timestamp_mapper_cfg = build_audio_timestamp_mapper_config(cfg);

        auto reorder_buffer = std::make_unique<AudioFixedWindowReorderBuffer>(audio_reorder_cfg);
        auto audio_frame_assembler = std::make_unique<AudioFrameAssembler>(audio_frame_assembler_cfg);

        std::optional<AudioRtpTimestampMapper> audio_timestamp_mapper;
        audio_timestamp_mapper.emplace(audio_timestamp_mapper_cfg);

        packet_parse_policy_ = packet_parse_policy;
        audio_packet_policy_ = std::move(*audio_packet_policy);
        reorder_buffer_ = std::move(reorder_buffer);
        audio_frame_assembler_ = std::move(audio_frame_assembler);
        audio_timestamp_mapper_ = std::move(audio_timestamp_mapper);
        audio_sink_ = &sink;
        configured_audio_payload_type_ = cfg.payload_type;
        configured_sampling_rate_hz_ = cfg.sampling_rate_hz;
        configured_packet_time_us_ = cfg.packet_time_us;
        configured_samples_per_packet_ = cfg.samples_per_packet;
        configured_channel_count_ = cfg.channel_count;

        auto started = start_common_runtime(std::move(port), open_cfg, std::move(receive_buffer));
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

        while (true) {
            auto stored_packet = reorder_buffer_->pop_next();
            if (!stored_packet) {
                return;
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

    [[nodiscard]] static AudioReorderBufferConfig build_audio_reorder_buffer_config(const RxAudioConfig &cfg) noexcept {
        (void)cfg;
        return AudioReorderBufferConfig{};
    }

    [[nodiscard]] static AudioRtpTimestampMapperConfig
    build_audio_timestamp_mapper_config(const RxAudioConfig &cfg) noexcept {
        return AudioRtpTimestampMapperConfig{
            .rtp_clock_rate = cfg.sampling_rate_hz,
            .anchor_rtp_timestamp = 0,
            .anchor_timestamp_ns = 0,
        };
    }

    PacketParsePolicy packet_parse_policy_{};
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
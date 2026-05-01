#ifndef ST2110_OBS_SOCKET_RX_AUDIO_BACKEND_HPP
#define ST2110_OBS_SOCKET_RX_AUDIO_BACKEND_HPP

#include "backend.hpp"
#include "backend_factory.hpp"
#include "bytes.hpp"
#include "packet_parse.hpp"
#include "socket_runtime.hpp"
#include "socket_rx_single_media_backend_base.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

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

        configured_audio_payload_type_.reset();

        configured_sampling_rate_hz_.reset();
        configured_packet_time_us_.reset();
        configured_samples_per_packet_.reset();
        configured_channel_count_.reset();

        packet_parse_policy_ = {};
        audio_sink_ = nullptr;
    }

    void process_received_datagram(ByteSpan udp_payload) noexcept override {
        if (audio_sink_ == nullptr || !configured_audio_payload_type_ || !configured_sampling_rate_hz_ ||
            !configured_packet_time_us_ || !configured_samples_per_packet_ || !configured_channel_count_) {
            return;
        }

        if (is_rtcp_like_datagram(udp_payload)) {
            record_ignored_control_datagram();
        }

        if (!datagram_matches_configured_payload_type(udp_payload, *configured_audio_payload_type_)) {
            record_ignored_nonmedia_datagram();
            return;
        }
    }

  private:
    [[nodiscard]] static PacketParsePolicy build_packet_parse_policy(const RxAudioConfig &cfg) noexcept {
        return PacketParsePolicy{};
    }

    [[nodiscard]] static std::expected<SocketRxOpenConfig, Error> build_open_config(const RxAudioConfig &cfg) {
        auto res = socket_rx_open_config_from_audio_config(cfg);
        if (!res) {
            return std::unexpected(res.error());
        }

        return res;
    }

    RxBackendLifecycleResult start_audio_runtime(const RxAudioConfig &cfg, IAudioFrameSink &sink,
                                                 const SocketRxOpenConfig &open_cfg,
                                                 std::unique_ptr<ISocketRxPort> port) {
        PacketParsePolicy packet_parse_policy = build_packet_parse_policy(cfg);
        auto receive_buffer = make_receive_buffer(packet_parse_policy);

        packet_parse_policy_ = packet_parse_policy;
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

    PacketParsePolicy packet_parse_policy_{};

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
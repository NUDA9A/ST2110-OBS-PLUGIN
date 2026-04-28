#ifndef ST2110_OBS_PLUGIN_AUDIO_PACKET_HPP
#define ST2110_OBS_PLUGIN_AUDIO_PACKET_HPP

#include "error.hpp"
#include "rtp.hpp"
#include "bytes.hpp"
#include "rx_config.hpp"

#include <cstdint>
#include <expected>
#include <cstddef>

namespace st2110 {
    enum class AudioPcmWireFormat {
        L16,
        L24,
    };

    struct AudioRtpPacketPolicy {
        uint32_t sampling_rate_hz = 0;
        uint16_t channel_count = 0;
        uint32_t samples_per_packet = 0;
        uint8_t payload_type = 0;
        AudioPcmWireFormat wire_format = AudioPcmWireFormat::L24;
    };

    struct AudioRtpPacketView {
        RtpHeaderView rtp{};
        ByteSpan payload{};
        uint32_t sampling_rate_hz = 0;
        uint16_t channel_count = 0;
        uint32_t samples_per_channel = 0;
        AudioPcmWireFormat wire_format = AudioPcmWireFormat::L24;
    };

    [[nodiscard]] inline std::expected<std::size_t, Error>
    audio_pcm_wire_sample_bytes(AudioPcmWireFormat wire_format) {
        switch (wire_format) {
            case AudioPcmWireFormat::L16:
                return 2;
            case AudioPcmWireFormat::L24:
                return 3;
            default:
                return std::unexpected(Error::InvalidValue);
        }
    }

    [[nodiscard]] inline std::expected<AudioRtpPacketPolicy, Error>
    audio_rtp_packet_policy_from_rx_audio_config(
            const RxAudioConfig& cfg,
            AudioPcmWireFormat wire_format) {
        if (Error err = validate_rx_audio_config(cfg); err != Error::Ok) {
            return std::unexpected(err);
        }

        return AudioRtpPacketPolicy{
            cfg.sampling_rate_hz,
            cfg.channel_count,
            cfg.samples_per_packet,
            cfg.payload_type,
            wire_format
        };
    }

    [[nodiscard]] inline std::expected<std::size_t, Error>
    audio_rtp_packet_payload_size_bytes(const AudioRtpPacketPolicy& policy) {
        auto bytes_per_sample = audio_pcm_wire_sample_bytes(policy.wire_format);
        if (!bytes_per_sample) {
            return std::unexpected(bytes_per_sample.error());
        }
        return static_cast<std::size_t>(policy.samples_per_packet) * static_cast<std::size_t>(policy.channel_count) * (*bytes_per_sample);
    }

    [[nodiscard]] inline std::expected<AudioRtpPacketView, Error>
    make_audio_rtp_packet_view(
            const RtpHeaderView& rtp,
            ByteSpan payload,
            const AudioRtpPacketPolicy& policy) {
        if (rtp.payload_type != policy.payload_type) {
            return std::unexpected(Error::InvalidValue);
        }

        auto expected_payload_size = audio_rtp_packet_payload_size_bytes(policy);
        if (!expected_payload_size) {
            return std::unexpected(expected_payload_size.error());
        }

        if (payload.size() != *expected_payload_size) {
            return std::unexpected(Error::InvalidValue);
        }

        return AudioRtpPacketView{
            rtp,
            payload,
            policy.sampling_rate_hz,
            policy.channel_count,
            policy.samples_per_packet,
            policy.wire_format
        };
    }
}

#endif //ST2110_OBS_PLUGIN_AUDIO_PACKET_HPP

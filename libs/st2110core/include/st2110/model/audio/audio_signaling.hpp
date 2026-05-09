#ifndef ST2110_OBS_PLUGIN_AUDIO_SIGNALING_HPP
#define ST2110_OBS_PLUGIN_AUDIO_SIGNALING_HPP

#include <st2110/foundation/error.hpp>
#include <st2110/model/audio/audio_channel_order.hpp>

#include <cstdint>
#include <expected>
#include <limits>
#include <optional>

namespace st2110 {
enum class AudioPcmEncoding { LinearPcm };

enum class AudioPcmBitDepth {
    Bits16,
    Bits24,
};

struct AudioMediaDescription {
    AudioPcmEncoding pcm_encoding = AudioPcmEncoding::LinearPcm;
    AudioPcmBitDepth pcm_bit_depth = AudioPcmBitDepth::Bits24;
    uint32_t sampling_rate_hz = 0;
    uint32_t packet_time_us = 0;
    uint16_t channel_count = 0;
};

struct AudioStreamSignaling {
    AudioMediaDescription media{};
    std::optional<AudioChannelOrder> channel_order{};
};

[[nodiscard]] inline Error validate_audio_sampling_rate_st2110_scope(uint32_t sampling_rate_hz) {
    switch (sampling_rate_hz) {
    case 0:
        return Error::InvalidValue;
    case 44100:
    case 48000:
    case 96000:
        return Error::Ok;
    default:
        return Error::Unsupported;
    }
}

[[nodiscard]] inline Error validate_audio_packet_time_st2110_scope(uint32_t packet_time_us) {
    switch (packet_time_us) {
    case 0:
        return Error::InvalidValue;
    case 1000:
    case 125:
        return Error::Ok;
    default:
        return Error::Unsupported;
    }
}

[[nodiscard]] inline Error validate_audio_channel_count_st2110_scope(uint16_t channel_count) {
    if (channel_count < 1) {
        return Error::InvalidValue;
    }

    if (channel_count > 64) {
        return Error::Unsupported;
    }

    return Error::Ok;
}

[[nodiscard]] inline std::expected<uint32_t, Error> derive_audio_samples_per_packet(uint32_t sampling_rate_hz,
                                                                                    uint32_t packet_time_us) {
    if (sampling_rate_hz == 0 || packet_time_us == 0) {
        return std::unexpected(Error::InvalidValue);
    }

    const uint64_t product = static_cast<uint64_t>(sampling_rate_hz) * static_cast<uint64_t>(packet_time_us);

    if (product % 1000000ULL != 0) {
        return std::unexpected(Error::InvalidValue);
    }

    const uint64_t samples_per_packet = product / 1000000ULL;

    if (samples_per_packet == 0 || samples_per_packet > std::numeric_limits<uint32_t>::max()) {
        return std::unexpected(Error::InvalidValue);
    }

    return static_cast<uint32_t>(samples_per_packet);
}

[[nodiscard]] inline std::expected<uint32_t, Error>
audio_samples_per_packet_from_media_description(const AudioMediaDescription &media) {
    return derive_audio_samples_per_packet(media.sampling_rate_hz, media.packet_time_us);
}

[[nodiscard]] inline Error validate_audio_media_description_structure(const AudioMediaDescription &media) {
    if (const Error err = validate_audio_sampling_rate_st2110_scope(media.sampling_rate_hz); err != Error::Ok) {
        return err;
    }

    if (const Error err = validate_audio_packet_time_st2110_scope(media.packet_time_us); err != Error::Ok) {
        return err;
    }

    if (const Error err = validate_audio_channel_count_st2110_scope(media.channel_count); err != Error::Ok) {
        return err;
    }

    auto samples_per_packet = audio_samples_per_packet_from_media_description(media);
    if (!samples_per_packet.has_value()) {
        return samples_per_packet.error();
    }

    return Error::Ok;
}

[[nodiscard]] inline Error validate_audio_stream_signaling(const AudioStreamSignaling &signaling) {
    if (const Error err = validate_audio_media_description_structure(signaling.media); err != Error::Ok) {
        return err;
    }

    if (signaling.channel_order.has_value()) {
        if (const Error err = validate_audio_channel_order(*signaling.channel_order); err != Error::Ok) {
            return err;
        }

        if (const Error err = validate_audio_channel_order_against_channel_count(*signaling.channel_order,
                                                                                 signaling.media.channel_count);
            err != Error::Ok) {
            return err;
        }
    }

    return Error::Ok;
}

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_AUDIO_SIGNALING_HPP
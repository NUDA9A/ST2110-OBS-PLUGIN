#ifndef ST2110_OBS_PLUGIN_AUDIO_SIGNALING_HPP
#define ST2110_OBS_PLUGIN_AUDIO_SIGNALING_HPP

#include "error.hpp"

#include <cstdint>
#include <string>
#include <optional>

namespace st2110 {
    enum class AudioConformanceLevel {
        LevelA,
        LevelAX,
        LevelB,
        LevelBX,
        LevelC,
        LevelCX
    };

    struct AudioConformanceRange {
        AudioConformanceLevel level;
        uint32_t sampling_rate_hz;
        uint32_t packet_time_us;
        uint16_t min_channels;
        uint16_t max_channels;
    };

    enum class AudioPcmEncoding {
        LinearPcm
    };

    struct AudioMediaDescription {
        AudioPcmEncoding pcm_encoding = AudioPcmEncoding::LinearPcm;
        uint32_t sampling_rate_hz = 0;
        uint32_t packet_time_us = 0;
        uint16_t channel_count = 0;
    };

    enum class AudioChannelOrderConvention {
        Unspecified,
        Smpte2110,
        Other
    };

    struct AudioChannelOrderSignaling {
        AudioChannelOrderConvention convention = AudioChannelOrderConvention::Unspecified;
        std::string raw_value;
    };

    struct AudioStreamSignaling {
        AudioMediaDescription media;
        std::optional<AudioChannelOrderSignaling> channel_order;
    };

    [[nodiscard]] constexpr AudioConformanceRange audio_level_a_receiver_baseline() {
        return {
            AudioConformanceLevel::LevelA,
            48000,
            1000,
            1,
            8
        };
    }

    [[nodiscard]] inline constexpr Error validate_audio_conformance_range(const AudioConformanceRange& range) {
        if (range.sampling_rate_hz == 0) {
            return Error::InvalidValue;
        }
        if (range.packet_time_us == 0) {
            return Error::InvalidValue;
        }
        if (range.min_channels < 1) {
            return Error::InvalidValue;
        }
        if (range.max_channels < range.min_channels) {
            return Error::InvalidValue;
        }
        switch (range.level) {
            case AudioConformanceLevel::LevelA:
            case AudioConformanceLevel::LevelB:
            case AudioConformanceLevel::LevelC:
            case AudioConformanceLevel::LevelAX:
            case AudioConformanceLevel::LevelBX:
            case AudioConformanceLevel::LevelCX:
                break;
            default:
                return Error::InvalidValue;
        }

        return Error::Ok;
    }

    [[nodiscard]] inline constexpr bool audio_media_description_matches_conformance_range(
            const AudioMediaDescription& media,
            const AudioConformanceRange& range) {
        if (media.sampling_rate_hz != range.sampling_rate_hz) {
            return false;
        }
        if (media.packet_time_us != range.packet_time_us) {
            return false;
        }
        if (range.min_channels > media.channel_count || range.max_channels < media.channel_count) {
            return false;
        }

        return true;
    }

    [[nodiscard]] inline constexpr Error validate_audio_media_description_against_conformance_range(
            const AudioMediaDescription& media,
            const AudioConformanceRange& range) {
        switch (media.pcm_encoding) {
            case AudioPcmEncoding::LinearPcm:
                break;
            default:
                return Error::InvalidValue;
        }

        if (Error err = validate_audio_conformance_range(range); err != Error::Ok) {
            return err;
        }
        if (!audio_media_description_matches_conformance_range(media, range)) {
            return Error::InvalidValue;
        }

        return Error::Ok;
    }

    [[nodiscard]] inline Error validate_audio_channel_order_signaling(
            const AudioChannelOrderSignaling& channel_order) {
        switch (channel_order.convention) {
            case AudioChannelOrderConvention::Unspecified:
                if (!channel_order.raw_value.empty()) {
                    return Error::InvalidValue;
                }
                break;
            case AudioChannelOrderConvention::Smpte2110:
                if (channel_order.raw_value.empty()) {
                    return Error::InvalidValue;
                }
                if (!channel_order.raw_value.starts_with("SMPTE2110.")) {
                    return Error::InvalidValue;
                }
                break;
            case AudioChannelOrderConvention::Other:
                if (channel_order.raw_value.empty()) {
                    return Error::InvalidValue;
                }
                break;
            default:
                return Error::InvalidValue;
        }

        return Error::Ok;
    }

    [[nodiscard]] inline Error validate_audio_stream_signaling(
            const AudioStreamSignaling& signaling) {
        if (Error err = validate_audio_media_description_against_conformance_range(signaling.media, audio_level_a_receiver_baseline()); err != Error::Ok) {
            return err;
        }
        if (signaling.channel_order) {
            if (Error err = validate_audio_channel_order_signaling(*signaling.channel_order); err != Error::Ok) {
                return err;
            }
        }

        return Error::Ok;
    }
}

#endif //ST2110_OBS_PLUGIN_AUDIO_SIGNALING_HPP

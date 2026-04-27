#ifndef ST2110_OBS_PLUGIN_AUDIO_SIGNALING_HPP
#define ST2110_OBS_PLUGIN_AUDIO_SIGNALING_HPP


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
}

#endif //ST2110_OBS_PLUGIN_AUDIO_SIGNALING_HPP

#ifndef ST2110_OBS_PLUGIN_AUDIO_SIGNALING_HPP
#define ST2110_OBS_PLUGIN_AUDIO_SIGNALING_HPP

#include <st2110/foundation/error.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

namespace st2110 {
enum class AudioConformanceRole { Sender, Receiver };

enum class AudioConformanceLevel { LevelA, LevelAX, LevelB, LevelBX, LevelC, LevelCX };

struct AudioConformanceRange {
    AudioConformanceRole role;
    AudioConformanceLevel level;
    uint32_t sampling_rate_hz;
    uint32_t packet_time_us;
    uint16_t min_channels;
    uint16_t max_channels;
};

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

enum class AudioChannelOrderConvention { Unspecified, Smpte2110, Other };

struct AudioChannelOrderSignaling {
    AudioChannelOrderConvention convention = AudioChannelOrderConvention::Unspecified;
    std::string raw_value;
};

struct AudioStreamSignaling {
    AudioMediaDescription media;
    std::optional<AudioChannelOrderSignaling> channel_order;
};

struct AudioChannelOrderDeclaredCountValidation {
    Error error = Error::Ok;
    uint16_t declared_channel_count = 0;
    bool has_declared_channel_count = false;
};

inline constexpr std::array<AudioConformanceRange, 6> st2110_30_sender_conformance_ranges{{
    {AudioConformanceRole::Sender, AudioConformanceLevel::LevelA, 48000, 1000, 1, 8},
    {AudioConformanceRole::Sender, AudioConformanceLevel::LevelAX, 96000, 1000, 1, 4},
    {AudioConformanceRole::Sender, AudioConformanceLevel::LevelB, 48000, 125, 1, 8},
    {AudioConformanceRole::Sender, AudioConformanceLevel::LevelBX, 96000, 125, 1, 8},
    {AudioConformanceRole::Sender, AudioConformanceLevel::LevelC, 48000, 125, 9, 64},
    {AudioConformanceRole::Sender, AudioConformanceLevel::LevelCX, 96000, 125, 9, 32},
}};

inline constexpr std::array<AudioConformanceRange, 15> st2110_30_receiver_conformance_ranges{{
    {AudioConformanceRole::Receiver, AudioConformanceLevel::LevelA, 48000, 1000, 1, 8},

    {AudioConformanceRole::Receiver, AudioConformanceLevel::LevelAX, 48000, 1000, 1, 8},
    {AudioConformanceRole::Receiver, AudioConformanceLevel::LevelAX, 96000, 1000, 1, 4},

    {AudioConformanceRole::Receiver, AudioConformanceLevel::LevelB, 48000, 1000, 1, 8},
    {AudioConformanceRole::Receiver, AudioConformanceLevel::LevelB, 48000, 125, 1, 8},

    {AudioConformanceRole::Receiver, AudioConformanceLevel::LevelBX, 48000, 1000, 1, 8},
    {AudioConformanceRole::Receiver, AudioConformanceLevel::LevelBX, 48000, 125, 1, 8},
    {AudioConformanceRole::Receiver, AudioConformanceLevel::LevelBX, 96000, 1000, 1, 4},
    {AudioConformanceRole::Receiver, AudioConformanceLevel::LevelBX, 96000, 125, 1, 8},

    {AudioConformanceRole::Receiver, AudioConformanceLevel::LevelC, 48000, 1000, 1, 8},
    {AudioConformanceRole::Receiver, AudioConformanceLevel::LevelC, 48000, 125, 1, 64},

    {AudioConformanceRole::Receiver, AudioConformanceLevel::LevelCX, 48000, 1000, 1, 8},
    {AudioConformanceRole::Receiver, AudioConformanceLevel::LevelCX, 48000, 125, 1, 64},
    {AudioConformanceRole::Receiver, AudioConformanceLevel::LevelCX, 96000, 1000, 1, 4},
    {AudioConformanceRole::Receiver, AudioConformanceLevel::LevelCX, 96000, 125, 1, 32},
}};

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

[[nodiscard]] inline bool audio_media_description_matches_conformance_range(const AudioMediaDescription &media,
                                                                            const AudioConformanceRange &range) {
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

    return Error::Ok;
}

[[nodiscard]] inline bool audio_signaling_channel_order_is_digit(char c) { return c >= '0' && c <= '9'; }

[[nodiscard]] inline bool audio_signaling_channel_order_token_contains_ws(std::string_view token) {
    for (const char c : token) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            return true;
        }
    }

    return false;
}

[[nodiscard]] inline AudioChannelOrderDeclaredCountValidation
audio_signaling_smpte2110_channel_order_symbol_count(std::string_view symbol) {
    AudioChannelOrderDeclaredCountValidation result{};

    if (symbol.empty()) {
        result.error = Error::InvalidValue;
        return result;
    }

    if (audio_signaling_channel_order_token_contains_ws(symbol)) {
        result.error = Error::InvalidValue;
        return result;
    }

    if (symbol == "M") {
        result.declared_channel_count = 1;
        result.has_declared_channel_count = true;
        return result;
    }

    if (symbol == "ST") {
        result.declared_channel_count = 2;
        result.has_declared_channel_count = true;
        return result;
    }

    if (symbol == "DM") {
        result.declared_channel_count = 2;
        result.has_declared_channel_count = true;
        return result;
    }

    if (symbol == "LtRt") {
        result.declared_channel_count = 2;
        result.has_declared_channel_count = true;
        return result;
    }

    if (symbol == "51") {
        result.declared_channel_count = 6;
        result.has_declared_channel_count = true;
        return result;
    }

    if (symbol == "71") {
        result.declared_channel_count = 8;
        result.has_declared_channel_count = true;
        return result;
    }

    if (symbol == "222") {
        result.declared_channel_count = 24;
        result.has_declared_channel_count = true;
        return result;
    }

    if (symbol == "SGRP") {
        result.declared_channel_count = 4;
        result.has_declared_channel_count = true;
        return result;
    }

    if (symbol.starts_with("U")) {
        if (symbol.size() != 3 || !audio_signaling_channel_order_is_digit(symbol[1]) ||
            !audio_signaling_channel_order_is_digit(symbol[2])) {
            result.error = Error::InvalidValue;
            return result;
        }

        const uint16_t count = static_cast<uint16_t>(static_cast<uint16_t>(symbol[1] - '0') * 10U +
                                                     static_cast<uint16_t>(symbol[2] - '0'));

        if (count == 0 || count > 64) {
            result.error = Error::InvalidValue;
            return result;
        }

        result.declared_channel_count = count;
        result.has_declared_channel_count = true;
        return result;
    }

    result.error = Error::InvalidValue;
    return result;
}

[[nodiscard]] inline AudioChannelOrderDeclaredCountValidation
parse_smpte2110_audio_channel_order_raw_value_and_count(std::string_view raw_value) {
    static constexpr std::string_view prefix = "SMPTE2110.";

    AudioChannelOrderDeclaredCountValidation result{};

    if (!raw_value.starts_with(prefix)) {
        result.error = Error::InvalidValue;
        return result;
    }

    std::string_view groups_text = raw_value.substr(prefix.size());

    if (groups_text.size() < 3 || groups_text.front() != '(' || groups_text.back() != ')') {
        result.error = Error::InvalidValue;
        return result;
    }

    groups_text.remove_prefix(1);
    groups_text.remove_suffix(1);

    if (groups_text.empty()) {
        result.error = Error::InvalidValue;
        return result;
    }

    uint32_t declared_channel_count = 0;
    std::size_t group_start = 0;

    while (group_start <= groups_text.size()) {
        const std::size_t separator_pos = groups_text.find(',', group_start);

        const std::string_view symbol = separator_pos == std::string_view::npos
                                            ? groups_text.substr(group_start)
                                            : groups_text.substr(group_start, separator_pos - group_start);

        auto group_count = audio_signaling_smpte2110_channel_order_symbol_count(symbol);

        if (group_count.error != Error::Ok) {
            result.error = group_count.error;
            return result;
        }

        declared_channel_count += group_count.declared_channel_count;

        if (declared_channel_count > std::numeric_limits<uint16_t>::max()) {
            result.error = Error::InvalidValue;
            return result;
        }

        if (separator_pos == std::string_view::npos) {
            break;
        }

        group_start = separator_pos + 1;

        if (group_start > groups_text.size()) {
            result.error = Error::InvalidValue;
            return result;
        }
    }

    if (declared_channel_count == 0) {
        result.error = Error::InvalidValue;
        return result;
    }

    result.declared_channel_count = static_cast<uint16_t>(declared_channel_count);
    result.has_declared_channel_count = true;
    return result;
}

[[nodiscard]] inline Error validate_audio_channel_order_signaling(const AudioChannelOrderSignaling &channel_order) {
    switch (channel_order.convention) {
    case AudioChannelOrderConvention::Unspecified:
        if (!channel_order.raw_value.empty()) {
            return Error::InvalidValue;
        }
        break;

    case AudioChannelOrderConvention::Smpte2110: {
        if (channel_order.raw_value.empty()) {
            return Error::InvalidValue;
        }

        break;
    }

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

[[nodiscard]] inline Error validate_audio_stream_signaling(const AudioStreamSignaling &signaling) {
    if (Error err = validate_audio_media_description_structure(signaling.media); err != Error::Ok) {
        return err;
    }

    if (signaling.channel_order) {
        if (Error err = validate_audio_channel_order_signaling(*signaling.channel_order); err != Error::Ok) {
            return err;
        }

        if (signaling.channel_order->convention == AudioChannelOrderConvention::Smpte2110) {
            auto parsed = parse_smpte2110_audio_channel_order_raw_value_and_count(signaling.channel_order->raw_value);

            if (parsed.error != Error::Ok) {
                return parsed.error;
            }

            if (parsed.has_declared_channel_count && parsed.declared_channel_count > signaling.media.channel_count) {
                return Error::InvalidValue;
            }
        }
    }

    return Error::Ok;
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_AUDIO_SIGNALING_HPP
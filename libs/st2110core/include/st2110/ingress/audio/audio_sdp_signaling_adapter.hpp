#ifndef ST2110_OBS_PLUGIN_AUDIO_SDP_SIGNALING_ADAPTER_HPP
#define ST2110_OBS_PLUGIN_AUDIO_SDP_SIGNALING_ADAPTER_HPP

#include "st2110/foundation/error.hpp"
#include "st2110/model/audio/audio_signaling.hpp"
#include "audio_sdp_media_section.hpp"

#include <expected>
#include <string>
#include <string_view>

#include <string_view>

#include "st2110/foundation/error.hpp"
#include "st2110/model/audio/audio_channel_order.hpp"

#include <cstdint>
#include <expected>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace st2110 {

[[nodiscard]] inline bool is_audio_channel_order_digit(char c) { return c >= '0' && c <= '9'; }

[[nodiscard]] inline bool audio_channel_order_token_contains_ws(std::string_view token) {
    for (const char c : token) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            return true;
        }
    }

    return false;
}

[[nodiscard]] inline std::expected<uint16_t, Error>
parse_audio_channel_order_u_two_digit_count(std::string_view symbol) {
    if (symbol.size() != 3 || symbol[0] != 'U' || !is_audio_channel_order_digit(symbol[1]) ||
        !is_audio_channel_order_digit(symbol[2])) {
        return std::unexpected(Error::InvalidValue);
    }

    const uint16_t value =
        static_cast<uint16_t>((static_cast<uint16_t>(symbol[1] - '0') * 10U) +
                              static_cast<uint16_t>(symbol[2] - '0'));

    if (value == 0 || value > 64) {
        return std::unexpected(Error::InvalidValue);
    }

    return value;
}

[[nodiscard]] inline std::expected<AudioChannelOrderGroup, Error>
audio_channel_order_group_from_smpte2110_symbol(std::string_view symbol) {
    if (symbol.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    if (audio_channel_order_token_contains_ws(symbol)) {
        return std::unexpected(Error::InvalidValue);
    }

    if (symbol == "M") {
        return AudioChannelOrderGroup{
            .kind = AudioChannelGroupKind::Mono,
            .symbol = std::string(symbol),
            .channel_count = 1,
        };
    }

    if (symbol == "ST") {
        return AudioChannelOrderGroup{
            .kind = AudioChannelGroupKind::Stereo,
            .symbol = std::string(symbol),
            .channel_count = 2,
        };
    }

    if (symbol == "DM") {
        return AudioChannelOrderGroup{
            .kind = AudioChannelGroupKind::DualMono,
            .symbol = std::string(symbol),
            .channel_count = 2,
        };
    }

    if (symbol == "LtRt") {
        return AudioChannelOrderGroup{
            .kind = AudioChannelGroupKind::MatrixStereo,
            .symbol = std::string(symbol),
            .channel_count = 2,
        };
    }

    if (symbol == "51") {
        return AudioChannelOrderGroup{
            .kind = AudioChannelGroupKind::FiveOne,
            .symbol = std::string(symbol),
            .channel_count = 6,
        };
    }

    if (symbol == "71") {
        return AudioChannelOrderGroup{
            .kind = AudioChannelGroupKind::SevenOne,
            .symbol = std::string(symbol),
            .channel_count = 8,
        };
    }

    if (symbol == "222") {
        return AudioChannelOrderGroup{
            .kind = AudioChannelGroupKind::TwentyTwoTwo,
            .symbol = std::string(symbol),
            .channel_count = 24,
        };
    }

    if (symbol == "SGRP") {
        return AudioChannelOrderGroup{
            .kind = AudioChannelGroupKind::SdiGroup,
            .symbol = std::string(symbol),
            .channel_count = 4,
        };
    }

    if (symbol.starts_with("U")) {
        auto count = parse_audio_channel_order_u_two_digit_count(symbol);
        if (!count.has_value()) {
            return std::unexpected(count.error());
        }

        return AudioChannelOrderGroup{
            .kind = AudioChannelGroupKind::Undefined,
            .symbol = std::string(symbol),
            .channel_count = *count,
        };
    }

    return std::unexpected(Error::Unsupported);
}

[[nodiscard]] inline std::expected<AudioChannelOrder, Error>
parse_smpte2110_audio_channel_order_raw_value(std::string_view raw_value) {
    static constexpr std::string_view prefix = "SMPTE2110.";

    if (!raw_value.starts_with(prefix)) {
        return std::unexpected(Error::InvalidValue);
    }

    std::string_view groups_text = raw_value.substr(prefix.size());

    if (groups_text.size() < 3 || groups_text.front() != '(' || groups_text.back() != ')') {
        return std::unexpected(Error::InvalidValue);
    }

    groups_text.remove_prefix(1);
    groups_text.remove_suffix(1);

    if (groups_text.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    AudioChannelOrder parsed{};
    parsed.convention = AudioChannelOrderConvention::Smpte2110;
    parsed.raw_value = std::string(raw_value);

    std::size_t group_start = 0;

    while (group_start <= groups_text.size()) {
        const std::size_t separator_pos = groups_text.find(',', group_start);

        const std::string_view symbol = separator_pos == std::string_view::npos
                                            ? groups_text.substr(group_start)
                                            : groups_text.substr(group_start, separator_pos - group_start);

        if (symbol.empty()) {
            return std::unexpected(Error::InvalidValue);
        }

        auto group = audio_channel_order_group_from_smpte2110_symbol(symbol);
        if (!group.has_value()) {
            return std::unexpected(group.error());
        }

        const uint32_t next_declared_count =
            static_cast<uint32_t>(parsed.declared_channel_count) + static_cast<uint32_t>(group->channel_count);

        if (next_declared_count > std::numeric_limits<uint16_t>::max()) {
            return std::unexpected(Error::InvalidValue);
        }

        parsed.declared_channel_count = static_cast<uint16_t>(next_declared_count);
        parsed.groups.push_back(std::move(*group));

        if (separator_pos == std::string_view::npos) {
            break;
        }

        group_start = separator_pos + 1;

        if (group_start > groups_text.size()) {
            return std::unexpected(Error::InvalidValue);
        }
    }

    if (const Error err = validate_audio_channel_order(parsed); err != Error::Ok) {
        return std::unexpected(err);
    }

    return parsed;
}

[[nodiscard]] inline std::expected<AudioChannelOrder, Error>
audio_channel_order_from_raw_sdp_value(std::string_view raw_value) {
    if (raw_value.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    if (raw_value.starts_with("SMPTE2110.")) {
        return parse_smpte2110_audio_channel_order_raw_value(raw_value);
    }

    AudioChannelOrder other{};
    other.convention = AudioChannelOrderConvention::Other;
    other.raw_value = std::string(raw_value);

    if (const Error err = validate_audio_channel_order(other); err != Error::Ok) {
        return std::unexpected(err);
    }

    return other;
}

} // namespace st2110

namespace st2110 {

struct AudioChannelOrderDeclaredCountValidation {
    Error error = Error::Ok;
    uint16_t declared_channel_count = 0;
    bool has_declared_channel_count = false;
};

[[nodiscard]] inline bool is_audio_channel_order_digit(char c) { return c >= '0' && c <= '9'; }

[[nodiscard]] inline bool audio_channel_order_token_contains_ws(std::string_view token) {
    for (const char c : token) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            return true;
        }
    }

    return false;
}

[[nodiscard]] inline std::expected<uint16_t, Error>
parse_audio_channel_order_u_two_digit_count(std::string_view symbol) {
    if (symbol.size() != 3 || symbol[0] != 'U' || !is_audio_channel_order_digit(symbol[1]) ||
        !is_audio_channel_order_digit(symbol[2])) {
        return std::unexpected(Error::InvalidValue);
    }

    const uint16_t value =
        static_cast<uint16_t>((static_cast<uint16_t>(symbol[1] - '0') * 10U) +
                              static_cast<uint16_t>(symbol[2] - '0'));

    if (value == 0 || value > 64) {
        return std::unexpected(Error::InvalidValue);
    }

    return value;
}

[[nodiscard]] inline std::expected<AudioChannelOrderGroup, Error>
audio_channel_order_group_from_smpte2110_symbol(std::string_view symbol) {
    if (symbol.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    if (audio_channel_order_token_contains_ws(symbol)) {
        return std::unexpected(Error::InvalidValue);
    }

    if (symbol == "M") {
        return AudioChannelOrderGroup{
            .kind = AudioChannelGroupKind::Mono,
            .symbol = std::string(symbol),
            .channel_count = 1,
        };
    }

    if (symbol == "ST") {
        return AudioChannelOrderGroup{
            .kind = AudioChannelGroupKind::Stereo,
            .symbol = std::string(symbol),
            .channel_count = 2,
        };
    }

    if (symbol == "DM") {
        return AudioChannelOrderGroup{
            .kind = AudioChannelGroupKind::DualMono,
            .symbol = std::string(symbol),
            .channel_count = 2,
        };
    }

    if (symbol == "LtRt") {
        return AudioChannelOrderGroup{
            .kind = AudioChannelGroupKind::MatrixStereo,
            .symbol = std::string(symbol),
            .channel_count = 2,
        };
    }

    if (symbol == "51") {
        return AudioChannelOrderGroup{
            .kind = AudioChannelGroupKind::FiveOne,
            .symbol = std::string(symbol),
            .channel_count = 6,
        };
    }

    if (symbol == "71") {
        return AudioChannelOrderGroup{
            .kind = AudioChannelGroupKind::SevenOne,
            .symbol = std::string(symbol),
            .channel_count = 8,
        };
    }

    if (symbol == "222") {
        return AudioChannelOrderGroup{
            .kind = AudioChannelGroupKind::TwentyTwoTwo,
            .symbol = std::string(symbol),
            .channel_count = 24,
        };
    }

    if (symbol == "SGRP") {
        return AudioChannelOrderGroup{
            .kind = AudioChannelGroupKind::SdiGroup,
            .symbol = std::string(symbol),
            .channel_count = 4,
        };
    }

    if (symbol.starts_with("U")) {
        auto count = parse_audio_channel_order_u_two_digit_count(symbol);
        if (!count.has_value()) {
            return std::unexpected(count.error());
        }

        return AudioChannelOrderGroup{
            .kind = AudioChannelGroupKind::Undefined,
            .symbol = std::string(symbol),
            .channel_count = *count,
        };
    }

    return std::unexpected(Error::Unsupported);
}

[[nodiscard]] inline std::expected<AudioChannelOrder, Error>
parse_smpte2110_audio_channel_order_raw_value(std::string_view raw_value) {
    static constexpr std::string_view prefix = "SMPTE2110.";

    if (!raw_value.starts_with(prefix)) {
        return std::unexpected(Error::InvalidValue);
    }

    std::string_view groups_text = raw_value.substr(prefix.size());

    if (groups_text.size() < 3 || groups_text.front() != '(' || groups_text.back() != ')') {
        return std::unexpected(Error::InvalidValue);
    }

    groups_text.remove_prefix(1);
    groups_text.remove_suffix(1);

    if (groups_text.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    AudioChannelOrder parsed{};
    parsed.convention = AudioChannelOrderConvention::Smpte2110;
    parsed.raw_value = std::string(raw_value);

    std::size_t group_start = 0;

    while (group_start <= groups_text.size()) {
        const std::size_t separator_pos = groups_text.find(',', group_start);

        const std::string_view symbol = separator_pos == std::string_view::npos
                                            ? groups_text.substr(group_start)
                                            : groups_text.substr(group_start, separator_pos - group_start);

        if (symbol.empty()) {
            return std::unexpected(Error::InvalidValue);
        }

        auto group = audio_channel_order_group_from_smpte2110_symbol(symbol);
        if (!group.has_value()) {
            return std::unexpected(group.error());
        }

        const uint32_t next_declared_count =
            static_cast<uint32_t>(parsed.declared_channel_count) + static_cast<uint32_t>(group->channel_count);

        if (next_declared_count > std::numeric_limits<uint16_t>::max()) {
            return std::unexpected(Error::InvalidValue);
        }

        parsed.declared_channel_count = static_cast<uint16_t>(next_declared_count);
        parsed.groups.push_back(std::move(*group));

        if (separator_pos == std::string_view::npos) {
            break;
        }

        group_start = separator_pos + 1;

        if (group_start > groups_text.size()) {
            return std::unexpected(Error::InvalidValue);
        }
    }

    if (const Error err = validate_audio_channel_order(parsed); err != Error::Ok) {
        return std::unexpected(err);
    }

    return parsed;
}

[[nodiscard]] inline std::expected<AudioChannelOrder, Error>
audio_channel_order_from_raw_sdp_value(std::string_view raw_value) {
    if (raw_value.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    if (raw_value.starts_with("SMPTE2110.")) {
        return parse_smpte2110_audio_channel_order_raw_value(raw_value);
    }

    AudioChannelOrder other{};
    other.convention = AudioChannelOrderConvention::Other;
    other.raw_value = std::string(raw_value);

    if (const Error err = validate_audio_channel_order(other); err != Error::Ok) {
        return std::unexpected(err);
    }

    return other;
}

} // namespace st2110

namespace st2110 {
[[nodiscard]] inline char audio_sdp_ascii_lower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return static_cast<char>(c - 'A' + 'a');
    }

    return c;
}

[[nodiscard]] inline bool audio_sdp_ascii_iequals(std::string_view lhs, std::string_view rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (audio_sdp_ascii_lower(lhs[i]) != audio_sdp_ascii_lower(rhs[i])) {
            return false;
        }
    }

    return true;
}

[[nodiscard]] inline std::expected<AudioPcmBitDepth, Error>
audio_pcm_bit_depth_from_raw_audio_sdp_rtpmap_encoding_name(std::string_view encoding_name) {
    if (encoding_name.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    if (audio_sdp_ascii_iequals(encoding_name, "L16")) {
        return AudioPcmBitDepth::Bits16;
    }

    if (audio_sdp_ascii_iequals(encoding_name, "L24")) {
        return AudioPcmBitDepth::Bits24;
    }

    return std::unexpected(Error::Unsupported);
}

[[nodiscard]] inline std::expected<AudioPcmEncoding, Error>
audio_pcm_encoding_from_raw_audio_sdp_rtpmap_encoding_name(std::string_view encoding_name) {
    if (encoding_name.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    if (audio_sdp_ascii_iequals(encoding_name, "L24") || audio_sdp_ascii_iequals(encoding_name, "L16")) {
        return AudioPcmEncoding::LinearPcm;
    }

    return std::unexpected(Error::Unsupported);
}

[[nodiscard]] inline AudioChannelOrderSignaling
audio_channel_order_signaling_from_raw_audio_sdp_value(std::string_view value) {
    AudioChannelOrderSignaling res{};

    if (value.starts_with("SMPTE2110.")) {
        res.convention = AudioChannelOrderConvention::Smpte2110;
    } else {
        res.convention = AudioChannelOrderConvention::Other;
    }

    res.raw_value = std::string(value);
    return res;
}

[[nodiscard]] inline std::expected<AudioStreamSignaling, Error>
audio_stream_signaling_from_raw_audio_sdp_media_section(const RawAudioSdpMediaSection &raw) {
    if (!contains_audio_payload_type(raw.media_payload_types, raw.payload_type)) {
        return std::unexpected(Error::InvalidValue);
    }

    if (raw.rtpmap.empty() || raw.parsed_rtpmap.encoding_name.empty() || raw.parsed_rtpmap.sampling_rate_hz == 0) {
        return std::unexpected(Error::InvalidValue);
    }

    if (!raw.parsed_rtpmap.channel_count.has_value()) {
        return std::unexpected(Error::InvalidValue);
    }

    if (!raw.packet_time_us.has_value()) {
        return std::unexpected(Error::InvalidValue);
    }

    auto encoding = audio_pcm_encoding_from_raw_audio_sdp_rtpmap_encoding_name(raw.parsed_rtpmap.encoding_name);

    if (!encoding.has_value()) {
        return std::unexpected(encoding.error());
    }

    auto bit_depth = audio_pcm_bit_depth_from_raw_audio_sdp_rtpmap_encoding_name(raw.parsed_rtpmap.encoding_name);
    if (!bit_depth.has_value()) {
        return std::unexpected(bit_depth.error());
    }

    AudioStreamSignaling signaling{};
    signaling.media.pcm_encoding = *encoding;
    signaling.media.pcm_bit_depth = *bit_depth;
    signaling.media.sampling_rate_hz = raw.parsed_rtpmap.sampling_rate_hz;
    signaling.media.packet_time_us = *raw.packet_time_us;
    signaling.media.channel_count = *raw.parsed_rtpmap.channel_count;

    if (raw.channel_order.has_value()) {
        if (raw.channel_order->empty()) {
            return std::unexpected(Error::InvalidValue);
        }

        signaling.channel_order = audio_channel_order_signaling_from_raw_audio_sdp_value(*raw.channel_order);
    }

    if (Error err = validate_audio_stream_signaling(signaling); err != Error::Ok) {
        return std::unexpected(err);
    }

    return signaling;
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_AUDIO_SDP_SIGNALING_ADAPTER_HPP

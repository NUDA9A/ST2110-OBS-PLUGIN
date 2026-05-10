#ifndef ST2110_OBS_PLUGIN_AUDIO_SDP_PARSE_HPP
#define ST2110_OBS_PLUGIN_AUDIO_SDP_PARSE_HPP

#include "st2110/foundation/error.hpp"
#include "st2110/ingress/shared/sdp_common.hpp"
#include "st2110/model/audio/audio_signaling.hpp"

#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace st2110 {

struct RawAudioSdpParseRtpMap {
    std::uint8_t payload_type = 0;
    std::string encoding_name{};
    std::uint32_t sampling_rate_hz = 0;
    std::optional<std::uint16_t> channel_count{};
};

struct RawAudioSdpParseFmtpToken {
    std::string_view name{};
    std::optional<std::string_view> value{};
};

[[nodiscard]] inline std::expected<std::vector<std::uint8_t>, Error>
parse_audio_sdp_parse_media_line_payload_types(std::string_view media_value) {
    media_value = trim_ws(strip_cr(media_value));

    const auto tokens = split_ws(media_value);
    if (tokens.size() < 4) {
        return std::unexpected(Error::InvalidValue);
    }

    if (tokens[0] != "audio") {
        return std::unexpected(Error::InvalidValue);
    }

    if (tokens[2] != "RTP/AVP") {
        return std::unexpected(Error::InvalidValue);
    }

    std::vector<std::uint8_t> payload_types{};
    payload_types.reserve(tokens.size() - 3);

    for (std::size_t i = 3; i < tokens.size(); ++i) {
        const auto payload_type = parse_payload_type(tokens[i]);
        if (!payload_type.has_value()) {
            return std::unexpected(Error::InvalidValue);
        }

        if (*payload_type < 96 || *payload_type > 127) {
            return std::unexpected(Error::InvalidValue);
        }

        payload_types.push_back(*payload_type);
    }

    return payload_types;
}

[[nodiscard]] inline bool audio_sdp_parse_media_line_contains_payload_type(
    const std::vector<std::uint8_t> &payload_types, const std::uint8_t expected_payload_type) {
    for (const std::uint8_t payload_type : payload_types) {
        if (payload_type == expected_payload_type) {
            return true;
        }
    }

    return false;
}

[[nodiscard]] inline std::expected<const RawSdpMediaSectionLines *, Error>
select_raw_audio_sdp_parse_media_section(const RawSdpDocument &raw_sdp, const std::uint8_t expected_payload_type) {
    for (const RawSdpMediaSectionLines &media : raw_sdp.media_sections) {
        auto payload_types = parse_audio_sdp_parse_media_line_payload_types(media.media_value);
        if (!payload_types.has_value()) {
            continue;
        }

        if (audio_sdp_parse_media_line_contains_payload_type(*payload_types, expected_payload_type)) {
            return &media;
        }
    }

    return std::unexpected(Error::InvalidValue);
}

[[nodiscard]] inline std::expected<RawAudioSdpParseRtpMap, Error>
parse_audio_sdp_parse_rtpmap_payload(std::string_view raw_rtpmap) {
    raw_rtpmap = trim_ws(strip_cr(raw_rtpmap));
    if (raw_rtpmap.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    const std::size_t pt_end = raw_rtpmap.find_first_of(" \t");
    if (pt_end == std::string_view::npos) {
        return std::unexpected(Error::InvalidValue);
    }

    const std::string_view pt_text = raw_rtpmap.substr(0, pt_end);
    const auto payload_type = parse_payload_type(pt_text);
    if (!payload_type.has_value()) {
        return std::unexpected(Error::InvalidValue);
    }

    std::string_view payload = raw_rtpmap.substr(pt_end);
    payload = trim_left_ws(payload);
    if (payload.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    const std::size_t first_slash = payload.find('/');
    if (first_slash == std::string_view::npos || first_slash == 0) {
        return std::unexpected(Error::InvalidValue);
    }

    RawAudioSdpParseRtpMap out{};
    out.payload_type = *payload_type;
    out.encoding_name = std::string(payload.substr(0, first_slash));

    payload.remove_prefix(first_slash + 1);
    if (payload.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    const std::size_t second_slash = payload.find('/');
    const std::string_view sampling_rate_text =
        second_slash == std::string_view::npos ? payload : payload.substr(0, second_slash);

    auto parsed_sampling_rate = parse_sdp_numeric_value<std::uint32_t>(sampling_rate_text);
    if (!parsed_sampling_rate.has_value() || *parsed_sampling_rate == 0) {
        return std::unexpected(Error::InvalidValue);
    }

    out.sampling_rate_hz = *parsed_sampling_rate;

    if (second_slash == std::string_view::npos) {
        return out;
    }

    payload.remove_prefix(second_slash + 1);
    if (payload.empty() || payload.find('/') != std::string_view::npos) {
        return std::unexpected(Error::InvalidValue);
    }

    auto parsed_channel_count = parse_sdp_numeric_value<std::uint16_t>(payload);
    if (!parsed_channel_count.has_value() || *parsed_channel_count == 0) {
        return std::unexpected(Error::InvalidValue);
    }

    out.channel_count = *parsed_channel_count;
    return out;
}

[[nodiscard]] inline std::expected<std::uint32_t, Error>
parse_audio_sdp_parse_ptime_us(std::string_view value) {
    value = trim_ws(strip_cr(value));
    if (value.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    const std::size_t dot_pos = value.find('.');
    if (dot_pos != std::string_view::npos && value.find('.', dot_pos + 1) != std::string_view::npos) {
        return std::unexpected(Error::InvalidValue);
    }

    const std::string_view integer_ms_text = dot_pos == std::string_view::npos ? value : value.substr(0, dot_pos);
    if (integer_ms_text.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    auto integer_ms = parse_sdp_numeric_value<std::uint64_t>(integer_ms_text);
    if (!integer_ms.has_value()) {
        return std::unexpected(integer_ms.error());
    }

    if (*integer_ms > std::numeric_limits<std::uint32_t>::max() / 1000ULL) {
        return std::unexpected(Error::InvalidValue);
    }

    std::uint64_t packet_time_us = *integer_ms * 1000ULL;

    if (dot_pos != std::string_view::npos) {
        std::string_view fractional_ms_text = value.substr(dot_pos + 1);
        if (fractional_ms_text.empty()) {
            return std::unexpected(Error::InvalidValue);
        }

        while (fractional_ms_text.size() > 3 && fractional_ms_text.back() == '0') {
            fractional_ms_text.remove_suffix(1);
        }

        if (fractional_ms_text.size() > 3) {
            return std::unexpected(Error::InvalidValue);
        }

        for (const char c : fractional_ms_text) {
            if (c < '0' || c > '9') {
                return std::unexpected(Error::InvalidValue);
            }
        }

        std::uint32_t fractional_us = 0;
        for (const char c : fractional_ms_text) {
            fractional_us = static_cast<std::uint32_t>(fractional_us * 10U + static_cast<std::uint32_t>(c - '0'));
        }

        for (std::size_t i = fractional_ms_text.size(); i < 3; ++i) {
            fractional_us *= 10U;
        }

        packet_time_us += fractional_us;
    }

    if (packet_time_us == 0 || packet_time_us > std::numeric_limits<std::uint32_t>::max()) {
        return std::unexpected(Error::InvalidValue);
    }

    return static_cast<std::uint32_t>(packet_time_us);
}

[[nodiscard]] inline std::expected<RawAudioSdpParseFmtpToken, Error>
parse_audio_sdp_parse_fmtp_token(std::string_view parameter) {
    parameter = trim_ws(parameter);
    if (parameter.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    const std::size_t eq_pos = parameter.find('=');
    if (eq_pos == std::string_view::npos) {
        return RawAudioSdpParseFmtpToken{
            .name = parameter,
            .value = std::nullopt,
        };
    }

    const std::string_view name = trim_ws(parameter.substr(0, eq_pos));
    const std::string_view value = trim_ws(parameter.substr(eq_pos + 1));

    if (name.empty() || value.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    return RawAudioSdpParseFmtpToken{
        .name = name,
        .value = value,
    };
}

[[nodiscard]] inline bool is_audio_sdp_parse_channel_order_digit(const char c) {
    return c >= '0' && c <= '9';
}

[[nodiscard]] inline bool audio_sdp_parse_channel_order_token_contains_ws(std::string_view token) {
    for (const char c : token) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            return true;
        }
    }

    return false;
}

[[nodiscard]] inline std::expected<std::uint16_t, Error>
parse_audio_sdp_parse_channel_order_u_two_digit_count(std::string_view symbol) {
    if (symbol.size() != 3 || symbol[0] != 'U' || !is_audio_sdp_parse_channel_order_digit(symbol[1]) ||
        !is_audio_sdp_parse_channel_order_digit(symbol[2])) {
        return std::unexpected(Error::InvalidValue);
    }

    const std::uint16_t value =
        static_cast<std::uint16_t>((static_cast<std::uint16_t>(symbol[1] - '0') * 10U) +
                                   static_cast<std::uint16_t>(symbol[2] - '0'));

    if (value == 0 || value > 64) {
        return std::unexpected(Error::InvalidValue);
    }

    return value;
}

[[nodiscard]] inline std::expected<AudioChannelOrderGroup, Error>
parse_audio_sdp_parse_channel_order_group_from_smpte2110_symbol(std::string_view symbol) {
    if (symbol.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    if (audio_sdp_parse_channel_order_token_contains_ws(symbol)) {
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
        auto count = parse_audio_sdp_parse_channel_order_u_two_digit_count(symbol);
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
parse_audio_sdp_parse_smpte2110_channel_order(std::string_view raw_value) {
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

        auto group = parse_audio_sdp_parse_channel_order_group_from_smpte2110_symbol(symbol);
        if (!group.has_value()) {
            return std::unexpected(group.error());
        }

        const std::uint32_t next_declared_count =
            static_cast<std::uint32_t>(parsed.declared_channel_count) + static_cast<std::uint32_t>(group->channel_count);

        if (next_declared_count > std::numeric_limits<std::uint16_t>::max()) {
            return std::unexpected(Error::InvalidValue);
        }

        parsed.declared_channel_count = static_cast<std::uint16_t>(next_declared_count);
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
parse_audio_sdp_parse_channel_order(std::string_view raw_value) {
    raw_value = trim_ws(raw_value);
    if (raw_value.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    if (raw_value.starts_with("SMPTE2110.")) {
        return parse_audio_sdp_parse_smpte2110_channel_order(raw_value);
    }

    AudioChannelOrder other{};
    other.convention = AudioChannelOrderConvention::Other;
    other.raw_value = std::string(raw_value);

    if (const Error err = validate_audio_channel_order(other); err != Error::Ok) {
        return std::unexpected(err);
    }

    return other;
}

[[nodiscard]] inline char audio_sdp_parse_ascii_lower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return static_cast<char>(c - 'A' + 'a');
    }

    return c;
}

[[nodiscard]] inline bool audio_sdp_parse_ascii_iequals(std::string_view lhs, std::string_view rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (audio_sdp_parse_ascii_lower(lhs[i]) != audio_sdp_parse_ascii_lower(rhs[i])) {
            return false;
        }
    }

    return true;
}

[[nodiscard]] inline std::expected<AudioPcmBitDepth, Error>
audio_pcm_bit_depth_from_raw_audio_sdp_parse_rtpmap_encoding_name(std::string_view encoding_name) {
    if (encoding_name.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    if (audio_sdp_parse_ascii_iequals(encoding_name, "L16")) {
        return AudioPcmBitDepth::Bits16;
    }

    if (audio_sdp_parse_ascii_iequals(encoding_name, "L24")) {
        return AudioPcmBitDepth::Bits24;
    }

    return std::unexpected(Error::Unsupported);
}

[[nodiscard]] inline std::expected<AudioPcmEncoding, Error>
audio_pcm_encoding_from_raw_audio_sdp_parse_rtpmap_encoding_name(std::string_view encoding_name) {
    if (encoding_name.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    if (audio_sdp_parse_ascii_iequals(encoding_name, "L16") ||
        audio_sdp_parse_ascii_iequals(encoding_name, "L24")) {
        return AudioPcmEncoding::LinearPcm;
    }

    return std::unexpected(Error::Unsupported);
}

[[nodiscard]] inline Error
apply_audio_media_specific_fmtp_to_signaling(const std::vector<std::string> &parameters,
                                             AudioStreamSignaling &signaling) {
    bool have_channel_order = false;

    for (const std::string &parameter_text : parameters) {
        auto parsed_token = parse_audio_sdp_parse_fmtp_token(parameter_text);
        if (!parsed_token.has_value()) {
            return parsed_token.error();
        }

        const RawAudioSdpParseFmtpToken token = *parsed_token;

        if (token.name == "channel-order") {
            if (have_channel_order || !token.value.has_value()) {
                return Error::InvalidValue;
            }

            auto parsed_channel_order = parse_audio_sdp_parse_channel_order(*token.value);
            if (!parsed_channel_order.has_value()) {
                return parsed_channel_order.error();
            }

            signaling.channel_order = std::move(*parsed_channel_order);
            have_channel_order = true;
            continue;
        }
    }

    return Error::Ok;
}

[[nodiscard]] inline std::expected<AudioStreamSignaling, Error>
parse_audio_stream_signaling(const RawSdpDocument &raw_sdp, const std::uint8_t expected_payload_type) {
    auto selected_media = select_raw_audio_sdp_parse_media_section(raw_sdp, expected_payload_type);
    if (!selected_media.has_value()) {
        return std::unexpected(selected_media.error());
    }

    const RawSdpMediaSectionLines &media = **selected_media;

    if (media.rtpmap.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    if (!media.ptime.has_value()) {
        return std::unexpected(Error::InvalidValue);
    }

    auto raw_rtpmap = parse_audio_sdp_parse_rtpmap_payload(media.rtpmap);
    if (!raw_rtpmap.has_value()) {
        return std::unexpected(raw_rtpmap.error());
    }

    auto parsed_ptime_us = parse_audio_sdp_parse_ptime_us(*media.ptime);
    if (!parsed_ptime_us.has_value()) {
        return std::unexpected(parsed_ptime_us.error());
    }

    auto parsed_timing = parse_stream_timing_signaling(raw_sdp.session, media, raw_rtpmap->sampling_rate_hz);
    if (!parsed_timing.has_value()) {
        return std::unexpected(parsed_timing.error());
    }

    auto parsed_transport = parse_stream_transport_signaling(raw_sdp.session, media);
    if (!parsed_transport.has_value()) {
        return std::unexpected(parsed_transport.error());
    }

    auto parsed_encoding =
        audio_pcm_encoding_from_raw_audio_sdp_parse_rtpmap_encoding_name(raw_rtpmap->encoding_name);
    if (!parsed_encoding.has_value()) {
        return std::unexpected(parsed_encoding.error());
    }

    auto parsed_bit_depth =
        audio_pcm_bit_depth_from_raw_audio_sdp_parse_rtpmap_encoding_name(raw_rtpmap->encoding_name);
    if (!parsed_bit_depth.has_value()) {
        return std::unexpected(parsed_bit_depth.error());
    }

    AudioStreamSignaling signaling{};
    signaling.media.pcm_encoding = *parsed_encoding;
    signaling.media.pcm_bit_depth = *parsed_bit_depth;
    signaling.media.sampling_rate_hz = raw_rtpmap->sampling_rate_hz;
    signaling.media.packet_time_us = *parsed_ptime_us;
    signaling.media.channel_count = *raw_rtpmap->channel_count;
    signaling.timing = std::move(*parsed_timing);
    signaling.transport = std::move(*parsed_transport);

    if (const Error err =
            apply_audio_media_specific_fmtp_to_signaling(media.fmtp_media_specific_parameters, signaling);
        err != Error::Ok) {
        return std::unexpected(err);
    }

    if (const Error err = validate_audio_stream_signaling(signaling); err != Error::Ok) {
        return std::unexpected(err);
    }

    return signaling;
}

[[nodiscard]] inline std::expected<AudioStreamSignaling, Error>
parse_audio_stream_signaling(std::string_view sdp, const std::uint8_t expected_payload_type) {
    auto raw_sdp = parse_raw_sdp_document(sdp);
    if (!raw_sdp.has_value()) {
        return std::unexpected(raw_sdp.error());
    }

    return parse_audio_stream_signaling(*raw_sdp, expected_payload_type);
}

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_AUDIO_SDP_PARSE_HPP
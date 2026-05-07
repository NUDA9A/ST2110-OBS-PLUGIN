#ifndef ST2110_OBS_PLUGIN_AUDIO_SDP_MEDIA_SECTION_HPP
#define ST2110_OBS_PLUGIN_AUDIO_SDP_MEDIA_SECTION_HPP

#include "st2110/foundation/error.hpp"

#include <charconv>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace st2110 {
struct RawAudioSdpAttribute {
    std::string name{};
    std::string value{};
};

struct RawAudioSdpRtpMap {
    std::string encoding_name{};
    uint32_t sampling_rate_hz = 0;
    std::optional<uint16_t> channel_count{};
};

struct RawAudioSdpFmtpParameters {
    std::optional<std::string> channel_order{};
    std::vector<RawAudioSdpAttribute> unknown_parameters{};
};

struct RawAudioSdpMediaSection {
    std::string media_line{};
    uint8_t payload_type = 0;
    std::vector<uint8_t> media_payload_types{};

    std::string rtpmap{};
    RawAudioSdpRtpMap parsed_rtpmap{};

    std::string fmtp{};
    RawAudioSdpFmtpParameters parsed_fmtp{};

    std::optional<uint32_t> packet_time_us{};
    std::optional<std::string> channel_order{};

    std::vector<RawAudioSdpAttribute> unknown_session_attributes{};
    std::vector<RawAudioSdpAttribute> unknown_attributes{};
};

[[nodiscard]] inline std::string_view strip_audio_sdp_cr(std::string_view line) {
    if (!line.empty() && line.back() == '\r') {
        line.remove_suffix(1);
    }

    return line;
}

[[nodiscard]] inline bool is_audio_sdp_ascii_ws(char c) { return c == ' ' || c == '\t'; }

[[nodiscard]] inline std::string_view trim_audio_sdp_ascii_ws(std::string_view text) {
    while (!text.empty() && is_audio_sdp_ascii_ws(text.front())) {
        text.remove_prefix(1);
    }

    while (!text.empty() && is_audio_sdp_ascii_ws(text.back())) {
        text.remove_suffix(1);
    }

    return text;
}

[[nodiscard]] inline std::string_view trim_audio_sdp_left_ws(std::string_view text) {
    while (!text.empty() && is_audio_sdp_ascii_ws(text.front())) {
        text.remove_prefix(1);
    }

    return text;
}

[[nodiscard]] inline std::vector<std::string_view> split_audio_sdp_ws(std::string_view line) {
    std::vector<std::string_view> result{};

    std::size_t pos = 0;

    while (pos < line.size()) {
        while (pos < line.size() && is_audio_sdp_ascii_ws(line[pos])) {
            ++pos;
        }

        if (pos >= line.size()) {
            break;
        }

        const std::size_t token_start = pos;

        while (pos < line.size() && !is_audio_sdp_ascii_ws(line[pos])) {
            ++pos;
        }

        result.emplace_back(line.substr(token_start, pos - token_start));
    }

    return result;
}

[[nodiscard]] inline std::expected<uint64_t, Error> parse_audio_sdp_uint64(std::string_view text) {
    if (text.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    uint64_t value = 0;

    const char *first = text.data();
    const char *last = text.data() + text.size();

    const auto [ptr, ec] = std::from_chars(first, last, value);

    if (ec != std::errc{} || ptr != last) {
        return std::unexpected(Error::InvalidValue);
    }

    return value;
}

[[nodiscard]] inline std::optional<uint8_t> parse_audio_payload_type(std::string_view text) {
    auto parsed = parse_audio_sdp_uint64(text);

    if (!parsed.has_value() || *parsed > 127) {
        return std::nullopt;
    }

    return static_cast<uint8_t>(*parsed);
}

[[nodiscard]] inline std::expected<uint16_t, Error> parse_audio_sdp_media_port_token(std::string_view token) {
    if (token.empty() || token.find('/') != std::string_view::npos) {
        return std::unexpected(Error::InvalidValue);
    }

    unsigned value = 0;

    const char *first = token.data();
    const char *last = token.data() + token.size();

    const auto [ptr, ec] = std::from_chars(first, last, value);

    if (ec != std::errc{} || ptr != last || value == 0 || value > std::numeric_limits<uint16_t>::max()) {
        return std::unexpected(Error::InvalidValue);
    }

    return static_cast<uint16_t>(value);
}

[[nodiscard]] inline Error validate_audio_sdp_media_protocol_token(std::string_view token) {
    return token == "RTP/AVP" ? Error::Ok : Error::InvalidValue;
}

[[nodiscard]] inline Error validate_audio_sdp_media_payload_type(uint8_t payload_type) {
    return payload_type >= 96 && payload_type <= 127 ? Error::Ok : Error::InvalidValue;
}

[[nodiscard]] inline std::expected<std::vector<uint8_t>, Error>
parse_audio_m_line_payload_types(std::string_view line) {
    line = strip_audio_sdp_cr(line);

    if (!line.starts_with("m=audio")) {
        return std::unexpected(Error::InvalidValue);
    }

    const auto tokens = split_audio_sdp_ws(line);

    // m=<media> <port> <proto> <payload-type>...
    if (tokens.size() < 4) {
        return std::unexpected(Error::InvalidValue);
    }

    if (tokens[0] != "m=audio") {
        return std::unexpected(Error::InvalidValue);
    }

    if (auto parsed_port = parse_audio_sdp_media_port_token(tokens[1]); !parsed_port.has_value()) {
        return std::unexpected(parsed_port.error());
    }

    if (Error err = validate_audio_sdp_media_protocol_token(tokens[2]); err != Error::Ok) {
        return std::unexpected(err);
    }

    std::vector<uint8_t> payload_types{};
    payload_types.reserve(tokens.size() - 3);

    for (std::size_t i = 3; i < tokens.size(); ++i) {
        const auto payload_type = parse_audio_payload_type(tokens[i]);

        if (!payload_type.has_value()) {
            return std::unexpected(Error::InvalidValue);
        }

        if (Error err = validate_audio_sdp_media_payload_type(*payload_type); err != Error::Ok) {
            return std::unexpected(err);
        }

        payload_types.push_back(*payload_type);
    }

    return payload_types;
}

[[nodiscard]] inline bool contains_audio_payload_type(const std::vector<uint8_t> &payload_types,
                                                      uint8_t expected_payload_type) {
    for (const uint8_t payload_type : payload_types) {
        if (payload_type == expected_payload_type) {
            return true;
        }
    }

    return false;
}

[[nodiscard]] inline std::optional<std::string_view> parse_audio_attribute_value(std::string_view line,
                                                                                 std::string_view prefix) {
    line = strip_audio_sdp_cr(line);

    if (!line.starts_with(prefix)) {
        return std::nullopt;
    }

    return line.substr(prefix.size());
}

[[nodiscard]] inline std::expected<std::optional<std::string_view>, Error>
parse_audio_payload_bound_attribute_value(std::string_view line, std::string_view prefix,
                                          uint8_t expected_payload_type) {
    line = strip_audio_sdp_cr(line);

    if (!line.starts_with(prefix)) {
        return std::optional<std::string_view>{};
    }

    std::string_view tail = line.substr(prefix.size());

    const std::size_t pt_end = tail.find_first_of(" \t");
    const std::string_view pt_text = pt_end == std::string_view::npos ? tail : tail.substr(0, pt_end);

    const auto payload_type = parse_audio_payload_type(pt_text);

    if (!payload_type.has_value()) {
        return std::unexpected(Error::InvalidValue);
    }

    if (*payload_type != expected_payload_type) {
        return std::optional<std::string_view>{};
    }

    if (pt_end == std::string_view::npos) {
        return std::unexpected(Error::InvalidValue);
    }

    tail.remove_prefix(pt_end);

    while (!tail.empty() && is_audio_sdp_ascii_ws(tail.front())) {
        tail.remove_prefix(1);
    }

    if (tail.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    return tail;
}

[[nodiscard]] inline RawAudioSdpAttribute parse_unknown_audio_sdp_attribute(std::string_view line) {
    line = strip_audio_sdp_cr(line);

    if (line.starts_with("a=")) {
        line.remove_prefix(2);

        const std::size_t colon_pos = line.find(':');

        if (colon_pos == std::string_view::npos) {
            return RawAudioSdpAttribute{.name = std::string(line), .value = ""};
        }

        return RawAudioSdpAttribute{.name = std::string(line.substr(0, colon_pos)),
                                    .value = std::string(line.substr(colon_pos + 1))};
    }

    if (line.starts_with("c=")) {
        return RawAudioSdpAttribute{.name = "c", .value = std::string(line.substr(2))};
    }

    const std::size_t eq_pos = line.find('=');

    if (eq_pos == std::string_view::npos) {
        return RawAudioSdpAttribute{.name = std::string(line), .value = ""};
    }

    return RawAudioSdpAttribute{.name = std::string(line.substr(0, eq_pos)),
                                .value = std::string(line.substr(eq_pos + 1))};
}

[[nodiscard]] inline std::expected<RawAudioSdpRtpMap, Error> parse_audio_sdp_rtpmap_payload(std::string_view payload) {
    payload = trim_audio_sdp_ascii_ws(payload);

    if (payload.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    const std::size_t first_slash = payload.find('/');

    if (first_slash == std::string_view::npos || first_slash == 0) {
        return std::unexpected(Error::InvalidValue);
    }

    RawAudioSdpRtpMap res{};
    res.encoding_name = std::string(payload.substr(0, first_slash));

    payload.remove_prefix(first_slash + 1);

    if (payload.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    const std::size_t second_slash = payload.find('/');

    const std::string_view sampling_rate_text =
        second_slash == std::string_view::npos ? payload : payload.substr(0, second_slash);

    auto sampling_rate = parse_audio_sdp_uint64(sampling_rate_text);

    if (!sampling_rate.has_value() || *sampling_rate == 0 || *sampling_rate > std::numeric_limits<uint32_t>::max()) {
        return std::unexpected(Error::InvalidValue);
    }

    res.sampling_rate_hz = static_cast<uint32_t>(*sampling_rate);

    if (second_slash == std::string_view::npos) {
        return res;
    }

    payload.remove_prefix(second_slash + 1);

    if (payload.empty() || payload.find('/') != std::string_view::npos) {
        return std::unexpected(Error::InvalidValue);
    }

    auto channel_count = parse_audio_sdp_uint64(payload);

    if (!channel_count.has_value() || *channel_count == 0 || *channel_count > std::numeric_limits<uint16_t>::max()) {
        return std::unexpected(Error::InvalidValue);
    }

    res.channel_count = static_cast<uint16_t>(*channel_count);

    return res;
}

[[nodiscard]] inline std::expected<uint32_t, Error> parse_audio_sdp_ptime_us(std::string_view value) {
    value = trim_audio_sdp_ascii_ws(strip_audio_sdp_cr(value));

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

    auto integer_ms = parse_audio_sdp_uint64(integer_ms_text);

    if (!integer_ms.has_value()) {
        return std::unexpected(integer_ms.error());
    }

    if (*integer_ms > std::numeric_limits<uint32_t>::max() / 1000ULL) {
        return std::unexpected(Error::InvalidValue);
    }

    uint64_t packet_time_us = *integer_ms * 1000ULL;

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

        uint32_t fractional_us = 0;
        for (const char c : fractional_ms_text) {
            fractional_us = static_cast<uint32_t>(fractional_us * 10U + static_cast<uint32_t>(c - '0'));
        }

        for (std::size_t i = fractional_ms_text.size(); i < 3; ++i) {
            fractional_us *= 10U;
        }

        packet_time_us += fractional_us;
    }

    if (packet_time_us == 0 || packet_time_us > std::numeric_limits<uint32_t>::max()) {
        return std::unexpected(Error::InvalidValue);
    }

    return static_cast<uint32_t>(packet_time_us);
}

struct RawAudioSdpFmtpParameterToken {
    std::string_view name{};
    std::string_view value{};
    bool has_value = false;
};

[[nodiscard]] inline bool audio_sdp_fmtp_token_contains_ws(std::string_view text) {
    for (const char c : text) {
        if (is_audio_sdp_ascii_ws(c)) {
            return true;
        }
    }

    return false;
}

[[nodiscard]] inline std::expected<std::vector<std::string_view>, Error>
split_audio_sdp_fmtp_parameters(std::string_view payload) {
    payload = trim_audio_sdp_ascii_ws(strip_audio_sdp_cr(payload));

    if (payload.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    std::vector<std::string_view> parts{};

    std::size_t part_start = 0;

    while (part_start <= payload.size()) {
        const std::size_t separator_pos = payload.find(';', part_start);

        const std::string_view raw_part = separator_pos == std::string_view::npos
                                              ? payload.substr(part_start)
                                              : payload.substr(part_start, separator_pos - part_start);

        const std::string_view part = trim_audio_sdp_ascii_ws(raw_part);

        if (part.empty()) {
            return std::unexpected(Error::InvalidValue);
        }

        parts.push_back(part);

        if (separator_pos == std::string_view::npos) {
            break;
        }

        part_start = separator_pos + 1;

        if (part_start > payload.size()) {
            return std::unexpected(Error::InvalidValue);
        }
    }

    return parts;
}

[[nodiscard]] inline std::expected<RawAudioSdpFmtpParameterToken, Error>
parse_audio_sdp_fmtp_parameter_token(std::string_view parameter) {
    parameter = trim_audio_sdp_ascii_ws(parameter);

    if (parameter.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    const std::size_t eq_pos = parameter.find('=');

    if (eq_pos == std::string_view::npos) {
        if (audio_sdp_fmtp_token_contains_ws(parameter)) {
            return std::unexpected(Error::InvalidValue);
        }

        return RawAudioSdpFmtpParameterToken{.name = parameter, .value = {}, .has_value = false};
    }

    if (eq_pos == 0 || eq_pos + 1 >= parameter.size()) {
        return std::unexpected(Error::InvalidValue);
    }

    if (parameter.find('=', eq_pos + 1) != std::string_view::npos) {
        return std::unexpected(Error::InvalidValue);
    }

    const std::string_view name = parameter.substr(0, eq_pos);
    const std::string_view value = parameter.substr(eq_pos + 1);

    if (name.empty() || value.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    if (audio_sdp_fmtp_token_contains_ws(name) || trim_audio_sdp_ascii_ws(value) != value) {
        return std::unexpected(Error::InvalidValue);
    }

    return RawAudioSdpFmtpParameterToken{.name = name, .value = value, .has_value = true};
}

[[nodiscard]] inline std::expected<RawAudioSdpFmtpParameters, Error>
parse_audio_sdp_fmtp_payload(std::string_view payload) {
    RawAudioSdpFmtpParameters res{};

    auto parts = split_audio_sdp_fmtp_parameters(payload);

    if (!parts.has_value()) {
        return std::unexpected(parts.error());
    }

    for (const std::string_view parameter : *parts) {
        auto token = parse_audio_sdp_fmtp_parameter_token(parameter);

        if (!token.has_value()) {
            return std::unexpected(token.error());
        }

        if (token->name == "channel-order") {
            if (!token->has_value || token->value.empty() || res.channel_order.has_value()) {
                return std::unexpected(Error::InvalidValue);
            }

            res.channel_order = std::string(token->value);
            continue;
        }

        RawAudioSdpAttribute unknown_parameter{};
        unknown_parameter.name = std::string(token->name);
        if (token->has_value) {
            unknown_parameter.value = std::string(token->value);
        }

        res.unknown_parameters.push_back(std::move(unknown_parameter));
    }

    return res;
}

[[nodiscard]] inline std::expected<RawAudioSdpMediaSection, Error>
select_raw_audio_sdp_media_section(std::string_view sdp, uint8_t expected_payload_type) {
    RawAudioSdpMediaSection res{};

    bool found = false;
    bool found_rtpmap = false;
    bool found_fmtp = false;

    bool seen_any_media_section = false;
    bool inside_selected_media_section = false;

    std::size_t line_start = 0;

    while (line_start <= sdp.size()) {
        std::size_t line_end = sdp.find('\n', line_start);

        if (line_end == std::string_view::npos) {
            line_end = sdp.size();
        }

        std::string_view line = sdp.substr(line_start, line_end - line_start);
        line = strip_audio_sdp_cr(line);

        if (line.starts_with("m=")) {
            seen_any_media_section = true;
            inside_selected_media_section = false;

            if (line.starts_with("m=audio")) {
                auto payload_types = parse_audio_m_line_payload_types(line);

                if (!payload_types.has_value()) {
                    return std::unexpected(payload_types.error());
                }

                if (contains_audio_payload_type(*payload_types, expected_payload_type)) {
                    if (found) {
                        return std::unexpected(Error::InvalidValue);
                    }

                    res.media_line = std::string(line.substr(2));
                    res.payload_type = expected_payload_type;
                    res.media_payload_types = std::move(*payload_types);

                    found = true;
                    inside_selected_media_section = true;
                }
            }
        } else if (!seen_any_media_section) {
            if (line.starts_with("a=")) {
                res.unknown_session_attributes.push_back(parse_unknown_audio_sdp_attribute(line));
            }
        } else if (inside_selected_media_section) {
            bool handled_attribute = false;

            if (line.starts_with("c=")) {
                res.unknown_attributes.push_back(parse_unknown_audio_sdp_attribute(line));
                handled_attribute = true;
            }

            auto rtpmap = parse_audio_payload_bound_attribute_value(line, "a=rtpmap:", expected_payload_type);

            if (!rtpmap.has_value()) {
                return std::unexpected(rtpmap.error());
            }

            if (rtpmap->has_value()) {
                if (found_rtpmap) {
                    return std::unexpected(Error::InvalidValue);
                }

                res.rtpmap = std::string(**rtpmap);

                auto parsed = parse_audio_sdp_rtpmap_payload(**rtpmap);

                if (!parsed.has_value()) {
                    return std::unexpected(parsed.error());
                }

                res.parsed_rtpmap = std::move(*parsed);

                found_rtpmap = true;
                handled_attribute = true;
            }

            auto fmtp = parse_audio_payload_bound_attribute_value(line, "a=fmtp:", expected_payload_type);

            if (!fmtp.has_value()) {
                return std::unexpected(fmtp.error());
            }

            if (fmtp->has_value()) {
                if (found_fmtp) {
                    return std::unexpected(Error::InvalidValue);
                }

                res.fmtp = std::string(**fmtp);

                auto parsed = parse_audio_sdp_fmtp_payload(**fmtp);

                if (!parsed.has_value()) {
                    return std::unexpected(parsed.error());
                }

                res.parsed_fmtp = std::move(*parsed);

                if (res.parsed_fmtp.channel_order.has_value()) {
                    res.channel_order = *res.parsed_fmtp.channel_order;
                }

                found_fmtp = true;
                handled_attribute = true;
            }

            auto ptime = parse_audio_attribute_value(line, "a=ptime:");

            if (ptime.has_value()) {
                if (res.packet_time_us.has_value()) {
                    return std::unexpected(Error::InvalidValue);
                }

                auto parsed = parse_audio_sdp_ptime_us(*ptime);

                if (!parsed.has_value()) {
                    return std::unexpected(parsed.error());
                }

                res.packet_time_us = *parsed;
                handled_attribute = true;
            }

            if (line.starts_with("a=") && !handled_attribute) {
                res.unknown_attributes.push_back(parse_unknown_audio_sdp_attribute(line));
            }
        }

        if (line_end == sdp.size()) {
            break;
        }

        line_start = line_end + 1;
    }

    if (!found || !found_rtpmap) {
        return std::unexpected(Error::InvalidValue);
    }

    return res;
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_AUDIO_SDP_MEDIA_SECTION_HPP

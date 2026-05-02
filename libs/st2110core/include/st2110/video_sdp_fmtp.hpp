//
// Created by vlaki on 24.04.2026.
//

#ifndef ST2110_OBS_PLUGIN_VIDEO_SDP_FMTP_HPP
#define ST2110_OBS_PLUGIN_VIDEO_SDP_FMTP_HPP

#include "error.hpp"
#include "video_sdp_media_section.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace st2110 {
struct RawVideoSdpExactFrameRate {
    uint32_t numerator = 0;
    uint32_t denominator = 1;
};

struct RawVideoSdpPixelAspectRatio {
    uint32_t width = 1;
    uint32_t height = 1;
};

struct RawVideoSdpFmtpUnknownParameter {
    std::string name{};
    std::optional<std::string> value{};
};

struct RawVideoSdpFmtpParameters {
    std::string sampling{};
    uint32_t width = 0;
    uint32_t height = 0;
    RawVideoSdpExactFrameRate exactframerate{};
    uint16_t depth = 0;
    bool depth_floating_point = false;
    std::string colorimetry{};
    std::string packing_mode{};
    std::string signal_standard{};
    std::optional<std::string> transfer_characteristic_system{};
    std::optional<std::string> range{};
    std::optional<RawVideoSdpPixelAspectRatio> pixel_aspect_ratio{};
    bool interlace = false;
    bool segmented = false;

    std::optional<std::string> timestamp_mode{};
    std::optional<uint64_t> ts_delay_sender_ticks{};
    std::optional<std::string> sender_type{};
    std::optional<uint32_t> troff_us{};
    std::optional<uint32_t> cmax{};
    std::optional<std::size_t> max_udp_datagram_bytes{};

    std::vector<RawVideoSdpFmtpUnknownParameter> unknown_parameters{};
};

[[nodiscard]] inline std::string_view split_part_to_string_view(auto &&part) {
    const auto size = static_cast<std::size_t>(std::ranges::distance(part));

    if (size == 0) {
        return {};
    }

    return std::string_view{std::addressof(*std::ranges::begin(part)), size};
}

[[nodiscard]] inline std::string_view trim_ascii_ws(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
        text.remove_prefix(1);
    }

    while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) {
        text.remove_suffix(1);
    }

    return text;
}

[[nodiscard]] inline std::expected<uint64_t, Error> parse_fmtp_uint64(std::string_view value) {
    value = trim_ascii_ws(value);

    if (value.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    uint64_t out = 0;
    const char *first = value.data();
    const char *last = value.data() + value.size();

    const auto [ptr, ec] = std::from_chars(first, last, out);

    if (ec != std::errc{} || ptr != last) {
        return std::unexpected(Error::InvalidValue);
    }

    return out;
}

[[nodiscard]] inline std::expected<uint32_t, Error> parse_fmtp_uint32(std::string_view value) {
    auto parsed = parse_fmtp_uint64(value);

    if (!parsed.has_value()) {
        return std::unexpected(parsed.error());
    }

    if (*parsed > std::numeric_limits<uint32_t>::max()) {
        return std::unexpected(Error::InvalidValue);
    }

    return static_cast<uint32_t>(*parsed);
}

struct RawVideoSdpFmtpParameterToken {
    std::string_view name{};
    std::optional<std::string_view> value{};
};

[[nodiscard]] inline bool is_fmtp_ascii_ws(char c) { return c == ' ' || c == '\t'; }

[[nodiscard]] inline bool contains_fmtp_ascii_ws(std::string_view text) {
    for (const char c : text) {
        if (is_fmtp_ascii_ws(c)) {
            return true;
        }
    }

    return false;
}

[[nodiscard]] inline std::expected<std::vector<std::string_view>, Error>
split_strict_video_sdp_fmtp_parameters(std::string_view payload) {
    payload = strip_cr(payload);
    payload = trim_ascii_ws(payload);

    if (payload.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    std::vector<std::string_view> parts{};

    std::size_t part_start = 0;

    while (part_start < payload.size()) {
        const std::size_t separator_pos = payload.find(';', part_start);

        const std::string_view part = separator_pos == std::string_view::npos
                                          ? payload.substr(part_start)
                                          : payload.substr(part_start, separator_pos - part_start);

        if (part.empty()) {
            return std::unexpected(Error::InvalidValue);
        }

        // Whitespace belongs to the separator grammar between parameters,
        // not inside a parameter token. This keeps "strict parse, explicit
        // fallback" and prevents silently accepting "width =1920" /
        // "width= 1920" / "sampling=x ; width=...".
        if (trim_ascii_ws(part) != part) {
            return std::unexpected(Error::InvalidValue);
        }

        parts.push_back(part);

        if (separator_pos == std::string_view::npos) {
            break;
        }

        std::size_t next_part_start = separator_pos + 1;

        // ST 2110-20 fmtp examples and the project grammar require a
        // semicolon followed by at least one WSP between parameters.
        // Reject "a=b;c=d" instead of treating it as tolerant input.
        if (next_part_start >= payload.size() || !is_fmtp_ascii_ws(payload[next_part_start])) {
            return std::unexpected(Error::InvalidValue);
        }

        while (next_part_start < payload.size() && is_fmtp_ascii_ws(payload[next_part_start])) {
            ++next_part_start;
        }

        if (next_part_start >= payload.size()) {
            return std::unexpected(Error::InvalidValue);
        }

        part_start = next_part_start;
    }

    return parts;
}

[[nodiscard]] inline std::expected<RawVideoSdpFmtpParameterToken, Error>
parse_strict_video_sdp_fmtp_parameter_token(std::string_view parameter) {
    if (parameter.empty() || contains_fmtp_ascii_ws(parameter)) {
        return std::unexpected(Error::InvalidValue);
    }

    const std::size_t eq_pos = parameter.find('=');

    if (eq_pos == std::string_view::npos) {
        return RawVideoSdpFmtpParameterToken{.name = parameter, .value = std::nullopt};
    }

    if (eq_pos == 0 || eq_pos + 1 >= parameter.size()) {
        return std::unexpected(Error::InvalidValue);
    }

    if (parameter.find('=', eq_pos + 1) != std::string_view::npos) {
        return std::unexpected(Error::InvalidValue);
    }

    return RawVideoSdpFmtpParameterToken{.name = parameter.substr(0, eq_pos), .value = parameter.substr(eq_pos + 1)};
}

[[nodiscard]] inline std::expected<std::string_view, Error>
require_fmtp_parameter_value(const RawVideoSdpFmtpParameterToken &token) {
    if (!token.value.has_value() || token.value->empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    return *token.value;
}

[[nodiscard]] inline std::expected<uint32_t, Error> parse_required_positive_fmtp_uint32(std::string_view value) {
    auto parsed = parse_fmtp_uint32(value);

    if (!parsed.has_value()) {
        return std::unexpected(parsed.error());
    }

    if (*parsed == 0) {
        return std::unexpected(Error::InvalidValue);
    }

    return *parsed;
}

[[nodiscard]] inline uint32_t gcd_u32(uint32_t a, uint32_t b) {
    while (b != 0) {
        const uint32_t rem = a % b;
        a = b;
        b = rem;
    }

    return a;
}

[[nodiscard]] inline std::expected<RawVideoSdpExactFrameRate, Error>
parse_fmtp_exact_frame_rate(std::string_view value) {
    value = trim_ascii_ws(value);

    if (value.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    const std::size_t slash_pos = value.find('/');

    if (slash_pos != std::string_view::npos && value.find('/', slash_pos + 1) != std::string_view::npos) {
        return std::unexpected(Error::InvalidValue);
    }

    const std::string_view numerator_text = slash_pos == std::string_view::npos ? value : value.substr(0, slash_pos);

    auto numerator = parse_required_positive_fmtp_uint32(numerator_text);

    if (!numerator.has_value()) {
        return std::unexpected(numerator.error());
    }

    if (slash_pos == std::string_view::npos) {
        return RawVideoSdpExactFrameRate{.numerator = *numerator, .denominator = 1};
    }

    const std::string_view denominator_text = value.substr(slash_pos + 1);

    auto denominator = parse_required_positive_fmtp_uint32(denominator_text);

    if (!denominator.has_value()) {
        return std::unexpected(denominator.error());
    }

    // Canonical SDP exactframerate form:
    // - integer frame rates use a single decimal integer, not N/1;
    // - rational frame rates use the smallest numerator/denominator representation.
    if (*denominator == 1) {
        return std::unexpected(Error::InvalidValue);
    }

    if (gcd_u32(*numerator, *denominator) != 1) {
        return std::unexpected(Error::InvalidValue);
    }

    return RawVideoSdpExactFrameRate{.numerator = *numerator, .denominator = *denominator};
}

[[nodiscard]] inline std::expected<RawVideoSdpPixelAspectRatio, Error>
parse_fmtp_pixel_aspect_ratio(std::string_view value) {
    value = trim_ascii_ws(value);

    if (value.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    const std::size_t colon_pos = value.find(':');

    if (colon_pos == std::string_view::npos || colon_pos == 0 || colon_pos + 1 >= value.size() ||
        value.find(':', colon_pos + 1) != std::string_view::npos) {
        return std::unexpected(Error::InvalidValue);
        }

    auto width = parse_required_positive_fmtp_uint32(value.substr(0, colon_pos));
    if (!width.has_value()) {
        return std::unexpected(width.error());
    }

    auto height = parse_required_positive_fmtp_uint32(value.substr(colon_pos + 1));
    if (!height.has_value()) {
        return std::unexpected(height.error());
    }

    const uint32_t divisor = gcd_u32(*width, *height);

    return RawVideoSdpPixelAspectRatio{
        .width = *width / divisor,
        .height = *height / divisor,
    };
}

struct RawVideoSdpFmtpDepthValue {
    uint16_t bits = 0;
    bool floating_point = false;
};

[[nodiscard]] inline std::expected<RawVideoSdpFmtpDepthValue, Error> parse_fmtp_depth(std::string_view value) {
    value = trim_ascii_ws(value);

    if (value.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    bool floating_point = false;

    if (value.back() == 'f') {
        floating_point = true;
        value.remove_suffix(1);

        if (value.empty()) {
            return std::unexpected(Error::InvalidValue);
        }
    }

    uint64_t bits = 0;
    const char *first = value.data();
    const char *last = value.data() + value.size();

    const auto [ptr, ec] = std::from_chars(first, last, bits);

    if (ec != std::errc{} || ptr != last || bits == 0 || bits > std::numeric_limits<uint16_t>::max()) {
        return std::unexpected(Error::InvalidValue);
    }

    // ST 2110-20 SDP uses 16f for 16-bit floating-point samples.
    // Do not accidentally accept non-standard tokens such as 8f / 10f / 12f.
    if (floating_point && bits != 16) {
        return std::unexpected(Error::InvalidValue);
    }

    return RawVideoSdpFmtpDepthValue{.bits = static_cast<uint16_t>(bits), .floating_point = floating_point};
}

[[nodiscard]] inline std::expected<std::optional<std::string_view>, Error>
parse_fmtp_attribute_payload_for_pt(std::string_view line, uint8_t expected_payload_type) {
    line = strip_cr(line);

    constexpr std::string_view prefix = "a=fmtp:";

    if (!line.starts_with(prefix)) {
        return std::optional<std::string_view>{};
    }

    std::string_view tail = line.substr(prefix.size());

    const std::size_t pt_end = tail.find_first_of(" \t");
    const std::string_view pt_text = pt_end == std::string_view::npos ? tail : tail.substr(0, pt_end);

    const auto payload_type = parse_payload_type(pt_text);

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

    while (!tail.empty() && (tail.front() == ' ' || tail.front() == '\t')) {
        tail.remove_prefix(1);
    }

    if (tail.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    return tail;
}

[[nodiscard]] inline std::expected<RawVideoSdpFmtpParameters, Error>
parse_video_sdp_fmtp_payload(std::string_view payload) {
    RawVideoSdpFmtpParameters res{};

    auto split_parts = split_strict_video_sdp_fmtp_parameters(payload);

    if (!split_parts.has_value()) {
        return std::unexpected(split_parts.error());
    }

    std::optional<std::string> sampling;
    std::optional<uint32_t> width;
    std::optional<uint32_t> height;
    std::optional<RawVideoSdpExactFrameRate> exact_framerate;
    std::optional<uint16_t> depth;
    bool depth_floating_point = false;
    std::optional<std::string> colorimetry;
    std::optional<std::string> packing_mode;
    std::optional<std::string> signal_standard;
    std::optional<std::string> tcs;
    std::optional<std::string> range;
    std::optional<RawVideoSdpPixelAspectRatio> pixel_aspect_ratio;
    std::optional<std::string> timestamp_mode;
    std::optional<uint64_t> ts_delay_sender_ticks;
    std::optional<std::string> sender_type;
    std::optional<uint32_t> troff_us;
    std::optional<uint32_t> cmax;
    std::optional<std::size_t> max_udp_datagram_bytes;
    bool interlace = false;
    bool segmented = false;

    for (const std::string_view parameter : *split_parts) {
        auto parsed_token = parse_strict_video_sdp_fmtp_parameter_token(parameter);

        if (!parsed_token.has_value()) {
            return std::unexpected(parsed_token.error());
        }

        const RawVideoSdpFmtpParameterToken token = *parsed_token;

        if (token.name == "sampling") {
            if (sampling.has_value()) {
                return std::unexpected(Error::InvalidValue);
            }

            auto value = require_fmtp_parameter_value(token);

            if (!value.has_value()) {
                return std::unexpected(value.error());
            }

            sampling = std::string(*value);
            continue;
        }

        if (token.name == "width") {
            if (width.has_value()) {
                return std::unexpected(Error::InvalidValue);
            }

            auto value = require_fmtp_parameter_value(token);

            if (!value.has_value()) {
                return std::unexpected(value.error());
            }

            auto parsed = parse_required_positive_fmtp_uint32(*value);

            if (!parsed.has_value()) {
                return std::unexpected(parsed.error());
            }

            width = *parsed;
            continue;
        }

        if (token.name == "height") {
            if (height.has_value()) {
                return std::unexpected(Error::InvalidValue);
            }

            auto value = require_fmtp_parameter_value(token);

            if (!value.has_value()) {
                return std::unexpected(value.error());
            }

            auto parsed = parse_required_positive_fmtp_uint32(*value);

            if (!parsed.has_value()) {
                return std::unexpected(parsed.error());
            }

            height = *parsed;
            continue;
        }

        if (token.name == "exactframerate") {
            if (exact_framerate.has_value()) {
                return std::unexpected(Error::InvalidValue);
            }

            auto value = require_fmtp_parameter_value(token);

            if (!value.has_value()) {
                return std::unexpected(value.error());
            }

            auto parsed = parse_fmtp_exact_frame_rate(*value);

            if (!parsed.has_value()) {
                return std::unexpected(parsed.error());
            }

            exact_framerate = *parsed;
            continue;
        }

        if (token.name == "depth") {
            if (depth.has_value()) {
                return std::unexpected(Error::InvalidValue);
            }

            auto value = require_fmtp_parameter_value(token);

            if (!value.has_value()) {
                return std::unexpected(value.error());
            }

            auto parsed_depth = parse_fmtp_depth(*value);

            if (!parsed_depth.has_value()) {
                return std::unexpected(parsed_depth.error());
            }

            depth = parsed_depth->bits;
            depth_floating_point = parsed_depth->floating_point;
            continue;
        }

        if (token.name == "colorimetry") {
            if (colorimetry.has_value()) {
                return std::unexpected(Error::InvalidValue);
            }

            auto value = require_fmtp_parameter_value(token);

            if (!value.has_value()) {
                return std::unexpected(value.error());
            }

            colorimetry = std::string(*value);
            continue;
        }

        if (token.name == "PM") {
            if (packing_mode.has_value()) {
                return std::unexpected(Error::InvalidValue);
            }

            auto value = require_fmtp_parameter_value(token);

            if (!value.has_value()) {
                return std::unexpected(value.error());
            }

            packing_mode = std::string(*value);
            continue;
        }

        if (token.name == "SSN") {
            if (signal_standard.has_value()) {
                return std::unexpected(Error::InvalidValue);
            }

            auto value = require_fmtp_parameter_value(token);

            if (!value.has_value()) {
                return std::unexpected(value.error());
            }

            signal_standard = std::string(*value);
            continue;
        }

        if (token.name == "TCS") {
            if (tcs.has_value()) {
                return std::unexpected(Error::InvalidValue);
            }

            auto value = require_fmtp_parameter_value(token);

            if (!value.has_value()) {
                return std::unexpected(value.error());
            }

            tcs = std::string(*value);
            continue;
        }

        if (token.name == "RANGE") {
            if (range.has_value()) {
                return std::unexpected(Error::InvalidValue);
            }

            auto value = require_fmtp_parameter_value(token);

            if (!value.has_value()) {
                return std::unexpected(value.error());
            }

            range = std::string(*value);
            continue;
        }

        if (token.name == "PAR") {
            if (pixel_aspect_ratio.has_value()) {
                return std::unexpected(Error::InvalidValue);
            }

            auto value = require_fmtp_parameter_value(token);

            if (!value.has_value()) {
                return std::unexpected(value.error());
            }

            auto parsed = parse_fmtp_pixel_aspect_ratio(*value);

            if (!parsed.has_value()) {
                return std::unexpected(parsed.error());
            }

            pixel_aspect_ratio = *parsed;
            continue;
        }

        if (token.name == "TSMODE") {
            if (timestamp_mode.has_value()) {
                return std::unexpected(Error::InvalidValue);
            }

            auto value = require_fmtp_parameter_value(token);

            if (!value.has_value()) {
                return std::unexpected(value.error());
            }

            timestamp_mode = std::string(*value);
            continue;
        }

        if (token.name == "TSDELAY") {
            if (ts_delay_sender_ticks.has_value()) {
                return std::unexpected(Error::InvalidValue);
            }

            auto value = require_fmtp_parameter_value(token);

            if (!value.has_value()) {
                return std::unexpected(value.error());
            }

            auto parsed = parse_fmtp_uint64(*value);

            if (!parsed.has_value()) {
                return std::unexpected(parsed.error());
            }

            ts_delay_sender_ticks = *parsed;
            continue;
        }

        if (token.name == "TP") {
            if (sender_type.has_value()) {
                return std::unexpected(Error::InvalidValue);
            }

            auto value = require_fmtp_parameter_value(token);

            if (!value.has_value()) {
                return std::unexpected(value.error());
            }

            sender_type = std::string(*value);
            continue;
        }

        if (token.name == "TROFF") {
            if (troff_us.has_value()) {
                return std::unexpected(Error::InvalidValue);
            }

            auto value = require_fmtp_parameter_value(token);

            if (!value.has_value()) {
                return std::unexpected(value.error());
            }

            auto parsed = parse_fmtp_uint32(*value);

            if (!parsed.has_value()) {
                return std::unexpected(parsed.error());
            }

            troff_us = *parsed;
            continue;
        }

        if (token.name == "CMAX") {
            if (cmax.has_value()) {
                return std::unexpected(Error::InvalidValue);
            }

            auto value = require_fmtp_parameter_value(token);

            if (!value.has_value()) {
                return std::unexpected(value.error());
            }

            auto parsed = parse_fmtp_uint32(*value);

            if (!parsed.has_value()) {
                return std::unexpected(parsed.error());
            }

            cmax = *parsed;
            continue;
        }

        if (token.name == "MAXUDP") {
            if (max_udp_datagram_bytes.has_value()) {
                return std::unexpected(Error::InvalidValue);
            }

            auto value = require_fmtp_parameter_value(token);

            if (!value.has_value()) {
                return std::unexpected(value.error());
            }

            auto parsed = parse_fmtp_uint64(*value);

            if (!parsed.has_value()) {
                return std::unexpected(parsed.error());
            }

            if (*parsed > std::numeric_limits<std::size_t>::max()) {
                return std::unexpected(Error::InvalidValue);
            }

            max_udp_datagram_bytes = static_cast<std::size_t>(*parsed);
            continue;
        }

        if (token.name == "interlace") {
            if (token.value.has_value() || interlace) {
                return std::unexpected(Error::InvalidValue);
            }

            interlace = true;
            continue;
        }

        if (token.name == "segmented") {
            if (token.value.has_value() || segmented) {
                return std::unexpected(Error::InvalidValue);
            }

            segmented = true;
            continue;
        }

        RawVideoSdpFmtpUnknownParameter unknown_parameter{};
        unknown_parameter.name = std::string(token.name);

        if (token.value.has_value()) {
            unknown_parameter.value = std::string(*token.value);
        }

        res.unknown_parameters.push_back(std::move(unknown_parameter));
    }

    if (!sampling.has_value()) {
        return std::unexpected(Error::InvalidValue);
    }
    res.sampling = std::move(*sampling);

    if (!width.has_value()) {
        return std::unexpected(Error::InvalidValue);
    }
    res.width = *width;

    if (!height.has_value()) {
        return std::unexpected(Error::InvalidValue);
    }
    res.height = *height;

    if (!exact_framerate.has_value()) {
        return std::unexpected(Error::InvalidValue);
    }
    res.exactframerate = std::move(*exact_framerate);

    if (!depth.has_value()) {
        return std::unexpected(Error::InvalidValue);
    }
    res.depth = *depth;
    res.depth_floating_point = depth_floating_point;

    if (!colorimetry.has_value()) {
        return std::unexpected(Error::InvalidValue);
    }
    res.colorimetry = std::move(*colorimetry);

    if (!packing_mode.has_value()) {
        return std::unexpected(Error::InvalidValue);
    }
    res.packing_mode = std::move(*packing_mode);

    if (!signal_standard.has_value()) {
        return std::unexpected(Error::InvalidValue);
    }
    res.signal_standard = std::move(*signal_standard);

    if (tcs.has_value()) {
        res.transfer_characteristic_system = std::move(*tcs);
    }

    if (range.has_value()) {
        res.range = std::move(*range);
    }

    if (pixel_aspect_ratio.has_value()) {
        res.pixel_aspect_ratio = *pixel_aspect_ratio;
    }

    if (timestamp_mode.has_value()) {
        res.timestamp_mode = std::move(*timestamp_mode);
    }

    if (ts_delay_sender_ticks.has_value()) {
        res.ts_delay_sender_ticks = *ts_delay_sender_ticks;
    }

    if (sender_type.has_value()) {
        res.sender_type = std::move(*sender_type);
    }

    if (troff_us.has_value()) {
        res.troff_us = *troff_us;
    }

    if (cmax.has_value()) {
        res.cmax = *cmax;
    }

    if (max_udp_datagram_bytes.has_value()) {
        res.max_udp_datagram_bytes = *max_udp_datagram_bytes;
    }

    res.interlace = interlace;
    res.segmented = segmented;

    return res;
}

[[nodiscard]] inline std::expected<std::optional<RawVideoSdpFmtpParameters>, Error>
parse_video_sdp_fmtp_attribute(std::string_view line, uint8_t expected_payload_type) {
    auto payload = parse_fmtp_attribute_payload_for_pt(line, expected_payload_type);

    if (!payload.has_value()) {
        return std::unexpected(payload.error());
    }

    if (!payload->has_value()) {
        return std::optional<RawVideoSdpFmtpParameters>{};
    }

    auto parsed = parse_video_sdp_fmtp_payload(**payload);

    if (!parsed.has_value()) {
        return std::unexpected(parsed.error());
    }

    return std::optional<RawVideoSdpFmtpParameters>{std::move(*parsed)};
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_VIDEO_SDP_FMTP_HPP

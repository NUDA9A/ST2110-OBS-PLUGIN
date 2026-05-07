#ifndef ST2110_OBS_PLUGIN_VIDEO_SDP_TIMING_ATTRIBUTES_HPP
#define ST2110_OBS_PLUGIN_VIDEO_SDP_TIMING_ATTRIBUTES_HPP

#include "st2110/foundation/error.hpp"
#include "video_sdp_media_section.hpp"

#include <charconv>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace st2110 {
struct RawVideoSdpPtpReferenceClock {
    std::string version{};
    std::string gmid{};
    std::optional<uint8_t> domain{};
};

struct RawVideoSdpReferenceClock {
    enum class Kind { Ptp, LocalMac, Other };

    Kind kind = Kind::Other;
    std::string raw_value{};
    std::optional<RawVideoSdpPtpReferenceClock> ptp{};
    std::optional<std::string> local_mac{};
};

struct RawVideoSdpMediaClock {
    enum class Kind { Direct, Sender, Other };

    Kind kind = Kind::Other;
    std::string raw_value{};
    std::optional<uint64_t> direct_offset{};
};

struct RawVideoSdpTimestampModeValue {
    std::string raw_token{};
};

struct RawVideoSdpSenderTypeValue {
    std::string raw_token{};
};

template <typename T> struct RawVideoSdpScopedTimingValue {
    T value{};
    RawSdpAttributeScope scope = RawSdpAttributeScope::Media;
};

struct RawVideoSdpTimingAttributes {
    std::optional<RawVideoSdpScopedTimingValue<RawVideoSdpReferenceClock>> reference_clock{};
    std::optional<RawVideoSdpScopedTimingValue<RawVideoSdpMediaClock>> media_clock{};
    std::optional<RawVideoSdpScopedTimingValue<RawVideoSdpTimestampModeValue>> timestamp_mode{};
    std::optional<RawVideoSdpScopedTimingValue<uint64_t>> ts_delay_sender_ticks{};
    std::optional<RawVideoSdpScopedTimingValue<RawVideoSdpSenderTypeValue>> sender_type{};
    std::optional<RawVideoSdpScopedTimingValue<uint32_t>> troff_us{};
    std::optional<RawVideoSdpScopedTimingValue<uint32_t>> cmax{};
};

[[nodiscard]] inline std::string_view trim(std::string_view s) {
    const std::size_t first = s.find_first_not_of(" \t\n\r");
    if (first == std::string_view::npos) {
        return {};
    }

    const std::size_t last = s.find_last_not_of(" \t\n\r");
    return s.substr(first, last - first + 1);
}

[[nodiscard]] inline std::expected<uint8_t, Error> parse_video_sdp_decimal_uint8(std::string_view value) {
    value = trim(value);

    if (value.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    unsigned long val = 0;
    const char *first = value.data();
    const char *last = value.data() + value.size();
    const auto [ptr, ec] = std::from_chars(first, last, val);

    if (ec != std::errc{} || ptr != last || val > 255) {
        return std::unexpected(Error::InvalidValue);
    }

    return static_cast<uint8_t>(val);
}

[[nodiscard]] inline bool is_hex_digit_ascii(char c) noexcept {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

template <std::size_t N>
[[nodiscard]] bool is_eui_with_dash_separators(std::string_view value) noexcept {
    if (value.size() != (N * 2) + (N - 1)) {
        return false;
    }

    for (std::size_t i = 0; i < N; ++i) {
        const std::size_t octet_pos = i * 3;

        if (!is_hex_digit_ascii(value[octet_pos]) || !is_hex_digit_ascii(value[octet_pos + 1])) {
            return false;
        }

        if (i + 1 < N && value[octet_pos + 2] != '-') {
            return false;
        }
    }

    return true;
}

[[nodiscard]] inline Error validate_raw_video_sdp_ptp_gmid(std::string_view gmid) {
    gmid = trim(gmid);

    if (gmid.empty()) {
        return Error::InvalidValue;
    }

    if (gmid == "traceable") {
        return Error::Ok;
    }

    return is_eui_with_dash_separators<8>(gmid) ? Error::Ok : Error::InvalidValue;
}

[[nodiscard]] inline Error validate_raw_video_sdp_localmac(std::string_view mac) {
    mac = trim(mac);

    if (mac.empty()) {
        return Error::InvalidValue;
    }

    return is_eui_with_dash_separators<6>(mac) ? Error::Ok : Error::InvalidValue;
}

[[nodiscard]] inline bool raw_video_sdp_has_reference_clock(const RawVideoSdpTimingAttributes &raw) {
    return raw.reference_clock.has_value();
}

[[nodiscard]] inline std::expected<RawVideoSdpReferenceClock, Error>
parse_video_sdp_reference_clock(std::string_view value) {
    value = trim(value);
    if (value.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    RawVideoSdpReferenceClock res{};
    res.raw_value = std::string(value);

    if (value.starts_with("ptp=")) {
        value.remove_prefix(4);

        const std::size_t first_colon = value.find(':');
        if (first_colon == std::string_view::npos || first_colon == 0) {
            return std::unexpected(Error::InvalidValue);
        }

        const std::string_view version = value.substr(0, first_colon);
        if (version != "IEEE1588-2008") {
            return std::unexpected(Error::InvalidValue);
        }

        value.remove_prefix(first_colon + 1);
        if (value.empty()) {
            return std::unexpected(Error::InvalidValue);
        }

        RawVideoSdpPtpReferenceClock ptp{};
        ptp.version = std::string(version);

        const std::size_t second_colon = value.find(':');

        if (second_colon == std::string_view::npos) {
            if (validate_raw_video_sdp_ptp_gmid(value) != Error::Ok) {
                return std::unexpected(Error::InvalidValue);
            }

            ptp.gmid = std::string(value);

            if (ptp.gmid == "traceable") {
                ptp.domain = std::nullopt;
            }

            res.kind = RawVideoSdpReferenceClock::Kind::Ptp;
            res.ptp = std::move(ptp);
            return res;
        }

        const std::string_view gmid = value.substr(0, second_colon);
        const std::string_view domain_text = value.substr(second_colon + 1);

        if (gmid.empty() || domain_text.empty()) {
            return std::unexpected(Error::InvalidValue);
        }

        if (gmid == "traceable") {
            return std::unexpected(Error::InvalidValue);
        }

        if (validate_raw_video_sdp_ptp_gmid(gmid) != Error::Ok) {
            return std::unexpected(Error::InvalidValue);
        }

        if (domain_text.find(':') != std::string_view::npos) {
            return std::unexpected(Error::InvalidValue);
        }

        auto domain = parse_video_sdp_decimal_uint8(domain_text);
        if (!domain.has_value()) {
            return std::unexpected(domain.error());
        }

        ptp.gmid = std::string(gmid);
        ptp.domain = *domain;

        res.kind = RawVideoSdpReferenceClock::Kind::Ptp;
        res.ptp = std::move(ptp);
        return res;
    }

    if (value.starts_with("localmac=")) {
        value.remove_prefix(9);

        if (validate_raw_video_sdp_localmac(value) != Error::Ok) {
            return std::unexpected(Error::InvalidValue);
        }

        res.kind = RawVideoSdpReferenceClock::Kind::LocalMac;
        res.local_mac = std::string(value);
        return res;
    }

    res.kind = RawVideoSdpReferenceClock::Kind::Other;
    return res;
}

[[nodiscard]] inline std::expected<RawVideoSdpMediaClock, Error> parse_video_sdp_media_clock(std::string_view value) {
    value = trim(value);
    if (value.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    RawVideoSdpMediaClock res{};
    res.raw_value = std::string(value);

    if (value == "sender") {
        res.kind = RawVideoSdpMediaClock::Kind::Sender;
        return res;
    }

    if (value.starts_with("direct=")) {
        value.remove_prefix(7);
        if (value.empty()) {
            return std::unexpected(Error::InvalidValue);
        }

        uint64_t offset = 0;
        const char *first = value.data();
        const char *last = value.data() + value.size();
        const auto [ptr, ec] = std::from_chars(first, last, offset);

        if (ec != std::errc{} || ptr != last) {
            return std::unexpected(Error::InvalidValue);
        }

        res.kind = RawVideoSdpMediaClock::Kind::Direct;
        res.direct_offset = offset;
        return res;
    }

    res.kind = RawVideoSdpMediaClock::Kind::Other;
    return res;
}

[[nodiscard]] inline std::expected<RawVideoSdpTimestampModeValue, Error>
parse_video_sdp_timestamp_mode(std::string_view value) {
    value = trim(value);
    if (value.empty()) {
        return std::unexpected(Error::InvalidValue);
    }
    RawVideoSdpTimestampModeValue res{};
    res.raw_token = value;
    return res;
}

[[nodiscard]] inline std::expected<RawVideoSdpSenderTypeValue, Error>
parse_video_sdp_sender_type(std::string_view value) {
    value = trim(value);
    if (value.empty()) {
        return std::unexpected(Error::InvalidValue);
    }
    RawVideoSdpSenderTypeValue res{};
    res.raw_token = value;
    return res;
}

[[nodiscard]] inline std::expected<uint64_t, Error> parse_video_sdp_ts_delay(std::string_view value) {
    value = trim(value);
    if (value.empty()) {
        return std::unexpected(Error::InvalidValue);
    }
    uint64_t val = 0;

    const char *first = value.data();
    const char *last = value.data() + value.size();

    const auto [ptr, ec] = std::from_chars(first, last, val);

    if (ec != std::errc{} || ptr != last) {
        return std::unexpected(Error::InvalidValue);
    }

    return val;
}

[[nodiscard]] inline std::expected<uint32_t, Error> parse_video_sdp_troff(std::string_view value) {
    value = trim(value);
    if (value.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    uint32_t val = 0;
    const char *first = value.data();
    const char *last = value.data() + value.size();
    const auto [ptr, ec] = std::from_chars(first, last, val);

    if (ec != std::errc{} || ptr != last) {
        return std::unexpected(Error::InvalidValue);
    }

    return val;
}

[[nodiscard]] inline std::expected<uint32_t, Error> parse_video_sdp_cmax(std::string_view value) {
    value = trim(value);
    if (value.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    uint32_t val = 0;
    const char *first = value.data();
    const char *last = value.data() + value.size();
    const auto [ptr, ec] = std::from_chars(first, last, val);

    if (ec != std::errc{} || ptr != last) {
        return std::unexpected(Error::InvalidValue);
    }

    return val;
}

[[nodiscard]] inline std::optional<RawSdpScopedAttributeValue>
resolve_raw_sdp_scoped_timing_attribute(const std::optional<RawSdpScopedAttributeValue> &session_value,
                                        const std::optional<RawSdpScopedAttributeValue> &media_value,
                                        const std::optional<std::string> &legacy_resolved_value) {
    if (media_value.has_value()) {
        return media_value;
    }

    if (session_value.has_value()) {
        return session_value;
    }

    // Compatibility path for tests/callers that manually construct
    // RawVideoSdpMediaSection using the old resolved fields.
    if (legacy_resolved_value.has_value()) {
        return RawSdpScopedAttributeValue{.value = *legacy_resolved_value, .scope = RawSdpAttributeScope::Media};
    }

    return std::nullopt;
}

[[nodiscard]] inline std::expected<RawVideoSdpTimingAttributes, Error>
parse_video_sdp_timing_attributes(const RawVideoSdpMediaSection &raw) {
    RawVideoSdpTimingAttributes res{};

    const auto ts_refclk =
        resolve_raw_sdp_scoped_timing_attribute(raw.session_ts_refclk, raw.media_ts_refclk, raw.ts_refclk);

    if (ts_refclk.has_value()) {
        auto parsed = parse_video_sdp_reference_clock(ts_refclk->value);

        if (!parsed.has_value()) {
            return std::unexpected(parsed.error());
        }

        res.reference_clock = RawVideoSdpScopedTimingValue<RawVideoSdpReferenceClock>{.value = std::move(*parsed),
                                                                                      .scope = ts_refclk->scope};
    }

    const auto mediaclk =
        resolve_raw_sdp_scoped_timing_attribute(raw.session_mediaclk, raw.media_mediaclk, raw.mediaclk);

    if (mediaclk.has_value()) {
        auto parsed = parse_video_sdp_media_clock(mediaclk->value);

        if (!parsed.has_value()) {
            return std::unexpected(parsed.error());
        }

        res.media_clock =
            RawVideoSdpScopedTimingValue<RawVideoSdpMediaClock>{.value = std::move(*parsed), .scope = mediaclk->scope};
    }

    const auto tsmode = resolve_raw_sdp_scoped_timing_attribute(raw.session_tsmode, raw.media_tsmode, raw.tsmode);

    if (tsmode.has_value()) {
        auto parsed = parse_video_sdp_timestamp_mode(tsmode->value);

        if (!parsed.has_value()) {
            return std::unexpected(parsed.error());
        }

        res.timestamp_mode = RawVideoSdpScopedTimingValue<RawVideoSdpTimestampModeValue>{.value = std::move(*parsed),
                                                                                         .scope = tsmode->scope};
    }

    const auto tsdelay = resolve_raw_sdp_scoped_timing_attribute(raw.session_tsdelay, raw.media_tsdelay, raw.tsdelay);

    if (tsdelay.has_value()) {
        auto parsed = parse_video_sdp_ts_delay(tsdelay->value);

        if (!parsed.has_value()) {
            return std::unexpected(parsed.error());
        }

        res.ts_delay_sender_ticks = RawVideoSdpScopedTimingValue<uint64_t>{.value = *parsed, .scope = tsdelay->scope};
    }

    const auto tp = resolve_raw_sdp_scoped_timing_attribute(raw.session_tp, raw.media_tp, raw.tp);

    if (tp.has_value()) {
        auto parsed = parse_video_sdp_sender_type(tp->value);

        if (!parsed.has_value()) {
            return std::unexpected(parsed.error());
        }

        res.sender_type =
            RawVideoSdpScopedTimingValue<RawVideoSdpSenderTypeValue>{.value = std::move(*parsed), .scope = tp->scope};
    }

    const auto troff = resolve_raw_sdp_scoped_timing_attribute(raw.session_troff, raw.media_troff, raw.troff);

    if (troff.has_value()) {
        auto parsed = parse_video_sdp_troff(troff->value);

        if (!parsed.has_value()) {
            return std::unexpected(parsed.error());
        }

        res.troff_us = RawVideoSdpScopedTimingValue<uint32_t>{.value = *parsed, .scope = troff->scope};
    }

    const auto cmax = resolve_raw_sdp_scoped_timing_attribute(raw.session_cmax, raw.media_cmax, raw.cmax);

    if (cmax.has_value()) {
        auto parsed = parse_video_sdp_cmax(cmax->value);

        if (!parsed.has_value()) {
            return std::unexpected(parsed.error());
        }

        res.cmax = RawVideoSdpScopedTimingValue<uint32_t>{.value = *parsed, .scope = cmax->scope};
    }

    return res;
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_VIDEO_SDP_TIMING_ATTRIBUTES_HPP

#ifndef ST2110_OBS_PLUGIN_AUDIO_SDP_TIMING_ATTRIBUTES_HPP
#define ST2110_OBS_PLUGIN_AUDIO_SDP_TIMING_ATTRIBUTES_HPP

#include "audio_sdp_media_section.hpp"
#include "error.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace st2110 {
enum class RawAudioSdpTimingAttributeScope { Session, Media };

struct RawAudioSdpPtpReferenceClock {
    std::string version{};
    std::string gmid{};
    std::optional<uint8_t> domain{};
};

struct RawAudioSdpReferenceClock {
    enum class Kind { Ptp, LocalMac, Other };

    Kind kind = Kind::Other;
    std::string raw_value{};
    std::optional<RawAudioSdpPtpReferenceClock> ptp{};
    std::optional<std::string> local_mac{};
};

struct RawAudioSdpMediaClock {
    enum class Kind { Direct, Sender, Other };

    Kind kind = Kind::Other;
    std::string raw_value{};
    std::optional<uint64_t> direct_offset{};
};

template <typename T> struct RawAudioSdpScopedTimingValue {
    T value{};
    RawAudioSdpTimingAttributeScope scope = RawAudioSdpTimingAttributeScope::Media;
};

struct RawAudioSdpTimingAttributes {
    std::optional<RawAudioSdpScopedTimingValue<RawAudioSdpReferenceClock>> reference_clock{};
    std::optional<RawAudioSdpScopedTimingValue<RawAudioSdpMediaClock>> media_clock{};
};

[[nodiscard]] inline std::string_view trim_audio_sdp_timing_value(std::string_view s) {
    return trim_audio_sdp_ascii_ws(strip_audio_sdp_cr(s));
}

[[nodiscard]] inline std::expected<uint8_t, Error> parse_audio_sdp_decimal_uint8(std::string_view value) {
    value = trim_audio_sdp_timing_value(value);

    auto parsed = parse_audio_sdp_uint64(value);

    if (!parsed.has_value() || *parsed > 255) {
        return std::unexpected(Error::InvalidValue);
    }

    return static_cast<uint8_t>(*parsed);
}

[[nodiscard]] inline bool is_audio_hex_digit_ascii(char c) noexcept {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

template <std::size_t N> [[nodiscard]] bool is_audio_eui_with_dash_separators(std::string_view value) noexcept {
    if (value.size() != (N * 2) + (N - 1)) {
        return false;
    }

    for (std::size_t i = 0; i < N; ++i) {
        const std::size_t octet_pos = i * 3;

        if (!is_audio_hex_digit_ascii(value[octet_pos]) || !is_audio_hex_digit_ascii(value[octet_pos + 1])) {
            return false;
        }

        if (i + 1 < N && value[octet_pos + 2] != '-') {
            return false;
        }
    }

    return true;
}

[[nodiscard]] inline Error validate_raw_audio_sdp_ptp_gmid(std::string_view gmid) {
    gmid = trim_audio_sdp_timing_value(gmid);

    if (gmid.empty()) {
        return Error::InvalidValue;
    }

    if (gmid == "traceable") {
        return Error::Ok;
    }

    return is_audio_eui_with_dash_separators<8>(gmid) ? Error::Ok : Error::InvalidValue;
}

[[nodiscard]] inline Error validate_raw_audio_sdp_localmac(std::string_view mac) {
    mac = trim_audio_sdp_timing_value(mac);

    if (mac.empty()) {
        return Error::InvalidValue;
    }

    return is_audio_eui_with_dash_separators<6>(mac) ? Error::Ok : Error::InvalidValue;
}

[[nodiscard]] inline bool raw_audio_sdp_has_reference_clock(const RawAudioSdpTimingAttributes &raw) {
    return raw.reference_clock.has_value();
}

[[nodiscard]] inline bool raw_audio_sdp_has_media_level_mediaclk(const RawAudioSdpTimingAttributes &raw) {
    return raw.media_clock.has_value() && raw.media_clock->scope == RawAudioSdpTimingAttributeScope::Media;
}

[[nodiscard]] inline std::expected<RawAudioSdpReferenceClock, Error>
parse_audio_sdp_reference_clock(std::string_view value) {
    value = trim_audio_sdp_timing_value(value);

    if (value.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    RawAudioSdpReferenceClock res{};
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

        RawAudioSdpPtpReferenceClock ptp{};
        ptp.version = std::string(version);

        const std::size_t second_colon = value.find(':');

        if (second_colon == std::string_view::npos) {
            if (validate_raw_audio_sdp_ptp_gmid(value) != Error::Ok) {
                return std::unexpected(Error::InvalidValue);
            }

            ptp.gmid = std::string(value);

            if (ptp.gmid == "traceable") {
                ptp.domain = std::nullopt;
            }

            res.kind = RawAudioSdpReferenceClock::Kind::Ptp;
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

        if (validate_raw_audio_sdp_ptp_gmid(gmid) != Error::Ok) {
            return std::unexpected(Error::InvalidValue);
        }

        if (domain_text.find(':') != std::string_view::npos) {
            return std::unexpected(Error::InvalidValue);
        }

        auto domain = parse_audio_sdp_decimal_uint8(domain_text);

        if (!domain.has_value()) {
            return std::unexpected(domain.error());
        }

        ptp.gmid = std::string(gmid);
        ptp.domain = *domain;

        res.kind = RawAudioSdpReferenceClock::Kind::Ptp;
        res.ptp = std::move(ptp);
        return res;
    }

    if (value.starts_with("localmac=")) {
        value.remove_prefix(9);

        if (validate_raw_audio_sdp_localmac(value) != Error::Ok) {
            return std::unexpected(Error::InvalidValue);
        }

        res.kind = RawAudioSdpReferenceClock::Kind::LocalMac;
        res.local_mac = std::string(value);
        return res;
    }

    res.kind = RawAudioSdpReferenceClock::Kind::Other;
    return res;
}

[[nodiscard]] inline std::expected<RawAudioSdpMediaClock, Error> parse_audio_sdp_media_clock(std::string_view value) {
    value = trim_audio_sdp_timing_value(value);

    if (value.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    RawAudioSdpMediaClock res{};
    res.raw_value = std::string(value);

    if (value == "sender") {
        res.kind = RawAudioSdpMediaClock::Kind::Sender;
        return res;
    }

    if (value.starts_with("direct=")) {
        value.remove_prefix(7);

        if (value.empty()) {
            return std::unexpected(Error::InvalidValue);
        }

        auto offset = parse_audio_sdp_uint64(value);

        if (!offset.has_value()) {
            return std::unexpected(Error::InvalidValue);
        }

        res.kind = RawAudioSdpMediaClock::Kind::Direct;
        res.direct_offset = *offset;
        return res;
    }

    res.kind = RawAudioSdpMediaClock::Kind::Other;
    return res;
}

template <typename Parsed, typename Parser>
[[nodiscard]] inline std::expected<std::optional<RawAudioSdpScopedTimingValue<Parsed>>, Error>
parse_single_scoped_audio_timing_attribute(const std::vector<RawAudioSdpAttribute> &attributes, std::string_view name,
                                           RawAudioSdpTimingAttributeScope scope, Parser parser) {
    std::optional<RawAudioSdpScopedTimingValue<Parsed>> result{};

    for (const auto &attribute : attributes) {
        if (attribute.name != name) {
            continue;
        }

        if (result.has_value()) {
            return std::unexpected(Error::InvalidValue);
        }

        auto parsed = parser(attribute.value);

        if (!parsed.has_value()) {
            return std::unexpected(parsed.error());
        }

        result = RawAudioSdpScopedTimingValue<Parsed>{.value = std::move(*parsed), .scope = scope};
    }

    return result;
}

[[nodiscard]] inline std::expected<RawAudioSdpTimingAttributes, Error>
parse_audio_sdp_timing_attributes(const RawAudioSdpMediaSection &raw) {
    RawAudioSdpTimingAttributes res{};

    auto session_reference_clock = parse_single_scoped_audio_timing_attribute<RawAudioSdpReferenceClock>(
        raw.unknown_session_attributes, "ts-refclk", RawAudioSdpTimingAttributeScope::Session,
        parse_audio_sdp_reference_clock);

    if (!session_reference_clock.has_value()) {
        return std::unexpected(session_reference_clock.error());
    }

    auto media_reference_clock = parse_single_scoped_audio_timing_attribute<RawAudioSdpReferenceClock>(
        raw.unknown_attributes, "ts-refclk", RawAudioSdpTimingAttributeScope::Media, parse_audio_sdp_reference_clock);

    if (!media_reference_clock.has_value()) {
        return std::unexpected(media_reference_clock.error());
    }

    if (media_reference_clock->has_value()) {
        res.reference_clock = std::move(*media_reference_clock);
    } else if (session_reference_clock->has_value()) {
        res.reference_clock = std::move(*session_reference_clock);
    }

    auto session_media_clock = parse_single_scoped_audio_timing_attribute<RawAudioSdpMediaClock>(
        raw.unknown_session_attributes, "mediaclk", RawAudioSdpTimingAttributeScope::Session, parse_audio_sdp_media_clock);

    if (!session_media_clock.has_value()) {
        return std::unexpected(session_media_clock.error());
    }

    auto media_media_clock = parse_single_scoped_audio_timing_attribute<RawAudioSdpMediaClock>(
        raw.unknown_attributes, "mediaclk", RawAudioSdpTimingAttributeScope::Media, parse_audio_sdp_media_clock);

    if (!media_media_clock.has_value()) {
        return std::unexpected(media_media_clock.error());
    }

    if (media_media_clock->has_value()) {
        res.media_clock = std::move(*media_media_clock);
    } else if (session_media_clock->has_value()) {
        res.media_clock = std::move(*session_media_clock);
    }

    return res;
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_AUDIO_SDP_TIMING_ATTRIBUTES_HPP
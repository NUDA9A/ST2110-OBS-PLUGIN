#ifndef ST2110_OBS_PLUGIN_VIDEO_SDP_INGESTION_HPP
#define ST2110_OBS_PLUGIN_VIDEO_SDP_INGESTION_HPP

#include "error.hpp"
#include "signaling_structs.hpp"
#include "video_sdp_fmtp.hpp"
#include "video_sdp_media_section.hpp"
#include "video_sdp_rtpmap.hpp"
#include "video_sdp_signaling_adapter.hpp"
#include "video_sdp_timing_attributes.hpp"
#include "video_signaling.hpp"

#include <array>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace st2110 {
[[nodiscard]] inline bool ascii_iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }

    for (std::size_t i = 0; i < a.size(); ++i) {
        char ca = a[i];
        char cb = b[i];

        if (ca >= 'A' && ca <= 'Z') {
            ca = static_cast<char>(ca - 'A' + 'a');
        }

        if (cb >= 'A' && cb <= 'Z') {
            cb = static_cast<char>(cb - 'A' + 'a');
        }

        if (ca != cb) {
            return false;
        }
    }

    return true;
}

[[nodiscard]] inline Error validate_video_sdp_rtpmap_for_video_signaling(const RawVideoSdpRtpMap &rtpmap) {
    if (!ascii_iequals(rtpmap.encoding_name, "raw")) {
        return Error::InvalidValue;
    }

    if (rtpmap.clock_rate != 90000) {
        return Error::InvalidValue;
    }

    // For current ST 2110-20 video ingestion path we do not model/use
    // rtpmap encoding parameters. Rejecting them avoids silently ignoring
    // extra semantics at the final ingestion boundary.
    if (rtpmap.encoding_parameters.has_value()) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

[[nodiscard]] inline std::optional<uint8_t> hex_nibble(char c) {
    if (c >= '0' && c <= '9') {
        return static_cast<uint8_t>(c - '0');
    }

    if (c >= 'a' && c <= 'f') {
        return static_cast<uint8_t>(10 + c - 'a');
    }

    if (c >= 'A' && c <= 'F') {
        return static_cast<uint8_t>(10 + c - 'A');
    }

    return std::nullopt;
}

[[nodiscard]] inline std::optional<uint8_t> parse_hex_byte(std::string_view text) {
    if (text.size() != 2) {
        return std::nullopt;
    }

    const auto hi = hex_nibble(text[0]);
    const auto lo = hex_nibble(text[1]);

    if (!hi.has_value() || !lo.has_value()) {
        return std::nullopt;
    }

    return static_cast<uint8_t>((*hi << 4) | *lo);
}

template <std::size_t N>
[[nodiscard]] std::expected<std::array<uint8_t, N>, Error> parse_separated_hex_octets(std::string_view text) {
    std::array<uint8_t, N> out{};

    std::size_t pos = 0;

    for (std::size_t i = 0; i < N; ++i) {
        if (pos + 2 > text.size()) {
            return std::unexpected(Error::InvalidValue);
        }

        const auto byte = parse_hex_byte(text.substr(pos, 2));

        if (!byte.has_value()) {
            return std::unexpected(Error::InvalidValue);
        }

        out[i] = *byte;
        pos += 2;

        if (i + 1 < N) {
            if (pos >= text.size()) {
                return std::unexpected(Error::InvalidValue);
            }

            const char sep = text[pos];

            if (sep != '-' && sep != ':') {
                return std::unexpected(Error::InvalidValue);
            }

            ++pos;
        }
    }

    if (pos != text.size()) {
        return std::unexpected(Error::InvalidValue);
    }

    return out;
}

[[nodiscard]] inline std::expected<ReferenceClock, Error>
reference_clock_from_raw_video_sdp_reference_clock(const RawVideoSdpReferenceClock &raw) {
    ReferenceClock res{};

    switch (raw.kind) {
    case RawVideoSdpReferenceClock::Kind::Ptp: {
        if (!raw.ptp.has_value()) {
            return std::unexpected(Error::InvalidValue);
        }

        const auto &raw_ptp = *raw.ptp;

        if (raw_ptp.version != "IEEE1588-2008") {
            return std::unexpected(Error::InvalidValue);
        }

        PtpReferenceClock ptp{};

        if (raw_ptp.gmid == "traceable") {
            ptp.traceable = true;
        } else {
            auto clock_identity = parse_separated_hex_octets<8>(raw_ptp.gmid);

            if (!clock_identity.has_value()) {
                return std::unexpected(clock_identity.error());
            }

            ptp.clock_identity = *clock_identity;
            ptp.traceable = false;
        }

        ptp.domain_number = raw_ptp.domain.value_or(0);

        res.kind = ReferenceClockKind::Ptp;
        res.ptp = ptp;
        return res;
    }

    case RawVideoSdpReferenceClock::Kind::LocalMac: {
        if (!raw.local_mac.has_value()) {
            return std::unexpected(Error::InvalidValue);
        }

        auto mac = parse_separated_hex_octets<6>(*raw.local_mac);

        if (!mac.has_value()) {
            return std::unexpected(mac.error());
        }

        LocalMacReferenceClock local_mac{};
        local_mac.mac = *mac;

        res.kind = ReferenceClockKind::LocalMac;
        res.local_mac = local_mac;
        return res;
    }

    case RawVideoSdpReferenceClock::Kind::Other: {
        if (raw.raw_value.empty()) {
            return std::unexpected(Error::InvalidValue);
        }

        res.kind = ReferenceClockKind::Other;
        res.raw_token = raw.raw_value;
        return res;
    }

    default:
        return std::unexpected(Error::InvalidValue);
    }
}

[[nodiscard]] inline std::expected<MediaClockMode, Error>
media_clock_mode_from_raw_video_sdp_media_clock(const RawVideoSdpMediaClock &raw) {
    switch (raw.kind) {
    case RawVideoSdpMediaClock::Kind::Direct:
        // Current VideoStreamSignaling only models the mode, not direct offset.
        // Accept only direct=0 so we do not silently drop a non-zero offset.
        if (raw.direct_offset.value_or(0) != 0) {
            return std::unexpected(Error::InvalidValue);
        }

        return MediaClockMode::Direct;

    case RawVideoSdpMediaClock::Kind::Sender:
        return MediaClockMode::Sender;

    case RawVideoSdpMediaClock::Kind::Other:
    default:
        return std::unexpected(Error::InvalidValue);
    }
}

[[nodiscard]] inline std::expected<TimestampMode, Error>
timestamp_mode_from_raw_video_sdp_timestamp_mode(const RawVideoSdpTimestampModeValue &raw) {
    // ST 2110-10 defines TSMODE values SAMP, NEW, PRES.
    if (raw.raw_token == "SAMP") {
        return TimestampMode::Samp;
    }

    if (raw.raw_token == "NEW") {
        return TimestampMode::New;
    }

    if (raw.raw_token == "PRES") {
        return TimestampMode::Pres;
    }

    return std::unexpected(Error::InvalidValue);
}

[[nodiscard]] inline std::expected<VideoSenderType, Error>
sender_type_from_raw_video_sdp_sender_type(const RawVideoSdpSenderTypeValue &raw) {
    if (raw.raw_token == "2110TPN") {
        return VideoSenderType::Narrow;
    }

    if (raw.raw_token == "2110TPNL") {
        return VideoSenderType::NarrowLinear;
    }

    if (raw.raw_token == "2110TPW") {
        return VideoSenderType::Wide;
    }

    return std::unexpected(Error::InvalidValue);
}

[[nodiscard]] inline Error apply_video_sdp_timing_attributes_to_signaling(const RawVideoSdpTimingAttributes &raw,
                                                                          VideoStreamSignaling &signaling) {
    if (raw.reference_clock.has_value()) {
        auto reference_clock = reference_clock_from_raw_video_sdp_reference_clock(raw.reference_clock->value);

        if (!reference_clock.has_value()) {
            return reference_clock.error();
        }

        signaling.reference_clock = std::move(*reference_clock);
    }

    if (raw.media_clock.has_value()) {
        auto media_clock_mode = media_clock_mode_from_raw_video_sdp_media_clock(raw.media_clock->value);

        if (!media_clock_mode.has_value()) {
            return media_clock_mode.error();
        }

        signaling.media_clock_mode = *media_clock_mode;
    }

    if (raw.timestamp_mode.has_value()) {
        auto timestamp_mode = timestamp_mode_from_raw_video_sdp_timestamp_mode(raw.timestamp_mode->value);

        if (!timestamp_mode.has_value()) {
            return timestamp_mode.error();
        }

        signaling.timestamp_mode = *timestamp_mode;
    }

    if (raw.ts_delay_sender_ticks.has_value()) {
        if (raw.ts_delay_sender_ticks->value > std::numeric_limits<uint32_t>::max()) {
            return Error::InvalidValue;
        }

        signaling.ts_delay_sender_ticks = static_cast<uint32_t>(raw.ts_delay_sender_ticks->value);
    }

    if (raw.sender_type.has_value()) {
        auto sender_type = sender_type_from_raw_video_sdp_sender_type(raw.sender_type->value);

        if (!sender_type.has_value()) {
            return sender_type.error();
        }

        signaling.sender_type = *sender_type;
    }

    if (raw.troff_us.has_value()) {
        signaling.troff_us = raw.troff_us->value;
    }

    if (raw.cmax.has_value()) {
        signaling.cmax = raw.cmax->value;
    }

    return Error::Ok;
}

[[nodiscard]] inline Error apply_video_sdp_fmtp_timing_sender_fields_to_signaling(const RawVideoSdpFmtpParameters &raw,
                                                                                  VideoStreamSignaling &signaling) {
    if (raw.timestamp_mode.has_value()) {
        RawVideoSdpTimestampModeValue value{.raw_token = *raw.timestamp_mode};

        auto mapped = timestamp_mode_from_raw_video_sdp_timestamp_mode(value);
        if (!mapped.has_value()) {
            return mapped.error();
        }

        signaling.timestamp_mode = *mapped;
    }

    if (raw.ts_delay_sender_ticks.has_value()) {
        if (*raw.ts_delay_sender_ticks > std::numeric_limits<uint32_t>::max()) {
            return Error::InvalidValue;
        }

        signaling.ts_delay_sender_ticks = static_cast<uint32_t>(*raw.ts_delay_sender_ticks);
    }

    if (raw.sender_type.has_value()) {
        RawVideoSdpSenderTypeValue value{.raw_token = *raw.sender_type};

        auto mapped = sender_type_from_raw_video_sdp_sender_type(value);
        if (!mapped.has_value()) {
            return mapped.error();
        }

        signaling.sender_type = *mapped;
    }

    if (raw.troff_us.has_value()) {
        signaling.troff_us = *raw.troff_us;
    }

    if (raw.cmax.has_value()) {
        signaling.cmax = *raw.cmax;
    }

    return Error::Ok;
}

template <typename T>
[[nodiscard]] inline bool
raw_video_sdp_timing_value_is_media_scoped(const std::optional<RawVideoSdpScopedTimingValue<T>> &value) {
    return value.has_value() && value->scope == RawSdpAttributeScope::Media;
}

[[nodiscard]] inline bool raw_video_sdp_has_media_level_mediaclk(const RawVideoSdpTimingAttributes &raw) {
    return raw.media_clock && raw.media_clock->scope == RawSdpAttributeScope::Media;
}

[[nodiscard]] inline Error
validate_no_duplicate_fmtp_and_standalone_timing_fields(const RawVideoSdpFmtpParameters &fmtp,
                                                        const RawVideoSdpTimingAttributes &timing) {
    // fmtp media type parameters are media-level signaling. They may override
    // session-level standalone compatibility attributes, but must not coexist
    // with media-level standalone attributes for the same semantic field.
    if (fmtp.timestamp_mode.has_value() && raw_video_sdp_timing_value_is_media_scoped(timing.timestamp_mode)) {
        return Error::InvalidValue;
    }

    if (fmtp.ts_delay_sender_ticks.has_value() &&
        raw_video_sdp_timing_value_is_media_scoped(timing.ts_delay_sender_ticks)) {
        return Error::InvalidValue;
    }

    if (fmtp.sender_type.has_value() && raw_video_sdp_timing_value_is_media_scoped(timing.sender_type)) {
        return Error::InvalidValue;
    }

    if (fmtp.troff_us.has_value() && raw_video_sdp_timing_value_is_media_scoped(timing.troff_us)) {
        return Error::InvalidValue;
    }

    if (fmtp.cmax.has_value() && raw_video_sdp_timing_value_is_media_scoped(timing.cmax)) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

[[nodiscard]] inline std::expected<VideoStreamSignaling, Error>
video_stream_signaling_from_raw_video_sdp_media_section(const RawVideoSdpMediaSection &raw) {
    if (raw.fmtp.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    auto expected_fmtp = parse_video_sdp_fmtp_payload(raw.fmtp);

    if (!expected_fmtp.has_value()) {
        return std::unexpected(expected_fmtp.error());
    }

    auto expected_stream_signaling = video_stream_signaling_from_raw_video_sdp_fmtp(*expected_fmtp);

    if (!expected_stream_signaling.has_value()) {
        return std::unexpected(expected_stream_signaling.error());
    }

    VideoStreamSignaling stream_signaling = std::move(*expected_stream_signaling);

    auto expected_rtpmap = parse_video_sdp_rtpmap_from_media_section(raw);

    if (!expected_rtpmap.has_value()) {
        return std::unexpected(expected_rtpmap.error());
    }

    if (Error err = validate_video_sdp_rtpmap_for_video_signaling(*expected_rtpmap); err != Error::Ok) {
        return std::unexpected(err);
    }

    auto expected_timing_attributes = parse_video_sdp_timing_attributes(raw);

    if (!expected_timing_attributes.has_value()) {
        return std::unexpected(expected_timing_attributes.error());
    }

    if (!raw_video_sdp_has_reference_clock(*expected_timing_attributes)) {
        return std::unexpected(Error::InvalidValue);
    }

    if (!raw_video_sdp_has_media_level_mediaclk(*expected_timing_attributes)) {
        return std::unexpected(Error::InvalidValue);
    }

    if (Error err =
            validate_no_duplicate_fmtp_and_standalone_timing_fields(*expected_fmtp, *expected_timing_attributes);
        err != Error::Ok) {
        return std::unexpected(err);
    }

    if (Error err = apply_video_sdp_timing_attributes_to_signaling(*expected_timing_attributes, stream_signaling);
        err != Error::Ok) {
        return std::unexpected(err);
    }

    if (Error err = apply_video_sdp_fmtp_timing_sender_fields_to_signaling(*expected_fmtp, stream_signaling);
        err != Error::Ok) {
        return std::unexpected(err);
    }

    if (Error err = validate_video_stream_signaling(stream_signaling); err != Error::Ok) {
        return std::unexpected(err);
    }

    return stream_signaling;
}

[[nodiscard]] inline std::expected<VideoStreamSignaling, Error>
parse_video_stream_signaling_from_sdp(std::string_view sdp, uint8_t expected_payload_type) {
    auto expected_raw_sdp_media_section = select_raw_video_sdp_media_section(sdp, expected_payload_type);

    if (!expected_raw_sdp_media_section.has_value()) {
        return std::unexpected(expected_raw_sdp_media_section.error());
    }

    return video_stream_signaling_from_raw_video_sdp_media_section(*expected_raw_sdp_media_section);
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_VIDEO_SDP_INGESTION_HPP

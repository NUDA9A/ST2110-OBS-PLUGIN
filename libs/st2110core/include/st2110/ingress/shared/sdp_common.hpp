#ifndef ST2110_OBS_SDP_COMMON_HPP
#define ST2110_OBS_SDP_COMMON_HPP

#include <st2110/foundation/error.hpp>
#include <st2110/model/common_sdp_parameters.hpp>

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace st2110 {

struct RawSdpSessionLines {
    std::optional<std::string> group{};
};

struct RawSdpMediaSectionLines {
    std::string media_value{};

    std::string connection{};
    std::string ts_refclk{};
    std::string mediaclk{};
    std::string source_filter{};
    std::optional<std::string> mid{};
    std::optional<std::string> ptime{};
    std::string rtpmap{};

    std::vector<std::string> fmtp_common_parameters{};
    std::vector<std::string> fmtp_media_specific_parameters{};
};

struct RawSdpDocument {
    RawSdpSessionLines session{};
    std::vector<RawSdpMediaSectionLines> media_sections{};
};

[[nodiscard]] inline std::string_view strip_cr(std::string_view line) {
    if (!line.empty() && line.back() == '\r') {
        line.remove_suffix(1);
    }

    return line;
}

[[nodiscard]] inline std::string_view trim_left_ws(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
        text.remove_prefix(1);
    }

    return text;
}

[[nodiscard]] inline std::string_view trim_right_ws(std::string_view text) {
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) {
        text.remove_suffix(1);
    }

    return text;
}

[[nodiscard]] inline std::string_view trim_ws(std::string_view text) { return trim_right_ws(trim_left_ws(text)); }

[[nodiscard]] inline std::optional<std::string_view> parse_attribute_value(std::string_view line,
                                                                           const std::string_view prefix) {
    line = strip_cr(line);

    if (!line.starts_with(prefix)) {
        return std::nullopt;
    }

    return line.substr(prefix.size());
}

[[nodiscard]] inline std::vector<std::string_view> split_char(std::string_view text, const char delimiter) {
    std::vector<std::string_view> result{};

    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = text.find(delimiter, start);

        if (end == std::string_view::npos) {
            result.emplace_back(text.substr(start));
            break;
        }

        result.emplace_back(text.substr(start, end - start));
        start = end + 1;
    }

    return result;
}

[[nodiscard]] inline std::vector<std::string_view> split_ws(const std::string_view line) {
    std::vector<std::string_view> result{};

    std::size_t pos = 0;

    while (pos < line.size()) {
        while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) {
            ++pos;
        }

        if (pos >= line.size()) {
            break;
        }

        const std::size_t token_start = pos;

        while (pos < line.size() && line[pos] != ' ' && line[pos] != '\t') {
            ++pos;
        }

        result.emplace_back(line.substr(token_start, pos - token_start));
    }

    return result;
}

[[nodiscard]] inline bool ascii_ieq_char(const char a, const char b) {
    const char a_upper = (a >= 'a' && a <= 'z') ? static_cast<char>(a - ('a' - 'A')) : a;
    const char b_upper = (b >= 'a' && b <= 'z') ? static_cast<char>(b - ('a' - 'A')) : b;
    return a_upper == b_upper;
}

[[nodiscard]] inline bool ascii_iequals(const std::string_view a, const std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }

    for (std::size_t i = 0; i < a.size(); ++i) {
        if (!ascii_ieq_char(a[i], b[i])) {
            return false;
        }
    }

    return true;
}

[[nodiscard]] inline bool ascii_istarts_with(const std::string_view text, const std::string_view prefix) {
    return text.size() >= prefix.size() && ascii_iequals(text.substr(0, prefix.size()), prefix);
}

template <typename T>
[[nodiscard]] inline std::expected<T, Error> parse_sdp_numeric_value(const std::string_view text) {
    if (text.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    std::uint64_t value = 0;

    const char *first = text.data();
    const char *last = text.data() + text.size();

    const auto [ptr, ec] = std::from_chars(first, last, value);

    if (ec != std::errc{} || ptr != last || value > std::numeric_limits<T>::max()) {
        return std::unexpected(Error::InvalidValue);
    }

    return static_cast<T>(value);
}

[[nodiscard]] inline std::optional<std::uint8_t> parse_payload_type(const std::string_view text) {
    auto res = parse_sdp_numeric_value<std::uint8_t>(text);
    if (!res.has_value() || *res > 127) {
        return std::nullopt;
    }

    return *res;
}

[[nodiscard]] inline bool is_common_fmtp_parameter_name(const std::string_view key) {
    return ascii_iequals(key, "MAXUDP") || ascii_iequals(key, "TSMODE") || ascii_iequals(key, "TSDELAY");
}

[[nodiscard]] inline Error split_fmtp_parameters(std::string_view fmtp_value,
                                                 std::vector<std::string> &common_parameters,
                                                 std::vector<std::string> &media_specific_parameters) {
    fmtp_value = trim_ws(strip_cr(fmtp_value));

    if (fmtp_value.empty()) {
        return Error::InvalidValue;
    }

    const std::size_t pt_end = fmtp_value.find_first_of(" \t");
    if (pt_end == std::string_view::npos) {
        return Error::InvalidValue;
    }

    const std::string_view pt_text = fmtp_value.substr(0, pt_end);
    if (!parse_payload_type(pt_text).has_value()) {
        return Error::InvalidValue;
    }

    std::string_view parameters_text = fmtp_value.substr(pt_end);
    parameters_text = trim_left_ws(parameters_text);

    if (parameters_text.empty()) {
        return Error::InvalidValue;
    }

    const auto parameters = split_char(parameters_text, ';');

    for (std::string_view parameter : parameters) {
        parameter = trim_ws(parameter);

        if (parameter.empty()) {
            continue;
        }

        const std::size_t eq_pos = parameter.find('=');
        const std::string_view key =
            trim_ws(eq_pos == std::string_view::npos ? parameter : parameter.substr(0, eq_pos));

        if (key.empty()) {
            return Error::InvalidValue;
        }

        if (is_common_fmtp_parameter_name(key)) {
            common_parameters.emplace_back(parameter);
        } else {
            media_specific_parameters.emplace_back(parameter);
        }
    }

    return Error::Ok;
}

[[nodiscard]] inline Error set_single_line(std::optional<std::string> &field, std::string_view value) {
    value = trim_ws(strip_cr(value));

    if (value.empty()) {
        return Error::InvalidValue;
    }

    if (field.has_value()) {
        return Error::InvalidValue;
    }

    field = std::string(value);
    return Error::Ok;
}

[[nodiscard]] inline Error set_required_single_line(std::string &field, std::string_view value) {
    value = trim_ws(strip_cr(value));

    if (value.empty()) {
        return Error::InvalidValue;
    }

    if (!field.empty()) {
        return Error::InvalidValue;
    }

    field = std::string(value);
    return Error::Ok;
}

[[nodiscard]] inline std::expected<RawSdpDocument, Error> parse_raw_sdp_document(std::string_view sdp) {
    RawSdpDocument res{};
    RawSdpMediaSectionLines *current_media = nullptr;

    std::size_t line_start = 0;

    while (line_start <= sdp.size()) {
        std::size_t line_end = sdp.find('\n', line_start);
        if (line_end == std::string_view::npos) {
            line_end = sdp.size();
        }

        std::string_view line = sdp.substr(line_start, line_end - line_start);
        line = strip_cr(line);

        if (!line.empty()) {
            if (line.starts_with("m=")) {
                std::string_view media_value = trim_ws(line.substr(2));

                if (media_value.empty()) {
                    return std::unexpected(Error::InvalidValue);
                }

                res.media_sections.push_back(RawSdpMediaSectionLines{.media_value = std::string(media_value)});
                current_media = &res.media_sections.back();
            } else if (current_media == nullptr) {
                if (const auto value = parse_attribute_value(line, "a=group:"); value.has_value()) {
                    if (const Error err = set_single_line(res.session.group, *value); err != Error::Ok) {
                        return std::unexpected(err);
                    }
                }
            } else {
                if (line.starts_with("c=")) {
                    if (const Error err = set_required_single_line(current_media->connection, line.substr(2));
                        err != Error::Ok) {
                        return std::unexpected(err);
                    }
                } else if (const auto value = parse_attribute_value(line, "a=ts-refclk:"); value.has_value()) {
                    if (const Error err = set_required_single_line(current_media->ts_refclk, *value);
                        err != Error::Ok) {
                        return std::unexpected(err);
                    }
                } else if (const auto value = parse_attribute_value(line, "a=mediaclk:"); value.has_value()) {
                    if (const Error err = set_required_single_line(current_media->mediaclk, *value); err != Error::Ok) {
                        return std::unexpected(err);
                    }
                } else if (const auto value = parse_attribute_value(line, "a=source-filter:"); value.has_value()) {
                    if (const Error err = set_required_single_line(current_media->source_filter, *value);
                        err != Error::Ok) {
                        return std::unexpected(err);
                    }
                } else if (const auto value = parse_attribute_value(line, "a=mid:"); value.has_value()) {
                    if (const Error err = set_single_line(current_media->mid, *value); err != Error::Ok) {
                        return std::unexpected(err);
                    }
                } else if (const auto value = parse_attribute_value(line, "a=rtpmap:"); value.has_value()) {
                    if (const Error err = set_required_single_line(current_media->rtpmap, *value); err != Error::Ok) {
                        return std::unexpected(err);
                    }
                } else if (const auto value = parse_attribute_value(line, "a=fmtp:"); value.has_value()) {
                    if (const Error err = split_fmtp_parameters(*value, current_media->fmtp_common_parameters,
                                                                current_media->fmtp_media_specific_parameters);
                        err != Error::Ok) {
                        return std::unexpected(err);
                    }
                } else if (const auto value = parse_attribute_value(line, "a=ptime:"); value.has_value()) {
                    if (const Error err = set_single_line(current_media->ptime, *value); err != Error::Ok) {
                        return std::unexpected(err);
                    }
                }
            }
        }

        if (line_end == sdp.size()) {
            break;
        }

        line_start = line_end + 1;
    }

    return res;
}

[[nodiscard]] inline std::expected<std::uint8_t, Error> parse_hex_u8(const std::string_view text) {
    if (text.size() != 2) {
        return std::unexpected(Error::InvalidValue);
    }

    unsigned value = 0;

    const char *first = text.data();
    const char *last = text.data() + text.size();

    const auto [ptr, ec] = std::from_chars(first, last, value, 16);
    if (ec != std::errc{} || ptr != last || value > 0xFFU) {
        return std::unexpected(Error::InvalidValue);
    }

    return static_cast<std::uint8_t>(value);
}

template <std::size_t N>
[[nodiscard]] inline std::expected<std::array<std::uint8_t, N>, Error>
parse_dash_separated_hex_bytes(const std::string_view text) {
    const auto parts = split_char(text, '-');
    if (parts.size() != N) {
        return std::unexpected(Error::InvalidValue);
    }

    std::array<std::uint8_t, N> out{};
    for (std::size_t i = 0; i < N; ++i) {
        auto byte = parse_hex_u8(trim_ws(parts[i]));
        if (!byte.has_value()) {
            return std::unexpected(byte.error());
        }

        out[i] = *byte;
    }

    return out;
}

[[nodiscard]] inline Error parse_single_fmtp_parameter_value(const std::vector<std::string> &parameters,
                                                             const std::string_view expected_key,
                                                             std::optional<std::string_view> &out) {
    out.reset();

    for (const std::string &parameter_text : parameters) {
        std::string_view parameter = trim_ws(parameter_text);
        if (parameter.empty()) {
            continue;
        }

        const std::size_t eq_pos = parameter.find('=');
        if (eq_pos == std::string_view::npos) {
            continue;
        }

        const std::string_view key = trim_ws(parameter.substr(0, eq_pos));
        const std::string_view value = trim_ws(parameter.substr(eq_pos + 1));

        if (key.empty() || value.empty()) {
            return Error::InvalidValue;
        }

        if (!ascii_iequals(key, expected_key)) {
            continue;
        }

        if (out.has_value()) {
            return Error::InvalidValue;
        }

        out = value;
    }

    return Error::Ok;
}

[[nodiscard]] inline std::expected<MediaClockSignaling, Error>
parse_media_clock_signaling_value(std::string_view raw_value) {
    raw_value = trim_ws(raw_value);
    if (raw_value.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    if (ascii_iequals(raw_value, "sender")) {
        MediaClockSignaling signaling{};
        signaling.kind = MediaClockKind::Sender;
        return signaling;
    }

    if (ascii_istarts_with(raw_value, "direct=")) {
        const std::string_view value_text = trim_ws(raw_value.substr(7));
        auto parsed_offset = parse_sdp_numeric_value<std::uint32_t>(value_text);
        if (!parsed_offset.has_value()) {
            return std::unexpected(parsed_offset.error());
        }

        MediaClockSignaling signaling{};
        signaling.kind = MediaClockKind::Direct;
        signaling.direct = DirectMediaClock{.rtp_clock_offset = *parsed_offset};
        return signaling;
    }

    MediaClockSignaling signaling{};
    signaling.kind = MediaClockKind::Other;
    signaling.raw_token = std::string(raw_value);
    return signaling;
}

[[nodiscard]] inline std::expected<ReferenceClock, Error> parse_reference_clock_value(std::string_view raw_value) {
    raw_value = trim_ws(raw_value);
    if (raw_value.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    if (ascii_istarts_with(raw_value, "ptp=")) {
        std::string_view ptp_value = raw_value.substr(4);

        if (ascii_iequals(ptp_value, "IEEE1588-2008:traceable")) {
            ReferenceClock clock{};
            clock.kind = ReferenceClockKind::Ptp;
            clock.ptp = PtpReferenceClock{
                .clock_identity = {},
                .domain_number = 0,
                .traceable = true,
            };
            return clock;
        }

        constexpr std::string_view kPtpPrefix = "IEEE1588-2008:";
        if (!ascii_istarts_with(ptp_value, kPtpPrefix)) {
            ReferenceClock clock{};
            clock.kind = ReferenceClockKind::Other;
            clock.raw_token = std::string(raw_value);
            return clock;
        }

        const std::string_view suffix = ptp_value.substr(kPtpPrefix.size());
        if (suffix.empty()) {
            return std::unexpected(Error::InvalidValue);
        }

        const auto parts = split_char(suffix, ':');
        if (parts.empty() || parts.size() > 2) {
            return std::unexpected(Error::InvalidValue);
        }

        auto parsed_identity = parse_dash_separated_hex_bytes<8>(trim_ws(parts[0]));
        if (!parsed_identity.has_value()) {
            return std::unexpected(parsed_identity.error());
        }

        std::uint16_t domain_number = 0;
        if (parts.size() == 2) {
            auto parsed_domain = parse_sdp_numeric_value<std::uint16_t>(trim_ws(parts[1]));
            if (!parsed_domain.has_value()) {
                return std::unexpected(parsed_domain.error());
            }

            domain_number = *parsed_domain;
        }

        ReferenceClock clock{};
        clock.kind = ReferenceClockKind::Ptp;
        clock.ptp = PtpReferenceClock{
            .clock_identity = *parsed_identity,
            .domain_number = domain_number,
            .traceable = false,
        };
        return clock;
    }

    if (ascii_istarts_with(raw_value, "localmac=")) {
        const std::string_view mac_text = trim_ws(raw_value.substr(9));
        auto parsed_mac = parse_dash_separated_hex_bytes<6>(mac_text);
        if (!parsed_mac.has_value()) {
            return std::unexpected(parsed_mac.error());
        }

        ReferenceClock clock{};
        clock.kind = ReferenceClockKind::LocalMac;
        clock.local_mac = LocalMacReferenceClock{.mac = *parsed_mac};
        return clock;
    }

    ReferenceClock clock{};
    clock.kind = ReferenceClockKind::Other;
    clock.raw_token = std::string(raw_value);
    return clock;
}

[[nodiscard]] inline std::expected<TimestampMode, Error> parse_timestamp_mode_value(std::string_view raw_value) {
    raw_value = trim_ws(raw_value);

    if (ascii_iequals(raw_value, "SAMP")) {
        return TimestampMode::Samp;
    }

    if (ascii_iequals(raw_value, "NEW")) {
        return TimestampMode::New;
    }

    if (ascii_iequals(raw_value, "PRES")) {
        return TimestampMode::Pres;
    }

    return std::unexpected(Error::InvalidValue);
}

[[nodiscard]] inline bool is_known_source_filter_mode(const std::string_view mode) {
    return mode == "incl" || mode == "excl";
}

[[nodiscard]] inline std::expected<SourceFilterSignaling, Error>
parse_source_filter_signaling_value(std::string_view raw_value, const SourceFilterSignaling::Scope scope) {
    raw_value = trim_ws(raw_value);
    if (raw_value.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    const auto tokens = split_ws(raw_value);
    if (tokens.size() < 5) {
        return std::unexpected(Error::InvalidValue);
    }

    if (!is_known_source_filter_mode(tokens[0])) {
        return std::unexpected(Error::InvalidValue);
    }

    if (tokens[1].empty() || tokens[2].empty() || tokens[3].empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    SourceFilterSignaling filter{};
    filter.raw_value = std::string(raw_value);
    filter.scope = scope;
    filter.filter_mode = std::string(tokens[0]);
    filter.network_type = std::string(tokens[1]);
    filter.address_type = std::string(tokens[2]);
    filter.destination_address = std::string(tokens[3]);

    for (std::size_t i = 4; i < tokens.size(); ++i) {
        if (tokens[i].empty()) {
            return std::unexpected(Error::InvalidValue);
        }

        filter.source_addresses.emplace_back(tokens[i]);
    }

    return filter;
}

[[nodiscard]] inline std::expected<StreamTimingSignaling, Error>
parse_stream_timing_signaling(const RawSdpSessionLines & /*session*/, const RawSdpMediaSectionLines &media,
                              const std::uint32_t rtp_clock_rate) {
    StreamTimingSignaling timing{};
    timing.rtp_clock_rate = rtp_clock_rate;

    if (media.ts_refclk.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    if (media.mediaclk.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    std::optional<std::string_view> fmtp_tsmode{};
    std::optional<std::string_view> fmtp_tsdelay{};

    if (const Error err = parse_single_fmtp_parameter_value(media.fmtp_common_parameters, "TSMODE", fmtp_tsmode);
        err != Error::Ok) {
        return std::unexpected(err);
    }

    if (const Error err = parse_single_fmtp_parameter_value(media.fmtp_common_parameters, "TSDELAY", fmtp_tsdelay);
        err != Error::Ok) {
        return std::unexpected(err);
    }

    {
        auto parsed = parse_reference_clock_value(media.ts_refclk);
        if (!parsed.has_value()) {
            return std::unexpected(parsed.error());
        }

        timing.reference_clock = std::move(*parsed);
    }

    {
        auto parsed = parse_media_clock_signaling_value(media.mediaclk);
        if (!parsed.has_value()) {
            return std::unexpected(parsed.error());
        }

        timing.media_clock = std::move(*parsed);
    }

    if (fmtp_tsmode.has_value()) {
        auto parsed = parse_timestamp_mode_value(*fmtp_tsmode);
        if (!parsed.has_value()) {
            return std::unexpected(parsed.error());
        }

        timing.timestamp_mode = *parsed;
    }

    if (fmtp_tsdelay.has_value()) {
        auto parsed = parse_sdp_numeric_value<std::uint32_t>(*fmtp_tsdelay);
        if (!parsed.has_value()) {
            return std::unexpected(parsed.error());
        }

        timing.ts_delay_us = *parsed;
    }

    return timing;
}

[[nodiscard]] inline std::expected<StreamTransportSignaling, Error>
parse_stream_transport_signaling(const RawSdpSessionLines &session, const RawSdpMediaSectionLines &media) {
    StreamTransportSignaling transport{};

    if (media.source_filter.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    if (media.mid.has_value()) {
        transport.mid = *media.mid;
    }

    std::optional<std::string_view> maxudp_value{};
    if (const Error err = parse_single_fmtp_parameter_value(media.fmtp_common_parameters, "MAXUDP", maxudp_value);
        err != Error::Ok) {
        return std::unexpected(err);
    }

    if (maxudp_value.has_value()) {
        auto parsed = parse_sdp_numeric_value<std::size_t>(*maxudp_value);
        if (!parsed.has_value()) {
            return std::unexpected(parsed.error());
        }

        if (*parsed == 0 || *parsed > 8960) {
            return std::unexpected(Error::InvalidValue);
        }

        transport.max_udp_datagram_bytes = *parsed;
    }

    {
        auto parsed = parse_source_filter_signaling_value(media.source_filter, SourceFilterSignaling::Scope::Media);
        if (!parsed.has_value()) {
            return std::unexpected(parsed.error());
        }

        transport.source_filters.push_back(std::move(*parsed));
    }

    if (transport.mid.has_value() && session.group.has_value()) {
        const auto tokens = split_ws(*session.group);

        if (!tokens.empty() && ascii_iequals(tokens[0], "DUP")) {
            if (tokens.size() != 3) {
                return std::unexpected(Error::InvalidValue);
            }

            DuplicateStreamGroup duplicate_group{
                .first_mid = std::string(tokens[1]),
                .second_mid = std::string(tokens[2]),
            };

            if (duplicate_group.first_mid.empty() || duplicate_group.second_mid.empty()) {
                return std::unexpected(Error::InvalidValue);
            }

            if (*transport.mid == duplicate_group.first_mid || *transport.mid == duplicate_group.second_mid) {
                transport.duplicate_group = std::move(duplicate_group);
            }
        }
    }

    return transport;
}

} // namespace st2110

#endif // ST2110_OBS_SDP_COMMON_HPP
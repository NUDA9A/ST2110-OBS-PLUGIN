#ifndef ST2110_OBS_PLUGIN_VIDEO_SDP_MEDIA_SECTION_HPP
#define ST2110_OBS_PLUGIN_VIDEO_SDP_MEDIA_SECTION_HPP

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

#include "error.hpp"

namespace st2110 {
struct RawSdpAttribute {
    std::string name{};
    std::string value{};
};

struct RawSdpConnectionData {
    std::string network_type{};
    std::string address_type{};
    std::string connection_address{};

    std::string base_address{};
    std::optional<uint8_t> ttl{};
    std::optional<uint32_t> address_count{};
};

enum class RawSdpAttributeScope { Session, Media };

struct RawSdpScopedAttributeValue {
    std::string value{};
    RawSdpAttributeScope scope = RawSdpAttributeScope::Media;
};

struct RawSdpSourceFilter {
    enum class Scope { Session, Media };

    std::string raw_value{};
    Scope scope = Scope::Media;

    std::string filter_mode{};
    std::string network_type{};
    std::string address_type{};
    std::string destination_address{};
    std::vector<std::string> source_addresses{};
};

struct RawSdpGroup {
    std::string semantics{};
    std::vector<std::string> mids{};
};

struct RawVideoSdpDuplicateMediaCandidate {
    std::string media_line{};
    uint8_t payload_type = 0;
    std::vector<uint8_t> media_payload_types{};

    std::optional<std::string> mid{};
    std::optional<RawSdpConnectionData> media_connection{};
    std::vector<RawSdpSourceFilter> source_filters{};

    std::string rtpmap{};
    std::string fmtp{};
};

struct RawVideoSdpMediaSection {
    std::string media_line{};
    uint8_t payload_type = 0;
    std::vector<uint8_t> media_payload_types{};

    std::string rtpmap{};
    std::string fmtp{};

    std::optional<std::string> ts_refclk{};
    std::optional<std::string> mediaclk{};
    std::optional<std::string> tsmode{};
    std::optional<std::string> tsdelay{};
    std::optional<std::string> tp{};
    std::optional<std::string> troff{};
    std::optional<std::string> cmax{};

    std::optional<RawSdpScopedAttributeValue> session_ts_refclk{};
    std::optional<RawSdpScopedAttributeValue> media_ts_refclk{};
    std::optional<RawSdpScopedAttributeValue> session_mediaclk{};
    std::optional<RawSdpScopedAttributeValue> media_mediaclk{};
    std::optional<RawSdpScopedAttributeValue> session_tsmode{};
    std::optional<RawSdpScopedAttributeValue> media_tsmode{};
    std::optional<RawSdpScopedAttributeValue> session_tsdelay{};
    std::optional<RawSdpScopedAttributeValue> media_tsdelay{};
    std::optional<RawSdpScopedAttributeValue> session_tp{};
    std::optional<RawSdpScopedAttributeValue> media_tp{};
    std::optional<RawSdpScopedAttributeValue> session_troff{};
    std::optional<RawSdpScopedAttributeValue> media_troff{};
    std::optional<RawSdpScopedAttributeValue> session_cmax{};
    std::optional<RawSdpScopedAttributeValue> media_cmax{};

    std::optional<RawSdpConnectionData> session_connection{};
    std::optional<RawSdpConnectionData> media_connection{};

    std::optional<std::string> mid{};
    std::vector<RawSdpSourceFilter> source_filters{};
    std::vector<RawSdpGroup> session_groups{};
    std::vector<RawVideoSdpDuplicateMediaCandidate> duplicate_candidates{};

    std::vector<RawSdpAttribute> unknown_session_attributes{};

    std::vector<RawSdpAttribute> unknown_attributes{};
};

[[nodiscard]] inline std::string_view strip_cr(std::string_view line) {
    if (!line.empty() && line.back() == '\r') {
        line.remove_suffix(1);
    }
    return line;
}

[[nodiscard]] inline std::vector<std::string_view> split_ws(std::string_view line) {
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

[[nodiscard]] inline std::optional<uint8_t> parse_payload_type(std::string_view text) {
    unsigned value = 0;

    const char *first = text.data();
    const char *last = text.data() + text.size();

    const auto [ptr, ec] = std::from_chars(first, last, value);

    if (ec != std::errc{} || ptr != last || value > 127) {
        return std::nullopt;
    }

    return static_cast<uint8_t>(value);
}

[[nodiscard]] inline std::expected<uint16_t, Error> parse_sdp_media_port_token(std::string_view token) {
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

[[nodiscard]] inline Error validate_video_sdp_media_protocol_token(std::string_view token) {
    return token == "RTP/AVP" ? Error::Ok : Error::InvalidValue;
}

[[nodiscard]] inline Error validate_video_sdp_media_payload_type(uint8_t payload_type) {
    return payload_type >= 96 && payload_type <= 127 ? Error::Ok : Error::InvalidValue;
}

[[nodiscard]] inline std::expected<std::vector<uint8_t>, Error>
parse_video_m_line_payload_types(std::string_view line) {
    line = strip_cr(line);

    if (!line.starts_with("m=video")) {
        return std::unexpected(Error::InvalidValue);
    }

    const auto tokens = split_ws(line);

    // m=<media> <port> <proto> <payload-type>...
    //
    // Example:
    // m=video 50000 RTP/AVP 112
    //
    // tokens:
    // [0] m=video
    // [1] 50000
    // [2] RTP/AVP
    // [3] 112
    if (tokens.size() < 4) {
        return std::unexpected(Error::InvalidValue);
    }

    if (tokens[0] != "m=video") {
        return std::unexpected(Error::InvalidValue);
    }

    if (auto parsed_port = parse_sdp_media_port_token(tokens[1]); !parsed_port.has_value()) {
        return std::unexpected(parsed_port.error());
    }

    if (Error err = validate_video_sdp_media_protocol_token(tokens[2]); err != Error::Ok) {
        return std::unexpected(err);
    }

    std::vector<uint8_t> payload_types{};
    payload_types.reserve(tokens.size() - 3);

    for (std::size_t i = 3; i < tokens.size(); ++i) {
        const auto payload_type = parse_payload_type(tokens[i]);

        if (!payload_type.has_value()) {
            return std::unexpected(Error::InvalidValue);
        }

        if (Error err = validate_video_sdp_media_payload_type(*payload_type); err != Error::Ok) {
            return std::unexpected(err);
        }

        payload_types.push_back(*payload_type);
    }

    return payload_types;
}

[[nodiscard]] inline bool contains_payload_type(const std::vector<uint8_t> &payload_types,
                                                uint8_t expected_payload_type) {
    for (const uint8_t payload_type : payload_types) {
        if (payload_type == expected_payload_type) {
            return true;
        }
    }

    return false;
}

[[nodiscard]] inline std::expected<std::optional<std::string_view>, Error>
parse_payload_bound_attribute_value(std::string_view line, std::string_view prefix, uint8_t expected_payload_type) {
    line = strip_cr(line);

    if (!line.starts_with(prefix)) {
        return std::optional<std::string_view>{};
    }

    std::string_view tail = line.substr(prefix.size());

    const std::size_t pt_end = tail.find_first_of(" \t");
    if (pt_end == std::string_view::npos) {
        return std::unexpected(Error::InvalidValue);
    }

    const std::string_view pt_text = tail.substr(0, pt_end);
    const auto payload_type = parse_payload_type(pt_text);

    if (!payload_type.has_value()) {
        return std::unexpected(Error::InvalidValue);
    }

    tail.remove_prefix(pt_end);

    while (!tail.empty() && (tail.front() == ' ' || tail.front() == '\t')) {
        tail.remove_prefix(1);
    }

    if (tail.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    if (*payload_type != expected_payload_type) {
        return std::optional<std::string_view>{};
    }

    return tail;
}

[[nodiscard]] inline std::optional<std::string_view> parse_attribute_value(std::string_view line,
                                                                           std::string_view prefix) {
    line = strip_cr(line);

    if (!line.starts_with(prefix)) {
        return std::nullopt;
    }

    return line.substr(prefix.size());
}

[[nodiscard]] inline std::string_view trim_left_ws(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
        text.remove_prefix(1);
    }

    return text;
}

[[nodiscard]] inline RawSdpAttribute parse_unknown_sdp_attribute(std::string_view line) {
    line = strip_cr(line);

    // Caller should only pass a= lines.
    line.remove_prefix(2);

    if (line.starts_with("rtpmap:") || line.starts_with("fmtp:")) {
        const std::size_t name_end = line.find_first_of(" \t");

        if (name_end == std::string_view::npos) {
            return RawSdpAttribute{.name = std::string(line), .value = ""};
        }

        return RawSdpAttribute{.name = std::string(line.substr(0, name_end)),
                               .value = std::string(trim_left_ws(line.substr(name_end)))};
    }

    const std::size_t colon_pos = line.find(':');

    if (colon_pos == std::string_view::npos) {
        return RawSdpAttribute{.name = std::string(line), .value = ""};
    }

    return RawSdpAttribute{.name = std::string(line.substr(0, colon_pos)),
                           .value = std::string(line.substr(colon_pos + 1))};
}

struct RawSdpConnectionAddressParameters {
    std::string base_address{};
    std::optional<uint8_t> ttl{};
    std::optional<uint32_t> address_count{};
};

[[nodiscard]] inline std::expected<uint64_t, Error> parse_sdp_connection_address_uint64(std::string_view value) {
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

[[nodiscard]] inline std::expected<RawSdpConnectionAddressParameters, Error>
parse_connection_address_parameters(std::string_view connection_address) {
    connection_address = strip_cr(connection_address);

    if (connection_address.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    RawSdpConnectionAddressParameters res{};

    const std::size_t first_slash = connection_address.find('/');

    if (first_slash == std::string_view::npos) {
        res.base_address = std::string(connection_address);
        return res;
    }

    const std::string_view base_address = connection_address.substr(0, first_slash);

    if (base_address.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    res.base_address = std::string(base_address);

    const std::size_t ttl_start = first_slash + 1;
    const std::size_t second_slash = connection_address.find('/', ttl_start);

    const std::string_view ttl_text = second_slash == std::string_view::npos
                                          ? connection_address.substr(ttl_start)
                                          : connection_address.substr(ttl_start, second_slash - ttl_start);

    auto parsed_ttl = parse_sdp_connection_address_uint64(ttl_text);

    if (!parsed_ttl.has_value()) {
        return std::unexpected(parsed_ttl.error());
    }

    if (*parsed_ttl > std::numeric_limits<uint8_t>::max()) {
        return std::unexpected(Error::InvalidValue);
    }

    res.ttl = static_cast<uint8_t>(*parsed_ttl);

    if (second_slash == std::string_view::npos) {
        return res;
    }

    const std::size_t address_count_start = second_slash + 1;

    if (connection_address.find('/', address_count_start) != std::string_view::npos) {
        return std::unexpected(Error::InvalidValue);
    }

    const std::string_view address_count_text = connection_address.substr(address_count_start);

    auto parsed_address_count = parse_sdp_connection_address_uint64(address_count_text);

    if (!parsed_address_count.has_value()) {
        return std::unexpected(parsed_address_count.error());
    }

    if (*parsed_address_count == 0 || *parsed_address_count > std::numeric_limits<uint32_t>::max()) {
        return std::unexpected(Error::InvalidValue);
    }

    res.address_count = static_cast<uint32_t>(*parsed_address_count);

    return res;
}

[[nodiscard]] inline std::expected<RawSdpConnectionData, Error> parse_connection_data(std::string_view line) {
    line = strip_cr(line);

    if (!line.starts_with("c=")) {
        return std::unexpected(Error::InvalidValue);
    }

    const auto tokens = split_ws(line.substr(2));

    // c=<nettype> <addrtype> <connection-address>
    if (tokens.size() != 3) {
        return std::unexpected(Error::InvalidValue);
    }

    auto parsed_connection_address = parse_connection_address_parameters(tokens[2]);

    if (!parsed_connection_address.has_value()) {
        return std::unexpected(parsed_connection_address.error());
    }

    return RawSdpConnectionData{.network_type = std::string(tokens[0]),
                                .address_type = std::string(tokens[1]),
                                .connection_address = std::string(tokens[2]),
                                .base_address = std::move(parsed_connection_address->base_address),
                                .ttl = parsed_connection_address->ttl,
                                .address_count = parsed_connection_address->address_count};
}

[[nodiscard]] inline bool source_filter_address_token_is_structurally_clean(std::string_view token) {
    if (token.empty()) {
        return false;
    }

    for (const char c : token) {
        if (c == ',' || c == ';') {
            return false;
        }
    }

    return true;
}

[[nodiscard]] inline bool is_known_source_filter_mode(std::string_view mode) {
    return mode == "incl" || mode == "excl";
}

[[nodiscard]] inline Error
validate_source_filter_attribute_tokens(const std::vector<std::string_view> &tokens) {
    if (tokens.size() < 5) {
        return Error::InvalidValue;
    }

    if (!is_known_source_filter_mode(tokens[0])) {
        return Error::InvalidValue;
    }

    for (std::size_t i = 1; i < 4; ++i) {
        if (tokens[i].empty()) {
            return Error::InvalidValue;
        }
    }

    for (std::size_t i = 4; i < tokens.size(); ++i) {
        if (!source_filter_address_token_is_structurally_clean(tokens[i])) {
            return Error::InvalidValue;
        }
    }

    return Error::Ok;
}

[[nodiscard]] inline std::expected<RawSdpSourceFilter, Error>
parse_source_filter_attribute_value(std::string_view value, RawSdpSourceFilter::Scope scope) {
    value = strip_cr(value);
    value = trim_left_ws(value);

    if (value.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    const auto tokens = split_ws(value);

    // RFC-style source-filter shape:
    // a=source-filter:<filter-mode> <nettype> <addrtype> <dest-address> <src-address>...
    //
    // Keep this raw layer runtime-agnostic:
    // - do not reject because multicast/socket support is missing;
    // - preserve the original value;
    // - only reject structurally malformed values.
    if (Error err = validate_source_filter_attribute_tokens(tokens); err != Error::Ok) {
        return std::unexpected(err);
    }

    RawSdpSourceFilter res{.raw_value = std::string(value),
                           .scope = scope,
                           .filter_mode = std::string(tokens[0]),
                           .network_type = std::string(tokens[1]),
                           .address_type = std::string(tokens[2]),
                           .destination_address = std::string(tokens[3])};

    res.source_addresses.reserve(tokens.size() - 4);
    for (std::size_t i = 4; i < tokens.size(); ++i) {
        res.source_addresses.emplace_back(tokens[i]);
    }

    return res;
}

[[nodiscard]] inline std::expected<RawSdpGroup, Error> parse_group_attribute(std::string_view value) {
    value = strip_cr(value);
    value = trim_left_ws(value);

    const auto tokens = split_ws(value);

    // a=group:<semantics> <mid>...
    if (tokens.size() < 2) {
        return std::unexpected(Error::InvalidValue);
    }

    RawSdpGroup group{};
    group.semantics = std::string(tokens[0]);
    group.mids.reserve(tokens.size() - 1);

    for (std::size_t i = 1; i < tokens.size(); ++i) {
        group.mids.emplace_back(tokens[i]);
    }

    return group;
}

[[nodiscard]] inline bool has_dup_session_group(const std::vector<RawSdpGroup> &groups) {
    for (const auto &group : groups) {
        if (group.semantics == "DUP") {
            return true;
        }
    }

    return false;
}

[[nodiscard]] inline bool raw_sdp_group_contains_mid(const RawSdpGroup &group, const std::string &mid) {
    for (const auto &candidate_mid : group.mids) {
        if (candidate_mid == mid) {
            return true;
        }
    }

    return false;
}

[[nodiscard]] inline bool has_dup_session_group_containing_both_mids(const std::vector<RawSdpGroup> &groups,
                                                                     const std::optional<std::string> &first_mid,
                                                                     const std::optional<std::string> &second_mid) {
    if (!first_mid.has_value() || !second_mid.has_value()) {
        return false;
    }

    if (*first_mid == *second_mid) {
        return false;
    }

    for (const auto &group : groups) {
        if (group.semantics != "DUP") {
            continue;
        }

        if (raw_sdp_group_contains_mid(group, *first_mid) && raw_sdp_group_contains_mid(group, *second_mid)) {
            return true;
        }
    }

    return false;
}

[[nodiscard]] inline Error set_raw_sdp_scoped_timing_attribute(std::optional<RawSdpScopedAttributeValue> &scoped_slot,
                                                               std::optional<std::string> &resolved_slot,
                                                               std::string_view value, RawSdpAttributeScope scope) {
    if (scoped_slot.has_value()) {
        return Error::InvalidValue;
    }

    scoped_slot = RawSdpScopedAttributeValue{.value = std::string(value), .scope = scope};

    // Compatibility resolved field:
    // - session fills it if no media-level override exists yet;
    // - media always overrides session.
    if (scope == RawSdpAttributeScope::Media || !resolved_slot.has_value()) {
        resolved_slot = std::string(value);
    }

    return Error::Ok;
}

[[nodiscard]] inline std::expected<RawVideoSdpMediaSection, Error>
select_raw_video_sdp_media_section(std::string_view sdp, uint8_t expected_payload_type) {
    RawVideoSdpMediaSection res{};
    bool found = false;

    bool seen_any_media_section = false;
    bool inside_selected_media_section = false;

    bool found_rtpmap = false;
    bool found_fmtp = false;

    bool inside_duplicate_candidate = false;
    bool duplicate_found_rtpmap = false;
    bool duplicate_found_fmtp = false;
    RawVideoSdpDuplicateMediaCandidate duplicate_candidate{};

    auto finish_duplicate_candidate = [&]() -> Error {
        if (!inside_duplicate_candidate) {
            return Error::Ok;
        }

        if (!duplicate_found_rtpmap || !duplicate_found_fmtp) {
            return Error::InvalidValue;
        }

        if (!has_dup_session_group_containing_both_mids(res.session_groups, res.mid, duplicate_candidate.mid)) {
            return Error::InvalidValue;
        }

        res.duplicate_candidates.push_back(std::move(duplicate_candidate));

        duplicate_candidate = RawVideoSdpDuplicateMediaCandidate{};
        duplicate_found_rtpmap = false;
        duplicate_found_fmtp = false;
        inside_duplicate_candidate = false;

        return Error::Ok;
    };

    std::size_t line_start = 0;

    while (line_start <= sdp.size()) {
        std::size_t line_end = sdp.find('\n', line_start);

        if (line_end == std::string_view::npos) {
            line_end = sdp.size();
        }

        std::string_view line = sdp.substr(line_start, line_end - line_start);
        line = strip_cr(line);

        if (line.starts_with("m=")) {
            if (Error err = finish_duplicate_candidate(); err != Error::Ok) {
                return std::unexpected(err);
            }
            seen_any_media_section = true;
            inside_selected_media_section = false;

            if (line.starts_with("m=video")) {
                auto payload_types = parse_video_m_line_payload_types(line);

                if (!payload_types.has_value()) {
                    return std::unexpected(payload_types.error());
                }

                if (contains_payload_type(*payload_types, expected_payload_type)) {
                    if (found) {
                        duplicate_candidate =
                            RawVideoSdpDuplicateMediaCandidate{.media_line = std::string(line),
                                                               .payload_type = expected_payload_type,
                                                               .media_payload_types = std::move(*payload_types)};

                        duplicate_found_rtpmap = false;
                        duplicate_found_fmtp = false;
                        inside_selected_media_section = false;
                        inside_duplicate_candidate = true;
                    } else {
                        res.media_line = std::string(line);
                        res.payload_type = expected_payload_type;
                        res.media_payload_types = std::move(*payload_types);

                        found = true;
                        inside_selected_media_section = true;
                    }
                }
            }
        } else if (!seen_any_media_section) {
            if (line.starts_with("c=")) {
                if (res.session_connection.has_value()) {
                    return std::unexpected(Error::InvalidValue);
                }

                auto parsed_connection = parse_connection_data(line);

                if (!parsed_connection.has_value()) {
                    return std::unexpected(parsed_connection.error());
                }

                res.session_connection = std::move(*parsed_connection);
            } else {
                bool handled_attribute = false;

                auto ts_refclk = parse_attribute_value(line, "a=ts-refclk:");

                if (ts_refclk.has_value()) {
                    if (Error err = set_raw_sdp_scoped_timing_attribute(res.session_ts_refclk, res.ts_refclk,
                                                                        *ts_refclk, RawSdpAttributeScope::Session);
                        err != Error::Ok) {
                        return std::unexpected(err);
                    }

                    handled_attribute = true;
                }

                auto mediaclk = parse_attribute_value(line, "a=mediaclk:");

                if (mediaclk.has_value()) {
                    if (Error err = set_raw_sdp_scoped_timing_attribute(res.session_mediaclk, res.mediaclk, *mediaclk,
                                                                        RawSdpAttributeScope::Session);
                        err != Error::Ok) {
                        return std::unexpected(err);
                    }

                    handled_attribute = true;
                }

                auto tsmode = parse_attribute_value(line, "a=tsmode:");
                if (!tsmode.has_value()) {
                    tsmode = parse_attribute_value(line, "a=TSMODE:");
                }

                if (tsmode.has_value()) {
                    if (Error err = set_raw_sdp_scoped_timing_attribute(res.session_tsmode, res.tsmode, *tsmode,
                                                                        RawSdpAttributeScope::Session);
                        err != Error::Ok) {
                        return std::unexpected(err);
                    }

                    handled_attribute = true;
                }

                auto tsdelay = parse_attribute_value(line, "a=tsdelay:");
                if (!tsdelay.has_value()) {
                    tsdelay = parse_attribute_value(line, "a=TSDELAY:");
                }

                if (tsdelay.has_value()) {
                    if (Error err = set_raw_sdp_scoped_timing_attribute(res.session_tsdelay, res.tsdelay, *tsdelay,
                                                                        RawSdpAttributeScope::Session);
                        err != Error::Ok) {
                        return std::unexpected(err);
                    }

                    handled_attribute = true;
                }

                auto tp = parse_attribute_value(line, "a=tp:");
                if (!tp.has_value()) {
                    tp = parse_attribute_value(line, "a=TP:");
                }

                if (tp.has_value()) {
                    if (Error err = set_raw_sdp_scoped_timing_attribute(res.session_tp, res.tp, *tp,
                                                                        RawSdpAttributeScope::Session);
                        err != Error::Ok) {
                        return std::unexpected(err);
                    }

                    handled_attribute = true;
                }

                auto troff = parse_attribute_value(line, "a=troff:");
                if (!troff.has_value()) {
                    troff = parse_attribute_value(line, "a=TROFF:");
                }

                if (troff.has_value()) {
                    if (Error err = set_raw_sdp_scoped_timing_attribute(res.session_troff, res.troff, *troff,
                                                                        RawSdpAttributeScope::Session);
                        err != Error::Ok) {
                        return std::unexpected(err);
                    }

                    handled_attribute = true;
                }

                auto cmax = parse_attribute_value(line, "a=cmax:");
                if (!cmax.has_value()) {
                    cmax = parse_attribute_value(line, "a=CMAX:");
                }

                if (cmax.has_value()) {
                    if (Error err = set_raw_sdp_scoped_timing_attribute(res.session_cmax, res.cmax, *cmax,
                                                                        RawSdpAttributeScope::Session);
                        err != Error::Ok) {
                        return std::unexpected(err);
                    }

                    handled_attribute = true;
                }

                auto group = parse_attribute_value(line, "a=group:");

                if (group.has_value()) {
                    auto parsed_group = parse_group_attribute(*group);

                    if (!parsed_group.has_value()) {
                        return std::unexpected(parsed_group.error());
                    }

                    res.session_groups.push_back(std::move(*parsed_group));
                } else {
                    auto source_filter = parse_attribute_value(line, "a=source-filter:");

                    if (source_filter.has_value()) {
                        auto parsed_source_filter =
                            parse_source_filter_attribute_value(*source_filter, RawSdpSourceFilter::Scope::Session);

                        if (!parsed_source_filter.has_value()) {
                            return std::unexpected(parsed_source_filter.error());
                        }

                        res.source_filters.push_back(std::move(*parsed_source_filter));
                    } else if (line.starts_with("a=") && !handled_attribute) {
                        res.unknown_session_attributes.push_back(parse_unknown_sdp_attribute(line));
                    }
                }
            }
        } else if (inside_selected_media_section) {
            if (line.starts_with("c=")) {
                if (res.media_connection.has_value()) {
                    return std::unexpected(Error::InvalidValue);
                }

                auto parsed_connection = parse_connection_data(line);

                if (!parsed_connection.has_value()) {
                    return std::unexpected(parsed_connection.error());
                }

                res.media_connection = std::move(*parsed_connection);
            } else {
                bool handled_attribute = false;

                auto rtpmap = parse_payload_bound_attribute_value(line, "a=rtpmap:", expected_payload_type);

                if (!rtpmap.has_value()) {
                    return std::unexpected(rtpmap.error());
                }

                if (rtpmap->has_value()) {
                    if (found_rtpmap) {
                        return std::unexpected(Error::InvalidValue);
                    }

                    res.rtpmap = std::string(**rtpmap);
                    found_rtpmap = true;
                    handled_attribute = true;
                }

                auto fmtp = parse_payload_bound_attribute_value(line, "a=fmtp:", expected_payload_type);

                if (!fmtp.has_value()) {
                    return std::unexpected(fmtp.error());
                }

                if (fmtp->has_value()) {
                    if (found_fmtp) {
                        return std::unexpected(Error::InvalidValue);
                    }

                    res.fmtp = std::string(**fmtp);
                    found_fmtp = true;
                    handled_attribute = true;
                }

                auto ts_refclk = parse_attribute_value(line, "a=ts-refclk:");

                if (ts_refclk.has_value()) {
                    if (Error err = set_raw_sdp_scoped_timing_attribute(res.media_ts_refclk, res.ts_refclk, *ts_refclk,
                                                                        RawSdpAttributeScope::Media);
                        err != Error::Ok) {
                        return std::unexpected(err);
                    }

                    handled_attribute = true;
                }

                auto mediaclk = parse_attribute_value(line, "a=mediaclk:");

                if (mediaclk.has_value()) {
                    if (Error err = set_raw_sdp_scoped_timing_attribute(res.media_mediaclk, res.mediaclk, *mediaclk,
                                                                        RawSdpAttributeScope::Media);
                        err != Error::Ok) {
                        return std::unexpected(err);
                    }

                    handled_attribute = true;
                }

                auto tsmode = parse_attribute_value(line, "a=tsmode:");
                if (!tsmode.has_value()) {
                    tsmode = parse_attribute_value(line, "a=TSMODE:");
                }

                if (tsmode.has_value()) {
                    if (Error err = set_raw_sdp_scoped_timing_attribute(res.media_tsmode, res.tsmode, *tsmode,
                                                                        RawSdpAttributeScope::Media);
                        err != Error::Ok) {
                        return std::unexpected(err);
                    }

                    handled_attribute = true;
                }

                auto tsdelay = parse_attribute_value(line, "a=tsdelay:");
                if (!tsdelay.has_value()) {
                    tsdelay = parse_attribute_value(line, "a=TSDELAY:");
                }

                if (tsdelay.has_value()) {
                    if (Error err = set_raw_sdp_scoped_timing_attribute(res.media_tsdelay, res.tsdelay, *tsdelay,
                                                                        RawSdpAttributeScope::Media);
                        err != Error::Ok) {
                        return std::unexpected(err);
                    }

                    handled_attribute = true;
                }

                auto tp = parse_attribute_value(line, "a=tp:");
                if (!tp.has_value()) {
                    tp = parse_attribute_value(line, "a=TP:");
                }

                if (tp.has_value()) {
                    if (Error err =
                            set_raw_sdp_scoped_timing_attribute(res.media_tp, res.tp, *tp, RawSdpAttributeScope::Media);
                        err != Error::Ok) {
                        return std::unexpected(err);
                    }

                    handled_attribute = true;
                }

                auto troff = parse_attribute_value(line, "a=troff:");
                if (!troff.has_value()) {
                    troff = parse_attribute_value(line, "a=TROFF:");
                }

                if (troff.has_value()) {
                    if (Error err = set_raw_sdp_scoped_timing_attribute(res.media_troff, res.troff, *troff,
                                                                        RawSdpAttributeScope::Media);
                        err != Error::Ok) {
                        return std::unexpected(err);
                    }

                    handled_attribute = true;
                }

                auto cmax = parse_attribute_value(line, "a=cmax:");
                if (!cmax.has_value()) {
                    cmax = parse_attribute_value(line, "a=CMAX:");
                }

                if (cmax.has_value()) {
                    if (Error err = set_raw_sdp_scoped_timing_attribute(res.media_cmax, res.cmax, *cmax,
                                                                        RawSdpAttributeScope::Media);
                        err != Error::Ok) {
                        return std::unexpected(err);
                    }

                    handled_attribute = true;
                }

                auto source_filter = parse_attribute_value(line, "a=source-filter:");

                if (source_filter.has_value()) {
                    auto parsed_source_filter =
                        parse_source_filter_attribute_value(*source_filter, RawSdpSourceFilter::Scope::Media);

                    if (!parsed_source_filter.has_value()) {
                        return std::unexpected(parsed_source_filter.error());
                    }

                    res.source_filters.push_back(std::move(*parsed_source_filter));
                    handled_attribute = true;
                }

                auto mid = parse_attribute_value(line, "a=mid:");

                if (mid.has_value()) {
                    const auto value = trim_left_ws(*mid);

                    if (res.mid.has_value() || value.empty()) {
                        return std::unexpected(Error::InvalidValue);
                    }

                    res.mid = std::string(value);
                    handled_attribute = true;
                }

                if (line.starts_with("a=") && !handled_attribute) {
                    res.unknown_attributes.push_back(parse_unknown_sdp_attribute(line));
                }
            }
        } else if (inside_duplicate_candidate) {
            if (line.starts_with("c=")) {
                if (duplicate_candidate.media_connection.has_value()) {
                    return std::unexpected(Error::InvalidValue);
                }

                auto parsed_connection = parse_connection_data(line);

                if (!parsed_connection.has_value()) {
                    return std::unexpected(parsed_connection.error());
                }

                duplicate_candidate.media_connection = std::move(*parsed_connection);
            } else {
                auto rtpmap = parse_payload_bound_attribute_value(line, "a=rtpmap:", expected_payload_type);

                if (!rtpmap.has_value()) {
                    return std::unexpected(rtpmap.error());
                }

                if (rtpmap->has_value()) {
                    if (duplicate_found_rtpmap) {
                        return std::unexpected(Error::InvalidValue);
                    }

                    duplicate_candidate.rtpmap = std::string(**rtpmap);
                    duplicate_found_rtpmap = true;
                }

                auto fmtp = parse_payload_bound_attribute_value(line, "a=fmtp:", expected_payload_type);

                if (!fmtp.has_value()) {
                    return std::unexpected(fmtp.error());
                }

                if (fmtp->has_value()) {
                    if (duplicate_found_fmtp) {
                        return std::unexpected(Error::InvalidValue);
                    }

                    duplicate_candidate.fmtp = std::string(**fmtp);
                    duplicate_found_fmtp = true;
                }

                auto source_filter = parse_attribute_value(line, "a=source-filter:");

                if (source_filter.has_value()) {
                    auto parsed_source_filter =
                        parse_source_filter_attribute_value(*source_filter, RawSdpSourceFilter::Scope::Media);

                    if (!parsed_source_filter.has_value()) {
                        return std::unexpected(parsed_source_filter.error());
                    }

                    duplicate_candidate.source_filters.push_back(std::move(*parsed_source_filter));
                }

                auto mid = parse_attribute_value(line, "a=mid:");

                if (mid.has_value()) {
                    const auto value = trim_left_ws(*mid);

                    if (duplicate_candidate.mid.has_value() || value.empty()) {
                        return std::unexpected(Error::InvalidValue);
                    }

                    duplicate_candidate.mid = std::string(value);
                }
            }
        }

        if (line_end == sdp.size()) {
            break;
        }

        line_start = line_end + 1;
    }

    if (Error err = finish_duplicate_candidate(); err != Error::Ok) {
        return std::unexpected(err);
    }

    if (!found || !found_rtpmap || !found_fmtp) {
        return std::unexpected(Error::InvalidValue);
    }

    return res;
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_VIDEO_SDP_MEDIA_SECTION_HPP
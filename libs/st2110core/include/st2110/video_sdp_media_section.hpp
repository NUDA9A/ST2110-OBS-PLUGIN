#ifndef ST2110_OBS_PLUGIN_VIDEO_SDP_MEDIA_SECTION_HPP
#define ST2110_OBS_PLUGIN_VIDEO_SDP_MEDIA_SECTION_HPP

#include <charconv>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <utility>
#include <system_error>

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
    };

    struct RawSdpSourceFilter {
        std::string raw_value{};
    };

    struct RawSdpGroup {
        std::string semantics{};
        std::vector<std::string> mids{};
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
        std::optional<RawSdpConnectionData> session_connection{};
        std::optional<RawSdpConnectionData> media_connection{};

        std::optional<std::string> mid{};
        std::vector<RawSdpSourceFilter> source_filters{};
        std::vector<RawSdpGroup> session_groups{};

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

        const char* first = text.data();
        const char* last = text.data() + text.size();

        const auto [ptr, ec] = std::from_chars(first, last, value);

        if (ec != std::errc{} || ptr != last || value > 127) {
            return std::nullopt;
        }

        return static_cast<uint8_t>(value);
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

        std::vector<uint8_t> payload_types{};
        payload_types.reserve(tokens.size() - 3);

        for (std::size_t i = 3; i < tokens.size(); ++i) {
            const auto payload_type = parse_payload_type(tokens[i]);

            if (!payload_type.has_value()) {
                return std::unexpected(Error::InvalidValue);
            }

            payload_types.push_back(*payload_type);
        }

        return payload_types;
    }

    [[nodiscard]] inline bool contains_payload_type(
            const std::vector<uint8_t>& payload_types,
            uint8_t expected_payload_type
    ) {
        for (const uint8_t payload_type : payload_types) {
            if (payload_type == expected_payload_type) {
                return true;
            }
        }

        return false;
    }

    [[nodiscard]] inline std::expected<std::optional<std::string_view>, Error>
    parse_payload_bound_attribute_value(
            std::string_view line,
            std::string_view prefix,
            uint8_t expected_payload_type
    ) {
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

    [[nodiscard]] inline std::optional<std::string_view>
    parse_attribute_value(std::string_view line, std::string_view prefix) {
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
                return RawSdpAttribute{
                        .name = std::string(line),
                        .value = ""
                };
            }

            return RawSdpAttribute{
                    .name = std::string(line.substr(0, name_end)),
                    .value = std::string(trim_left_ws(line.substr(name_end)))
            };
        }

        const std::size_t colon_pos = line.find(':');

        if (colon_pos == std::string_view::npos) {
            return RawSdpAttribute{
                    .name = std::string(line),
                    .value = ""
            };
        }

        return RawSdpAttribute{
                .name = std::string(line.substr(0, colon_pos)),
                .value = std::string(line.substr(colon_pos + 1))
        };
    }

    [[nodiscard]] inline std::expected<RawSdpConnectionData, Error>
    parse_connection_data(std::string_view line) {
        line = strip_cr(line);

        if (!line.starts_with("c=")) {
            return std::unexpected(Error::InvalidValue);
        }

        const auto tokens = split_ws(line.substr(2));

        // c=<nettype> <addrtype> <connection-address>
        if (tokens.size() != 3) {
            return std::unexpected(Error::InvalidValue);
        }

        return RawSdpConnectionData{
                .network_type = std::string(tokens[0]),
                .address_type = std::string(tokens[1]),
                .connection_address = std::string(tokens[2])
        };
    }

    [[nodiscard]] inline std::expected<RawSdpGroup, Error>
    parse_group_attribute(std::string_view value) {
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

    [[nodiscard]] inline bool has_dup_session_group(const std::vector<RawSdpGroup>& groups) {
        for (const auto& group : groups) {
            if (group.semantics == "DUP") {
                return true;
            }
        }

        return false;
    }

    [[nodiscard]] inline std::expected<RawVideoSdpMediaSection, Error>
    select_raw_video_sdp_media_section(std::string_view sdp, uint8_t expected_payload_type) {
        RawVideoSdpMediaSection res{};
        bool found = false;

        bool seen_any_media_section = false;
        bool inside_selected_media_section = false;

        bool found_rtpmap = false;
        bool found_fmtp = false;

        std::size_t line_start = 0;

        while (line_start <= sdp.size()) {
            std::size_t line_end = sdp.find('\n', line_start);

            if (line_end == std::string_view::npos) {
                line_end = sdp.size();
            }

            std::string_view line = sdp.substr(line_start, line_end - line_start);
            line = strip_cr(line);

            if (line.starts_with("m=")) {
                seen_any_media_section = true;
                inside_selected_media_section = false;

                if (line.starts_with("m=video")) {
                    auto payload_types = parse_video_m_line_payload_types(line);

                    if (!payload_types.has_value()) {
                        return std::unexpected(payload_types.error());
                    }

                    if (contains_payload_type(*payload_types, expected_payload_type)) {
                        if (found) {
                            // Redundant RTP descriptions may carry the same payload type
                            // in multiple video media sections grouped by a=session-level
                            // group:DUP. This raw SDP boundary does not implement redundant
                            // stream selection yet; keep the first selected media section as
                            // the current primary/default section and ignore later matching
                            // sections instead of failing SDP ingestion.
                            if (!has_dup_session_group(res.session_groups)) {
                                return std::unexpected(Error::InvalidValue);
                            }

                            inside_selected_media_section = false;
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
                            const auto value = trim_left_ws(*source_filter);

                            if (value.empty()) {
                                return std::unexpected(Error::InvalidValue);
                            }

                            res.source_filters.push_back(RawSdpSourceFilter{
                                    .raw_value = std::string(value)
                            });
                        } else if (line.starts_with("a=")) {
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

                    auto rtpmap = parse_payload_bound_attribute_value(
                            line,
                            "a=rtpmap:",
                            expected_payload_type
                    );

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

                    auto fmtp = parse_payload_bound_attribute_value(
                            line,
                            "a=fmtp:",
                            expected_payload_type
                    );

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
                        if (res.ts_refclk.has_value()) {
                            return std::unexpected(Error::InvalidValue);
                        }

                        res.ts_refclk = std::string(*ts_refclk);
                        handled_attribute = true;
                    }

                    auto mediaclk = parse_attribute_value(line, "a=mediaclk:");

                    if (mediaclk.has_value()) {
                        if (res.mediaclk.has_value()) {
                            return std::unexpected(Error::InvalidValue);
                        }

                        res.mediaclk = std::string(*mediaclk);
                        handled_attribute = true;
                    }

                    auto tsmode = parse_attribute_value(line, "a=tsmode:");

                    if (tsmode.has_value()) {
                        if (res.tsmode.has_value()) {
                            return std::unexpected(Error::InvalidValue);
                        }

                        res.tsmode = std::string(*tsmode);
                        handled_attribute = true;
                    }

                    auto tsdelay = parse_attribute_value(line, "a=tsdelay:");

                    if (tsdelay.has_value()) {
                        if (res.tsdelay.has_value()) {
                            return std::unexpected(Error::InvalidValue);
                        }

                        res.tsdelay = std::string(*tsdelay);
                        handled_attribute = true;
                    }

                    auto tp = parse_attribute_value(line, "a=tp:");
                    if (!tp.has_value()) {
                        tp = parse_attribute_value(line, "a=TP:");
                    }

                    if (tp.has_value()) {
                        if (res.tp.has_value()) {
                            return std::unexpected(Error::InvalidValue);
                        }

                        res.tp = std::string(*tp);
                        handled_attribute = true;
                    }

                    auto troff = parse_attribute_value(line, "a=troff:");
                    if (!troff.has_value()) {
                        troff = parse_attribute_value(line, "a=TROFF:");
                    }

                    if (troff.has_value()) {
                        if (res.troff.has_value()) {
                            return std::unexpected(Error::InvalidValue);
                        }

                        res.troff = std::string(*troff);
                        handled_attribute = true;
                    }

                    auto cmax = parse_attribute_value(line, "a=cmax:");
                    if (!cmax.has_value()) {
                        cmax = parse_attribute_value(line, "a=CMAX:");
                    }

                    if (cmax.has_value()) {
                        if (res.cmax.has_value()) {
                            return std::unexpected(Error::InvalidValue);
                        }

                        res.cmax = std::string(*cmax);
                        handled_attribute = true;
                    }

                    auto source_filter = parse_attribute_value(line, "a=source-filter:");

                    if (source_filter.has_value()) {
                        const auto value = trim_left_ws(*source_filter);

                        if (value.empty()) {
                            return std::unexpected(Error::InvalidValue);
                        }

                        res.source_filters.push_back(RawSdpSourceFilter{
                                .raw_value = std::string(value)
                        });
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
            }

            if (line_end == sdp.size()) {
                break;
            }

            line_start = line_end + 1;
        }

        if (!found || !found_rtpmap || !found_fmtp) {
            return std::unexpected(Error::InvalidValue);
        }

        return res;
    }
}

#endif //ST2110_OBS_PLUGIN_VIDEO_SDP_MEDIA_SECTION_HPP
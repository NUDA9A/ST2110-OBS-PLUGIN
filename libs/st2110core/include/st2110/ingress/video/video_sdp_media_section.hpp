#ifndef ST2110_OBS_PLUGIN_VIDEO_SDP_MEDIA_SECTION_HPP
#define ST2110_OBS_PLUGIN_VIDEO_SDP_MEDIA_SECTION_HPP

#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <st2110/foundation/error.hpp>
#include <st2110/ingress/shared/sdp_common.hpp>

namespace st2110 {
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

struct RawVideoSdpMediaSection : RawSdpCommonParameters {
    std::string media_line{};
    uint8_t payload_type = 0;
    std::vector<uint8_t> media_payload_types{};

    std::string rtpmap{};
    std::string fmtp{};

    std::optional<std::string> tp{};
    std::optional<std::string> troff{};
    std::optional<std::string> cmax{};

    std::optional<RawSdpScopedAttributeValue> session_tp{};
    std::optional<RawSdpScopedAttributeValue> media_tp{};
    std::optional<RawSdpScopedAttributeValue> session_troff{};
    std::optional<RawSdpScopedAttributeValue> media_troff{};
    std::optional<RawSdpScopedAttributeValue> session_cmax{};
    std::optional<RawSdpScopedAttributeValue> media_cmax{};

    std::vector<RawVideoSdpDuplicateMediaCandidate> duplicate_candidates{};

    std::vector<RawSdpAttribute> unknown_session_attributes{};
    std::vector<RawSdpAttribute> unknown_attributes{};
};

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
            bool handled_attribute = false;

            if (const Error err =
                    parse_raw_sdp_common_line(line, res, RawSdpAttributeScope::Session, handled_attribute);
                err != Error::Ok) {
                return std::unexpected(err);
            }

            if (!handled_attribute) {
                auto tp = parse_attribute_value(line, "a=tp:");
                if (!tp.has_value()) {
                    tp = parse_attribute_value(line, "a=TP:");
                }

                if (tp.has_value()) {
                    if (Error err = set_raw_sdp_scoped_common_attribute(res.session_tp, res.tp, *tp,
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
                    if (Error err = set_raw_sdp_scoped_common_attribute(res.session_troff, res.troff, *troff,
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
                    if (Error err = set_raw_sdp_scoped_common_attribute(res.session_cmax, res.cmax, *cmax,
                                                                        RawSdpAttributeScope::Session);
                        err != Error::Ok) {
                        return std::unexpected(err);
                    }

                    handled_attribute = true;
                }
            }

            if (line.starts_with("a=") && !handled_attribute) {
                res.unknown_session_attributes.push_back(parse_unknown_sdp_attribute(line));
            }
        } else if (inside_selected_media_section) {
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

            if (!handled_attribute) {
                if (const Error err =
                        parse_raw_sdp_common_line(line, res, RawSdpAttributeScope::Media, handled_attribute);
                    err != Error::Ok) {
                    return std::unexpected(err);
                }
            }

            if (!handled_attribute) {
                auto tp = parse_attribute_value(line, "a=tp:");
                if (!tp.has_value()) {
                    tp = parse_attribute_value(line, "a=TP:");
                }

                if (tp.has_value()) {
                    if (Error err =
                            set_raw_sdp_scoped_common_attribute(res.media_tp, res.tp, *tp, RawSdpAttributeScope::Media);
                        err != Error::Ok) {
                        return std::unexpected(err);
                    }

                    handled_attribute = true;
                }
            }

            if (!handled_attribute) {
                auto troff = parse_attribute_value(line, "a=troff:");
                if (!troff.has_value()) {
                    troff = parse_attribute_value(line, "a=TROFF:");
                }

                if (troff.has_value()) {
                    if (Error err = set_raw_sdp_scoped_common_attribute(res.media_troff, res.troff, *troff,
                                                                        RawSdpAttributeScope::Media);
                        err != Error::Ok) {
                        return std::unexpected(err);
                    }

                    handled_attribute = true;
                }
            }

            if (!handled_attribute) {
                auto cmax = parse_attribute_value(line, "a=cmax:");
                if (!cmax.has_value()) {
                    cmax = parse_attribute_value(line, "a=CMAX:");
                }

                if (cmax.has_value()) {
                    if (Error err = set_raw_sdp_scoped_common_attribute(res.media_cmax, res.cmax, *cmax,
                                                                        RawSdpAttributeScope::Media);
                        err != Error::Ok) {
                        return std::unexpected(err);
                    }

                    handled_attribute = true;
                }
            }

            if (line.starts_with("a=") && !handled_attribute) {
                res.unknown_attributes.push_back(parse_unknown_sdp_attribute(line));
            }
        } else if (inside_duplicate_candidate) {
            bool handled_attribute = false;

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
                handled_attribute = true;
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
                handled_attribute = true;
            }

            if (!handled_attribute) {
                if (const Error err = parse_raw_sdp_media_metadata_line(
                        line, duplicate_candidate.media_connection, duplicate_candidate.mid,
                        duplicate_candidate.source_filters, handled_attribute);
                    err != Error::Ok) {
                    return std::unexpected(err);
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
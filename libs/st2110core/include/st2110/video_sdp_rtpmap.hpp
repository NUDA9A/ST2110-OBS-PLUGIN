#ifndef ST2110_OBS_PLUGIN_VIDEO_SDP_RTPMAP_HPP
#define ST2110_OBS_PLUGIN_VIDEO_SDP_RTPMAP_HPP

#include "error.hpp"
#include "video_sdp_media_section.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <optional>
#include <expected>
#include <charconv>
#include <system_error>
#include <utility>

namespace st2110 {
    struct RawVideoSdpRtpMap {
        std::string encoding_name{};
        uint32_t clock_rate = 0;
        std::optional<std::string> encoding_parameters{};
    };

    [[nodiscard]] inline std::string_view trim_rtpmap_ascii_ws(std::string_view s) {
        const std::size_t first = s.find_first_not_of(" \t\n\r");
        if (first == std::string_view::npos) {
            return {};
        }

        const std::size_t last = s.find_last_not_of(" \t\n\r");
        return s.substr(first, last - first + 1);
    }


    [[nodiscard]] inline std::expected<RawVideoSdpRtpMap, Error>
    parse_video_sdp_rtpmap_payload(std::string_view payload) {
        payload = trim_rtpmap_ascii_ws(payload);

        if (payload.empty()) {
            return std::unexpected(Error::InvalidValue);
        }

        const std::size_t first_slash = payload.find('/');

        if (first_slash == std::string_view::npos || first_slash == 0) {
            return std::unexpected(Error::InvalidValue);
        }

        RawVideoSdpRtpMap res{};
        res.encoding_name = std::string(payload.substr(0, first_slash));

        payload.remove_prefix(first_slash + 1);

        if (payload.empty()) {
            return std::unexpected(Error::InvalidValue);
        }

        const std::size_t second_slash = payload.find('/');

        const std::string_view clock_rate_text =
                second_slash == std::string_view::npos
                ? payload
                : payload.substr(0, second_slash);

        if (clock_rate_text.empty()) {
            return std::unexpected(Error::InvalidValue);
        }

        uint32_t clock_rate = 0;

        const char* first = clock_rate_text.data();
        const char* last = clock_rate_text.data() + clock_rate_text.size();

        const auto [ptr, ec] = std::from_chars(first, last, clock_rate);

        if (ec != std::errc{} || ptr != last || clock_rate == 0) {
            return std::unexpected(Error::InvalidValue);
        }

        res.clock_rate = clock_rate;

        if (second_slash != std::string_view::npos) {
            payload.remove_prefix(second_slash + 1);

            if (payload.empty() || payload.find('/') != std::string_view::npos) {
                return std::unexpected(Error::InvalidValue);
            }

            res.encoding_parameters = std::string(payload);
        }

        return res;
    }

    [[nodiscard]] inline std::expected<std::optional<std::string_view>, Error>
    parse_rtpmap_attribute_payload_for_pt(std::string_view line, uint8_t expected_payload_type) {
        line = strip_cr(line);

        constexpr std::string_view prefix = "a=rtpmap:";

        if (!line.starts_with(prefix)) {
            return std::optional<std::string_view>{};
        }

        std::string_view tail = line.substr(prefix.size());

        const std::size_t pt_end = tail.find_first_of(" \t");
        const std::string_view pt_text =
                pt_end == std::string_view::npos ? tail : tail.substr(0, pt_end);

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

    [[nodiscard]] inline std::expected<std::optional<RawVideoSdpRtpMap>, Error>
    parse_video_sdp_rtpmap_attribute(std::string_view line, uint8_t expected_payload_type) {
        auto payload = parse_rtpmap_attribute_payload_for_pt(line, expected_payload_type);

        if (!payload.has_value()) {
            return std::unexpected(payload.error());
        }

        if (!payload->has_value()) {
            return std::optional<RawVideoSdpRtpMap>{};
        }

        auto parsed = parse_video_sdp_rtpmap_payload(**payload);

        if (!parsed.has_value()) {
            return std::unexpected(parsed.error());
        }

        return std::optional<RawVideoSdpRtpMap>{std::move(*parsed)};
    }

    [[nodiscard]] inline std::expected<RawVideoSdpRtpMap, Error>
    parse_video_sdp_rtpmap_from_media_section(const RawVideoSdpMediaSection& raw) {
        if (raw.rtpmap.empty()) {
            return std::unexpected(Error::InvalidValue);
        }

        if (!contains_payload_type(raw.media_payload_types, raw.payload_type)) {
            return std::unexpected(Error::InvalidValue);
        }

        auto parsed = parse_video_sdp_rtpmap_payload(raw.rtpmap);

        if (!parsed.has_value()) {
            return std::unexpected(parsed.error());
        }

        return *parsed;
    }
}

#endif //ST2110_OBS_PLUGIN_VIDEO_SDP_RTPMAP_HPP

//
// Created by vlaki on 24.04.2026.
//

#ifndef ST2110_OBS_PLUGIN_VIDEO_SDP_FMTP_HPP
#define ST2110_OBS_PLUGIN_VIDEO_SDP_FMTP_HPP

#include "error.hpp"
#include "video_sdp_media_section.hpp"

#include <cstdint>
#include <string>
#include <optional>
#include <vector>
#include <expected>
#include <string_view>
#include <ranges>
#include <charconv>
#include <memory>
#include <limits>
#include <system_error>
#include <utility>

namespace st2110 {
    struct RawVideoSdpExactFrameRate {
        uint32_t numerator = 0;
        uint32_t denominator = 1;
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
        std::string colorimetry{};
        std::string packing_mode{};
        std::string signal_standard{};
        std::optional<std::string> transfer_characteristic_system{};
        std::optional<std::string> range{};
        bool interlace = false;
        bool segmented = false;

        std::optional<std::string> timestamp_mode{};
        std::optional<uint64_t> ts_delay_sender_ticks{};
        std::optional<std::string> sender_type{};
        std::optional<uint32_t> troff_us{};
        std::optional<uint32_t> cmax{};

        std::vector<RawVideoSdpFmtpUnknownParameter> unknown_parameters{};
    };

    [[nodiscard]] inline std::string_view split_part_to_string_view(auto&& part) {
        const auto size = static_cast<std::size_t>(std::ranges::distance(part));

        if (size == 0) {
            return {};
        }

        return std::string_view{
                std::addressof(*std::ranges::begin(part)),
                size
        };
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

    [[nodiscard]] inline std::expected<uint64_t, Error>
    parse_fmtp_uint64(std::string_view value) {
        value = trim_ascii_ws(value);

        if (value.empty()) {
            return std::unexpected(Error::InvalidValue);
        }

        uint64_t out = 0;
        const char* first = value.data();
        const char* last = value.data() + value.size();

        const auto [ptr, ec] = std::from_chars(first, last, out);

        if (ec != std::errc{} || ptr != last) {
            return std::unexpected(Error::InvalidValue);
        }

        return out;
    }

    [[nodiscard]] inline std::expected<uint32_t, Error>
    parse_fmtp_uint32(std::string_view value) {
        auto parsed = parse_fmtp_uint64(value);

        if (!parsed.has_value()) {
            return std::unexpected(parsed.error());
        }

        if (*parsed > std::numeric_limits<uint32_t>::max()) {
            return std::unexpected(Error::InvalidValue);
        }

        return static_cast<uint32_t>(*parsed);
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

    [[nodiscard]] inline std::expected<RawVideoSdpFmtpParameters, Error>
    parse_video_sdp_fmtp_payload(std::string_view payload) {
        RawVideoSdpFmtpParameters res{};
        auto split_view = payload | std::views::split(';');
        std::optional<std::string> sampling;
        std::optional<uint32_t> width;
        std::optional<uint32_t> height;
        std::optional<RawVideoSdpExactFrameRate> exact_framerate;
        std::optional<uint16_t> depth;
        std::optional<std::string> colorimetry;
        std::optional<std::string> packing_mode;
        std::optional<std::string> signal_standard;
        std::optional<std::string> tcs;
        std::optional<std::string> range;
        std::optional<std::string> timestamp_mode;
        std::optional<uint64_t> ts_delay_sender_ticks;
        std::optional<std::string> sender_type;
        std::optional<uint32_t> troff_us;
        std::optional<uint32_t> cmax;
        bool interlace = false;
        bool segmented = false;


        for (auto elem : split_view) {
            std::string_view key_val_pair = trim_ascii_ws(split_part_to_string_view(elem));

            if (key_val_pair.empty()) {
                return std::unexpected(Error::InvalidValue);
            }

            if (key_val_pair.starts_with("sampling=")) {
                if (sampling.has_value()) {
                    return std::unexpected(Error::InvalidValue);
                }

                auto value = key_val_pair.substr(9);
                if (value.empty()) {
                    return std::unexpected(Error::InvalidValue);
                }
                sampling = std::string(value);
                continue;
            }
            if (key_val_pair.starts_with("width=")) {
                if (width.has_value()) {
                    return std::unexpected(Error::InvalidValue);
                }

                unsigned value = 0;

                const char* first = key_val_pair.data() + 6;
                const char* last = key_val_pair.data() + key_val_pair.size();

                const auto [ptr, ec] = std::from_chars(first, last, value);

                if (ec != std::errc{} || ptr != last || value == 0) {
                    return std::unexpected(Error::InvalidValue);
                }

                width = static_cast<uint32_t>(value);
                continue;
            }
            if (key_val_pair.starts_with("height=")) {
                if (height.has_value()) {
                    return std::unexpected(Error::InvalidValue);
                }

                unsigned value = 0;

                const char* first = key_val_pair.data() + 7;
                const char* last = key_val_pair.data() + key_val_pair.size();

                const auto [ptr, ec] = std::from_chars(first, last, value);

                if (ec != std::errc{} || ptr != last || value == 0) {
                    return std::unexpected(Error::InvalidValue);
                }

                height = static_cast<uint32_t>(value);
                continue;
            }
            if (key_val_pair.starts_with("exactframerate=")) {
                if (exact_framerate.has_value()) {
                    return std::unexpected(Error::InvalidValue);
                }

                exact_framerate = RawVideoSdpExactFrameRate{};
                key_val_pair = key_val_pair.substr(15);
                std::size_t slash_pos = key_val_pair.find_first_of('/');
                if (slash_pos == std::string_view::npos) {
                    slash_pos = key_val_pair.size();
                }
                auto numerator_str = key_val_pair.substr(0, slash_pos);
                unsigned value = 0;

                const char* first = numerator_str.data();
                const char* last = numerator_str.data() + numerator_str.size();

                const auto [ptr, ec] = std::from_chars(first, last, value);

                if (ec != std::errc{} || ptr != last || value == 0) {
                    return std::unexpected(Error::InvalidValue);
                }

                exact_framerate->numerator = static_cast<uint32_t>(value);
                if (slash_pos == key_val_pair.size()) {
                    continue;
                }

                auto denum_str = key_val_pair.substr(slash_pos + 1);

                value = 0;
                first = denum_str.data();
                last = denum_str.data() + denum_str.size();

                const auto [ptr2, ec2] = std::from_chars(first, last, value);

                if (ec2 != std::errc{} || ptr2 != last || value == 0) {
                    return std::unexpected(Error::InvalidValue);
                }

                exact_framerate->denominator = static_cast<uint32_t>(value);
                continue;
            }
            if (key_val_pair.starts_with("depth=")) {
                if (depth.has_value()) {
                    return std::unexpected(Error::InvalidValue);
                }

                unsigned value = 0;

                const char* first = key_val_pair.data() + 6;
                const char* last = key_val_pair.data() + key_val_pair.size();

                const auto [ptr, ec] = std::from_chars(first, last, value);

                if (ec != std::errc{} || ptr != last || value == 0) {
                    return std::unexpected(Error::InvalidValue);
                }

                if (value > std::numeric_limits<uint16_t>::max()) {
                    return std::unexpected(Error::InvalidValue);
                }

                depth = static_cast<uint16_t>(value);
                continue;
            }
            if (key_val_pair.starts_with("colorimetry=")) {
                if (colorimetry.has_value()) {
                    return std::unexpected(Error::InvalidValue);
                }

                auto value = key_val_pair.substr(12);
                if (value.empty()) {
                    return std::unexpected(Error::InvalidValue);
                }
                colorimetry = std::string(value);
                continue;
            }
            if (key_val_pair.starts_with("PM=")) {
                if (packing_mode.has_value()) {
                    return std::unexpected(Error::InvalidValue);
                }

                auto value = key_val_pair.substr(3);
                if (value.empty()) {
                    return std::unexpected(Error::InvalidValue);
                }
                packing_mode = std::string(value);
                continue;
            }
            if (key_val_pair.starts_with("SSN=")) {
                if (signal_standard.has_value()) {
                    return std::unexpected(Error::InvalidValue);
                }

                auto value = key_val_pair.substr(4);
                if (value.empty()) {
                    return std::unexpected(Error::InvalidValue);
                }
                signal_standard = std::string(value);
                continue;
            }
            if (key_val_pair.starts_with("TCS=")) {
                if (tcs.has_value()) {
                    return std::unexpected(Error::InvalidValue);
                }

                auto value = key_val_pair.substr(4);
                if (value.empty()) {
                    return std::unexpected(Error::InvalidValue);
                }
                tcs = std::string(value);
                continue;
            }
            if (key_val_pair.starts_with("RANGE=")) {
                if (range.has_value()) {
                    return std::unexpected(Error::InvalidValue);
                }

                auto value = key_val_pair.substr(6);
                if (value.empty()) {
                    return std::unexpected(Error::InvalidValue);
                }
                range = std::string(value);
                continue;
            }
            if (key_val_pair.starts_with("TSMODE=")) {
                if (timestamp_mode.has_value()) {
                    return std::unexpected(Error::InvalidValue);
                }

                auto value = key_val_pair.substr(7);
                if (value.empty()) {
                    return std::unexpected(Error::InvalidValue);
                }

                timestamp_mode = std::string(value);
                continue;
            }

            if (key_val_pair.starts_with("TSDELAY=")) {
                if (ts_delay_sender_ticks.has_value()) {
                    return std::unexpected(Error::InvalidValue);
                }

                auto parsed = parse_fmtp_uint64(key_val_pair.substr(8));
                if (!parsed.has_value()) {
                    return std::unexpected(parsed.error());
                }

                ts_delay_sender_ticks = *parsed;
                continue;
            }

            if (key_val_pair.starts_with("TP=")) {
                if (sender_type.has_value()) {
                    return std::unexpected(Error::InvalidValue);
                }

                auto value = key_val_pair.substr(3);
                if (value.empty()) {
                    return std::unexpected(Error::InvalidValue);
                }

                sender_type = std::string(value);
                continue;
            }

            if (key_val_pair.starts_with("TROFF=")) {
                if (troff_us.has_value()) {
                    return std::unexpected(Error::InvalidValue);
                }

                auto parsed = parse_fmtp_uint32(key_val_pair.substr(6));
                if (!parsed.has_value()) {
                    return std::unexpected(parsed.error());
                }

                troff_us = *parsed;
                continue;
            }

            if (key_val_pair.starts_with("CMAX=")) {
                if (cmax.has_value()) {
                    return std::unexpected(Error::InvalidValue);
                }

                auto parsed = parse_fmtp_uint32(key_val_pair.substr(5));
                if (!parsed.has_value()) {
                    return std::unexpected(parsed.error());
                }

                cmax = *parsed;
                continue;
            }
            if (key_val_pair == "interlace") {
                if (interlace) {
                    return std::unexpected(Error::InvalidValue);
                }

                interlace = true;
                continue;
            }
            if (key_val_pair == "segmented") {
                if (segmented) {
                    return std::unexpected(Error::InvalidValue);
                }

                segmented = true;
                continue;
            }

            RawVideoSdpFmtpUnknownParameter unknown_parameter{};
            const std::size_t eq_pos = key_val_pair.find('=');
            if (eq_pos == 0) {
                return std::unexpected(Error::InvalidValue);
            }
            if (eq_pos == std::string_view::npos) {
                unknown_parameter.name = std::string(key_val_pair);
            } else {
                unknown_parameter.name = std::string(key_val_pair.substr(0, eq_pos));
                unknown_parameter.value = std::string(key_val_pair.substr(eq_pos + 1));
            }
            res.unknown_parameters.push_back(unknown_parameter);
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
}

#endif //ST2110_OBS_PLUGIN_VIDEO_SDP_FMTP_HPP

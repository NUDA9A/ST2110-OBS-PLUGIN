#ifndef ST2110_OBS_PLUGIN_VIDEO_SDP_TIMING_ATTRIBUTES_HPP
#define ST2110_OBS_PLUGIN_VIDEO_SDP_TIMING_ATTRIBUTES_HPP

#include "error.hpp"
#include "video_sdp_media_section.hpp"

#include <string>
#include <cstdint>
#include <optional>
#include <string_view>
#include <expected>
#include <charconv>
#inclue <utility>


namespace st2110 {
    struct RawVideoSdpPtpReferenceClock {
        std::string version{};
        std::string gmid{};
        std::optional<uint8_t> domain{};
    };

    struct RawVideoSdpReferenceClock {
        enum class Kind {
            Ptp,
            LocalMac,
            Other
        };

        Kind kind = Kind::Other;
        std::string raw_value{};
        std::optional<RawVideoSdpPtpReferenceClock> ptp{};
        std::optional<std::string> local_mac{};
    };

    struct RawVideoSdpMediaClock {
        enum class Kind {
            Direct,
            Sender,
            Other
        };

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

    struct RawVideoSdpTimingAttributes {
        std::optional<RawVideoSdpReferenceClock> reference_clock{};
        std::optional<RawVideoSdpMediaClock> media_clock{};
        std::optional<RawVideoSdpTimestampModeValue> timestamp_mode{};
        std::optional<uint64_t> ts_delay_sender_ticks{};
        std::optional<RawVideoSdpSenderTypeValue> sender_type{};
        std::optional<uint32_t> troff_us{};
        std::optional<uint32_t> cmax{};
    };

    [[nodiscard]] inline std::string_view trim(std::string_view s) {
        const std::size_t first = s.find_first_not_of(" \t\n\r");
        if (first == std::string_view::npos) {
            return {};
        }

        const std::size_t last = s.find_last_not_of(" \t\n\r");
        return s.substr(first, last - first + 1);
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
            if (first_colon == std::string_view::npos) {
                return std::unexpected(Error::InvalidValue);
            }

            RawVideoSdpPtpReferenceClock ptp{};
            ptp.version = std::string(value.substr(0, first_colon));
            if (ptp.version.empty()) {
                return std::unexpected(Error::InvalidValue);
            }

            value.remove_prefix(first_colon + 1);
            if (value.empty()) {
                return std::unexpected(Error::InvalidValue);
            }

            const std::size_t second_colon = value.find(':');
            if (second_colon == std::string_view::npos) {
                ptp.gmid = std::string(value);
            } else {
                ptp.gmid = std::string(value.substr(0, second_colon));
                value.remove_prefix(second_colon + 1);

                uint32_t domain = 0;
                const char* first = value.data();
                const char* last = value.data() + value.size();
                const auto [ptr, ec] = std::from_chars(first, last, domain);

                if (ec != std::errc{} || ptr != last || domain > 255) {
                    return std::unexpected(Error::InvalidValue);
                }

                ptp.domain = static_cast<uint8_t>(domain);
            }

            if (ptp.gmid.empty()) {
                return std::unexpected(Error::InvalidValue);
            }

            res.kind = RawVideoSdpReferenceClock::Kind::Ptp;
            res.ptp = std::move(ptp);
            return res;
        }

        if (value.starts_with("localmac=")) {
            value.remove_prefix(9);
            if (value.empty()) {
                return std::unexpected(Error::InvalidValue);
            }

            res.kind = RawVideoSdpReferenceClock::Kind::LocalMac;
            res.local_mac = std::string(value);
            return res;
        }

        res.kind = RawVideoSdpReferenceClock::Kind::Other;
        return res;
    }

    [[nodiscard]] inline std::expected<RawVideoSdpMediaClock, Error>
    parse_video_sdp_media_clock(std::string_view value) {
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
            const char* first = value.data();
            const char* last = value.data() + value.size();
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

    [[nodiscard]] inline std::expected<uint64_t, Error>
    parse_video_sdp_ts_delay(std::string_view value) {
        value = trim(value);
        if (value.empty()) {
            return std::unexpected(Error::InvalidValue);
        }
        uint64_t val = 0;

        const char* first = value.data();
        const char* last = value.data() + value.size();

        const auto [ptr, ec] = std::from_chars(first, last, val);

        if (ec != std::errc{} || ptr != last) {
            return std::unexpected(Error::InvalidValue);
        }

        return val;
    }

    [[nodiscard]] inline std::expected<uint32_t, Error>
    parse_video_sdp_troff(std::string_view value) {
        value = trim(value);
        if (value.empty()) {
            return std::unexpected(Error::InvalidValue);
        }

        uint32_t val = 0;
        const char* first = value.data();
        const char* last = value.data() + value.size();
        const auto [ptr, ec] = std::from_chars(first, last, val);

        if (ec != std::errc{} || ptr != last) {
            return std::unexpected(Error::InvalidValue);
        }

        return val;
    }

    [[nodiscard]] inline std::expected<uint32_t, Error>
    parse_video_sdp_cmax(std::string_view value) {
        value = trim(value);
        if (value.empty()) {
            return std::unexpected(Error::InvalidValue);
        }

        uint32_t val = 0;
        const char* first = value.data();
        const char* last = value.data() + value.size();
        const auto [ptr, ec] = std::from_chars(first, last, val);

        if (ec != std::errc{} || ptr != last) {
            return std::unexpected(Error::InvalidValue);
        }

        return val;
    }

    [[nodiscard]] inline std::expected<RawVideoSdpTimingAttributes, Error>
    parse_video_sdp_timing_attributes(const RawVideoSdpMediaSection& raw) {
        RawVideoSdpTimingAttributes res{};
        if (raw.ts_refclk) {
            auto expected_ts_refclck = parse_video_sdp_reference_clock(*raw.ts_refclk);
            if (expected_ts_refclck) {
                res.reference_clock = std::move(*expected_ts_refclck);
            } else {
                return std::unexpected(expected_ts_refclck.error());
            }
        }
        if (raw.mediaclk) {
            auto expected_mediaclk = parse_video_sdp_media_clock(*raw.mediaclk);
            if (expected_mediaclk) {
                res.media_clock = std::move(*expected_mediaclk);
            } else {
                return std::unexpected(expected_mediaclk.error());
            }
        }
        if (raw.tsmode) {
            auto expected_tsmode = parse_video_sdp_timestamp_mode(*raw.tsmode);
            if (expected_tsmode) {
                res.timestamp_mode = std::move(*expected_tsmode);
            } else {
                return std::unexpected(expected_tsmode.error());
            }
        }
        if (raw.tsdelay) {
            auto expected_tsdelay = parse_video_sdp_ts_delay(*raw.tsdelay);
            if (expected_tsdelay) {
                res.ts_delay_sender_ticks = std::move(*expected_tsdelay);
            } else {
                return std::unexpected(expected_tsdelay.error());
            }
        }
        if (raw.tp) {
            auto expected_tp = parse_video_sdp_sender_type(*raw.tp);
            if (expected_tp) {
                res.sender_type = std::move(*expected_tp);
            } else {
                return std::unexpected(expected_tp.error());
            }
        }
        if (raw.troff) {
            auto expected_troff = parse_video_sdp_troff(*raw.troff);
            if (expected_troff) {
                res.troff_us = std::move(*expected_troff);
            } else {
                return std::unexpected(expected_troff.error());
            }
        }
        if (raw.cmax) {
            auto expected_cmax = parse_video_sdp_cmax(*raw.cmax);
            if (expected_cmax) {
                res.cmax = std::move(*expected_cmax);
            } else {
                return std::unexpected(expected_cmax.error());
            }
        }

        return res;
    }
}

#endif //ST2110_OBS_PLUGIN_VIDEO_SDP_TIMING_ATTRIBUTES_HPP

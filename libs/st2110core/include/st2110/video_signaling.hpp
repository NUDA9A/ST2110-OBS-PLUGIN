#ifndef ST2110_OBS_PLUGIN_VIDEO_SIGNALING_HPP
#define ST2110_OBS_PLUGIN_VIDEO_SIGNALING_HPP

#include "pixel_format.hpp"
#include "video_scan_mode.hpp"
#include "error.hpp"
#include "config_validation.hpp"
#include "packet_parse.hpp"
#include "rx_config.hpp"

#include <cstdint>
#include <optional>
#include <array>
#include <string>
#include <expected>


namespace st2110 {
    enum class VideoPackingMode {
        Gpm, Bpm
    };

    enum class MediaClockMode {
        Direct, Sender
    };

    enum class TimestampMode {
        Samp, New, Pres
    };

    enum class ReferenceClockKind {
        LocalMac, Ptp, Other
    };

    struct PtpReferenceClock {
        // IEEE1588 clock identity from ts-refclk:ptp=...
        // representation format can stay implementation-chosen for now
        std::array<uint8_t, 8> clock_identity{};
        uint16_t domain_number = 0;
        bool traceable = false;
    };

    struct LocalMacReferenceClock {
        std::array<uint8_t, 6> mac{};
    };

    struct ReferenceClock {
        ReferenceClockKind kind = ReferenceClockKind::Ptp;

        std::optional<PtpReferenceClock> ptp{};
        std::optional<LocalMacReferenceClock> local_mac{};

        // preserve future extensibility / unknown RFC forms
        std::optional<std::string> raw_token{};
    };

    enum class VideoSenderType {
        Narrow, NarrowLinear, Wide
    };

    struct VideoStreamSignaling {
        PixelFormat format = PixelFormat::UYVY;
        VideoScanMode scan_mode = VideoScanMode::Progressive;

        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t fps_num = 0;
        uint32_t fps_den = 0;

        VideoPackingMode packing_mode = VideoPackingMode::Gpm;
        std::optional<std::size_t> max_udp_datagram_bytes{};

        MediaClockMode media_clock_mode = MediaClockMode::Direct;
        TimestampMode timestamp_mode = TimestampMode::New;
        ReferenceClock reference_clock{};

        uint32_t ts_delay_sender_ticks = 0;

        VideoSenderType sender_type = VideoSenderType::Narrow;
        std::optional<uint32_t> troff_us{};
        std::optional<uint32_t> cmax{};
    };

    inline Error validate_video_sender_signaling(
            VideoSenderType sender_type,
            const std::optional<uint32_t>& troff_us,
            const std::optional<uint32_t>& cmax) {
        switch (sender_type) {
            case VideoSenderType::Narrow:
                if (troff_us != std::nullopt || cmax != std::nullopt) {
                    return Error::InvalidValue;
                }
                return Error::Ok;
            case VideoSenderType::NarrowLinear:
                if (troff_us != std::nullopt || cmax != std::nullopt) {
                    return Error::InvalidValue;
                }
                return Error::Ok;
            case VideoSenderType::Wide:
                if (troff_us != std::nullopt || !cmax.has_value() || *cmax == 0) {
                    return Error::InvalidValue;
                }
                return Error::Ok;
            default:
                return Error::InvalidValue;
        }
    }

    inline Error validate_reference_clock(const ReferenceClock& clock) {
        switch (clock.kind) {
            case ReferenceClockKind::Ptp: {
                if (!clock.ptp.has_value() || clock.local_mac.has_value() || clock.raw_token.has_value()) {
                    return Error::InvalidValue;
                }
                return Error::Ok;
            }
            case ReferenceClockKind::LocalMac: {
                if (clock.ptp.has_value() || !clock.local_mac.has_value() || clock.raw_token.has_value()) {
                    return Error::InvalidValue;
                }
                return Error::Ok;
            }
            case ReferenceClockKind::Other: {
                if (clock.ptp.has_value() || clock.local_mac.has_value() || !clock.raw_token.has_value() ||
                    clock.raw_token->empty()) {
                    return Error::InvalidValue;
                }
                return Error::Ok;
            }
            default:
                return Error::InvalidValue;
        }
    }

    inline Error validate_media_clock_mode(MediaClockMode mode) {
        switch (mode) {
            case MediaClockMode::Direct:
            case MediaClockMode::Sender:
                return Error::Ok;
            default:
                return Error::InvalidValue;
        }
    }

    inline Error validate_timestamp_mode(TimestampMode mode) {
        switch (mode) {
            case TimestampMode::New:
            case TimestampMode::Pres:
            case TimestampMode::Samp:
                return Error::Ok;
            default:
                return Error::InvalidValue;
        }
    }

    inline Error validate_video_timing_signaling(
            MediaClockMode media_clock_mode,
            TimestampMode timestamp_mode,
            uint32_t ts_delay_sender_ticks) {
        if (Error err = validate_media_clock_mode(media_clock_mode); err != Error::Ok) {
            return err;
        }
        if (Error err = validate_timestamp_mode(timestamp_mode); err != Error::Ok) {
            return err;
        }
        (void)ts_delay_sender_ticks;
        return Error::Ok;
    }

    inline Error validate_video_stream_signaling(const VideoStreamSignaling& videoStreamSignaling) {
        if (Error err = validate_video_timing_signaling(videoStreamSignaling.media_clock_mode, videoStreamSignaling.timestamp_mode, videoStreamSignaling.ts_delay_sender_ticks); err != Error::Ok) {
            return err;
        }
        if (Error err = validate_video_sender_signaling(
                    videoStreamSignaling.sender_type,
                    videoStreamSignaling.troff_us,
                    videoStreamSignaling.cmax);
                err != Error::Ok) {
            return err;
        }
        if (Error err = validate_reference_clock(videoStreamSignaling.reference_clock); err != Error::Ok) {
            return err;
        }
        if (Error err = config_validation::validate_video_format_constraints(videoStreamSignaling.format, videoStreamSignaling.width, videoStreamSignaling.height); err != Error::Ok) {
            return err;
        }
        if (Error err = config_validation::validate_video_scan_mode(videoStreamSignaling.scan_mode); err != Error::Ok) {
            return err;
        }
        if (Error err = config_validation::validate_frame_rate(videoStreamSignaling.fps_num, videoStreamSignaling.fps_den); err != Error::Ok) {
            return err;
        }
        if (Error err = validate_packet_parse_policy_config(PacketParsePolicy{videoStreamSignaling.max_udp_datagram_bytes}); err != Error::Ok) {
            return err;
        }
        return Error::Ok;
    }

    inline PacketParsePolicy packet_parse_policy_from_video_stream_signaling(
            const VideoStreamSignaling& signaling) {
        return PacketParsePolicy{signaling.max_udp_datagram_bytes};
    }

    inline Error validate_video_stream_signaling_against_rx_video_config(
            const VideoStreamSignaling& signaling,
            const RxVideoConfig& cfg) {
        if (Error err = validate_video_stream_signaling(signaling); err != Error::Ok) {
            return err;
        }
        if (Error err = validate_rx_video_config(cfg); err != Error::Ok) {
            return err;
        }
        if (cfg.format != signaling.format) {
            return Error::InvalidValue;
        }
        if (cfg.scan_mode != signaling.scan_mode) {
            return Error::InvalidValue;
        }
        if (cfg.width != signaling.width) {
            return Error::InvalidValue;
        }
        if (cfg.height != signaling.height) {
            return Error::InvalidValue;
        }
        if (cfg.fps_num != signaling.fps_num) {
            return Error::InvalidValue;
        }
        if (cfg.fps_den != signaling.fps_den) {
            return Error::InvalidValue;
        }
        return Error::Ok;
    }

    inline std::expected<RxVideoConfig, Error> rx_video_config_from_video_stream_signaling(const VideoStreamSignaling& signaling,
                                                                                           uint16_t udp_port,
                                                                                           uint8_t payload_type,
                                                                                           std::string local_ip,
                                                                                           std::string dest_ip) {
        if (Error err = validate_video_stream_signaling(signaling); err != Error::Ok) {
            return std::unexpected(err);
        }
        RxVideoConfig res{
            .width = signaling.width,
            .height = signaling.height,
            .fps_num = signaling.fps_num,
            .fps_den = signaling.fps_den,
            .udp_port = udp_port,
            .payload_type = payload_type,
            .local_ip = std::move(local_ip),
            .dest_ip = std::move(dest_ip),
            .format = signaling.format,
            .scan_mode = signaling.scan_mode
        };
        if (Error err = validate_rx_video_config(res); err != Error::Ok) {
            return std::unexpected(err);
        }
        return res;
    }
}

#endif //ST2110_OBS_PLUGIN_VIDEO_SIGNALING_HPP

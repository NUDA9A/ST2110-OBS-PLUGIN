#ifndef ST2110_OBS_VIDEO_SIGNALING_TYPES_HPP
#define ST2110_OBS_VIDEO_SIGNALING_TYPES_HPP

#include <st2110/model/video/video_media_types.hpp>
#include <st2110/model/video/video_packing_mode.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <cstddef>

namespace st2110 {
enum class MediaClockMode { Direct, Sender };

enum class TimestampMode { Samp, New, Pres };

enum class ReferenceClockKind { LocalMac, Ptp, Other };

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

enum class VideoSenderType { Narrow, NarrowLinear, Wide };

struct VideoStreamSignaling {
    VideoMediaDescription media{};
    VideoScanMode scan_mode = VideoScanMode::Progressive;
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

inline Error validate_reference_clock(const ReferenceClock &clock) {
    switch (clock.kind) {
    case ReferenceClockKind::Ptp: {
        if (!clock.ptp.has_value() || clock.local_mac.has_value() || clock.raw_token.has_value()) {
            return Error::InvalidValue;
        }

        const auto &ptp = *clock.ptp;

        bool all_zero = true;
        for (uint8_t b : ptp.clock_identity) {
            if (b != 0) {
                all_zero = false;
                break;
            }
        }

        if (!ptp.traceable && all_zero) {
            return Error::InvalidValue;
        }

        return Error::Ok;
    }

    case ReferenceClockKind::LocalMac: {
        if (clock.ptp.has_value() || !clock.local_mac.has_value() || clock.raw_token.has_value()) {
            return Error::InvalidValue;
        }

        bool all_zero = true;
        for (uint8_t b : clock.local_mac->mac) {
            if (b != 0) {
                all_zero = false;
                break;
            }
        }

        if (all_zero) {
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

inline Error validate_video_sender_signaling(VideoSenderType sender_type, const std::optional<uint32_t> &troff_us,
                                             const std::optional<uint32_t> &cmax) {
    (void)sender_type;

    if (troff_us.has_value() && *troff_us == 0) {
        return Error::InvalidValue;
    }

    if (cmax.has_value() && *cmax == 0) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

inline Error validate_required_video_signal_standard(const std::optional<VideoSignalStandard> &ssn) {
    if (!ssn.has_value()) {
        return Error::InvalidValue;
    }

    return validate_video_signal_standard(*ssn);
}

}

#endif // ST2110_OBS_VIDEO_SIGNALING_TYPES_HPP

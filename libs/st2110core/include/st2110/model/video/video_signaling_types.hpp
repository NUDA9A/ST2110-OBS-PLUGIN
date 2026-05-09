#ifndef ST2110_OBS_VIDEO_SIGNALING_TYPES_HPP
#define ST2110_OBS_VIDEO_SIGNALING_TYPES_HPP

#include <st2110/model/video/video_media_types.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace st2110 {
struct VideoRtpClockSignaling {
    std::uint32_t rtp_clock_rate = 90000;
};

enum class MediaClockKind { Direct, Sender, Other };

struct DirectMediaClock {
    std::uint32_t rtp_clock_offset = 0;
};

struct MediaClockSignaling {
    MediaClockKind kind = MediaClockKind::Direct;
    std::optional<DirectMediaClock> direct{};
    std::optional<std::string> raw_token{};
};

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

enum class VideoStreamRedundancyKind {
    SingleStream,
    DuplicatePrimary,
    DuplicateRedundant,
};

struct VideoDuplicateStreamGroup {
    std::string primary_mid{};
    std::string redundant_mid{};
};

struct VideoStreamSignaling {
    VideoMediaDescription media{};
    VideoScanMode scan_mode = VideoScanMode::Progressive;
    VideoPackingMode packing_mode = VideoPackingMode::Gpm;
    VideoRtpClockSignaling rtp_clock{};
    std::optional<std::size_t> max_udp_datagram_bytes{};

    MediaClockSignaling media_clock{};
    TimestampMode timestamp_mode = TimestampMode::New;
    ReferenceClock reference_clock{};

    std::optional<std::uint32_t> ts_delay_us{};

    VideoSenderType sender_type = VideoSenderType::Narrow;
    std::optional<uint32_t> troff_us{};
    std::optional<uint32_t> cmax{};

    std::optional<std::string> mid{};
    VideoStreamRedundancyKind redundancy_kind = VideoStreamRedundancyKind::SingleStream;
    std::optional<VideoDuplicateStreamGroup> duplicate_group{};
};

inline Error validate_video_duplicate_stream_group(const VideoDuplicateStreamGroup &group) {
    if (group.primary_mid.empty() || group.redundant_mid.empty()) {
        return Error::InvalidValue;
    }

    if (group.primary_mid == group.redundant_mid) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

inline Error validate_media_clock_signaling(const MediaClockSignaling &clock) {
    switch (clock.kind) {
    case MediaClockKind::Direct:
        if (!clock.direct.has_value() || clock.raw_token.has_value()) {
            return Error::InvalidValue;
        }

        if (clock.direct->rtp_clock_offset != 0) {
            return Error::InvalidValue;
        }

        return Error::Ok;

    case MediaClockKind::Sender:
        if (clock.direct.has_value() || clock.raw_token.has_value()) {
            return Error::InvalidValue;
        }

        return Error::Ok;

    case MediaClockKind::Other:
        if (clock.direct.has_value() || !clock.raw_token.has_value() || clock.raw_token->empty()) {
            return Error::InvalidValue;
        }

        return Error::Ok;

    default:
        return Error::InvalidValue;
    }
}

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

        if (ptp.traceable) {
            if (!all_zero) {
                return Error::InvalidValue;
            }
        } else {
            if (all_zero) {
                return Error::InvalidValue;
            }
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

inline Error validate_video_max_udp_datagram_bytes(const std::optional<std::size_t> &max_udp_datagram_bytes,
                                                   VideoPackingMode packing_mode) {
    const std::size_t effective_max_udp = max_udp_datagram_bytes.value_or(1460);

    if (packing_mode == VideoPackingMode::Bpm) {
        return effective_max_udp <= 1460 ? Error::Ok : Error::InvalidValue;
    }

    return Error::Ok;
}

inline Error validate_video_stream_signaling(const VideoStreamSignaling &signaling) {
    if (const Error err =
            validate_video_media_description_cross_field_constraints(signaling.media, signaling.scan_mode);
        err != Error::Ok) {
        return err;
    }
    if (const Error err = validate_video_sender_signaling(signaling.sender_type, signaling.troff_us, signaling.cmax);
        err != Error::Ok) {
        return err;
    }
    if (const Error err = validate_reference_clock(signaling.reference_clock); err != Error::Ok) {
        return err;
    }
    if (const Error err = validate_media_clock_signaling(signaling.media_clock); err != Error::Ok) {
        return err;
    }
    if (const Error err =
            validate_video_max_udp_datagram_bytes(signaling.max_udp_datagram_bytes, signaling.packing_mode);
        err != Error::Ok) {
        return err;
    }

    if (signaling.rtp_clock.rtp_clock_rate != 90000) {
        return Error::InvalidValue;
    }

    if (signaling.mid.has_value() && signaling.mid->empty()) {
        return Error::InvalidValue;
    }

    switch (signaling.redundancy_kind) {
    case VideoStreamRedundancyKind::SingleStream:
        if (signaling.duplicate_group.has_value()) {
            return Error::InvalidValue;
        }
        break;

    case VideoStreamRedundancyKind::DuplicatePrimary:
    case VideoStreamRedundancyKind::DuplicateRedundant: {
        if (!signaling.mid.has_value() || !signaling.duplicate_group.has_value()) {
            return Error::InvalidValue;
        }
        if (const Error err = validate_video_duplicate_stream_group(*signaling.duplicate_group); err != Error::Ok) {
            return err;
        }

        const auto &mid = *signaling.mid;
        const auto &group = *signaling.duplicate_group;

        if (mid != group.primary_mid && mid != group.redundant_mid) {
            return Error::InvalidValue;
        }

        if (signaling.redundancy_kind == VideoStreamRedundancyKind::DuplicatePrimary && mid != group.primary_mid) {
            return Error::InvalidValue;
        }

        if (signaling.redundancy_kind == VideoStreamRedundancyKind::DuplicateRedundant && mid != group.redundant_mid) {
            return Error::InvalidValue;
        }

        break;
    }

    default:
        return Error::InvalidValue;
    }

    return Error::Ok;
}

} // namespace st2110

#endif // ST2110_OBS_VIDEO_SIGNALING_TYPES_HPP

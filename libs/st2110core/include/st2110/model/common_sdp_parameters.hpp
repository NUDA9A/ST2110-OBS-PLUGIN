#ifndef ST2110_OBS_COMMON_SDP_PARAMETERS_HPP
#define ST2110_OBS_COMMON_SDP_PARAMETERS_HPP

#include <st2110/foundation/error.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace st2110 {
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
    std::array<std::uint8_t, 8> clock_identity{};
    std::uint16_t domain_number = 0;
    bool traceable = false;
};

struct LocalMacReferenceClock {
    std::array<std::uint8_t, 6> mac{};
};

struct ReferenceClock {
    ReferenceClockKind kind = ReferenceClockKind::Ptp;

    std::optional<PtpReferenceClock> ptp{};
    std::optional<LocalMacReferenceClock> local_mac{};

    // preserve future extensibility / unknown RFC forms
    std::optional<std::string> raw_token{};
};

struct SourceFilterSignaling {
    enum class Scope { Session, Media };

    std::string raw_value{};
    Scope scope = Scope::Media;

    std::string filter_mode{};
    std::string network_type{};
    std::string address_type{};
    std::string destination_address{};
    std::vector<std::string> source_addresses{};
};

struct DuplicateStreamGroup {
    std::string first_mid{};
    std::string second_mid{};
};

struct StreamTimingSignaling {
    std::uint32_t rtp_clock_rate = 0;
    MediaClockSignaling media_clock{};
    TimestampMode timestamp_mode = TimestampMode::New;
    ReferenceClock reference_clock{};
    std::optional<std::uint32_t> ts_delay_us{};
};

struct StreamTransportSignaling {
    std::optional<std::size_t> max_udp_datagram_bytes{};
    std::vector<SourceFilterSignaling> source_filters{};
    std::optional<std::string> mid{};
    std::optional<DuplicateStreamGroup> duplicate_group{};
};

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
        for (const std::uint8_t b : ptp.clock_identity) {
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
        for (const std::uint8_t b : clock.local_mac->mac) {
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

[[nodiscard]] inline Error validate_duplicate_stream_group(const DuplicateStreamGroup &group) {
    if (group.first_mid.empty() || group.second_mid.empty()) {
        return Error::InvalidValue;
    }

    if (group.first_mid == group.second_mid) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

[[nodiscard]] inline Error validate_source_filter_signaling(const SourceFilterSignaling &filter) {
    if (filter.filter_mode.empty()) {
        return Error::InvalidValue;
    }

    if (filter.network_type.empty()) {
        return Error::InvalidValue;
    }

    if (filter.address_type.empty()) {
        return Error::InvalidValue;
    }

    if (filter.destination_address.empty()) {
        return Error::InvalidValue;
    }

    if (filter.source_addresses.empty()) {
        return Error::InvalidValue;
    }

    for (const std::string &source_address : filter.source_addresses) {
        if (source_address.empty()) {
            return Error::InvalidValue;
        }
    }

    return Error::Ok;
}

inline Error validate_stream_timing_signaling(const StreamTimingSignaling &timing_signaling) {
    if (timing_signaling.rtp_clock_rate == 0) {
        return Error::InvalidValue;
    }

    if (const Error err = validate_media_clock_signaling(timing_signaling.media_clock); err != Error::Ok) {
        return err;
    }

    if (const Error err = validate_reference_clock(timing_signaling.reference_clock); err != Error::Ok) {
        return err;
    }

    if (timing_signaling.timestamp_mode == TimestampMode::Samp && !timing_signaling.ts_delay_us.has_value()) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

inline Error validate_stream_transport_signaling(const StreamTransportSignaling &stream_transport_signaling) {
    for (const SourceFilterSignaling &source_filter : stream_transport_signaling.source_filters) {
        if (const Error err = validate_source_filter_signaling(source_filter); err != Error::Ok) {
            return err;
        }
    }

    if (stream_transport_signaling.mid.has_value() && stream_transport_signaling.mid->empty()) {
        return Error::InvalidValue;
    }

    if (!stream_transport_signaling.duplicate_group.has_value()) {
        return Error::Ok;
    }

    if (!stream_transport_signaling.mid.has_value()) {
        return Error::InvalidValue;
    }

    if (const Error err = validate_duplicate_stream_group(*stream_transport_signaling.duplicate_group);
        err != Error::Ok) {
        return err;
    }

    const auto &mid = *stream_transport_signaling.mid;
    const auto &group = *stream_transport_signaling.duplicate_group;

    if (mid != group.first_mid && mid != group.second_mid) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

} // namespace st2110

#endif // ST2110_OBS_COMMON_SDP_PARAMETERS_HPP

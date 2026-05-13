#ifndef ST2110_OBS_RTP_TIMESTAMP_MAPPER_CONFIG_HPP
#define ST2110_OBS_RTP_TIMESTAMP_MAPPER_CONFIG_HPP

#include <st2110/foundation/rtp_timestamp_anchor_policy.hpp>
#include <st2110/foundation/timestamp.hpp>

#include <cstdint>

namespace st2110 {
inline constexpr std::uint64_t rtpTimestampNanosecondsPerSecond = 1'000'000'000ULL;
inline constexpr std::uint32_t rtpTimestampAmbiguousForwardDelta = 0x80000000U;

struct RtpTimestampMapperConfig {
    std::uint32_t rtp_clock_rate = 0;
    RtpTimestampInitialAnchorMode initial_anchor_mode = RtpTimestampInitialAnchorMode::FirstObservedBecomesLocalZero;
    std::uint32_t anchor_rtp_timestamp = 0;
    TimestampNs anchor_timestamp_ns = 0;
};
} // namespace st2110

#endif // ST2110_OBS_RTP_TIMESTAMP_MAPPER_CONFIG_HPP

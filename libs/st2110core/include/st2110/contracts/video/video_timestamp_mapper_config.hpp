#ifndef ST2110_OBS_VIDEO_TIMESTAMP_MAPPER_CONFIG_HPP
#define ST2110_OBS_VIDEO_TIMESTAMP_MAPPER_CONFIG_HPP

#include <st2110/foundation/rtp_timestamp_anchor_policy.hpp>

namespace st2110 {
inline constexpr std::uint64_t videoTimestampNanosecondsPerSecond = 1000000000ULL;

struct VideoRtpTimestampMapperConfig {
    std::uint32_t rtp_clock_rate = 90000;
    RtpTimestampInitialAnchorMode initial_anchor_mode = RtpTimestampInitialAnchorMode::FirstObservedBecomesLocalZero;
    std::uint32_t anchor_rtp_timestamp = 0;
    TimestampNs anchor_timestamp_ns = 0;
};

[[nodiscard]] inline VideoRtpTimestampMapperConfig make_video_rtp_timestamp_mapper_config() {
    return VideoRtpTimestampMapperConfig{};
}
} // namespace st2110

#endif // ST2110_OBS_VIDEO_TIMESTAMP_MAPPER_CONFIG_HPP

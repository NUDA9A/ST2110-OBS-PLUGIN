#ifndef ST2110_OBS_PLUGIN_RTP_TIMESTAMP_MAPPER_HPP
#define ST2110_OBS_PLUGIN_RTP_TIMESTAMP_MAPPER_HPP

#include <st2110/contracts/rtp_timestamp_mapper_config.hpp>
#include <st2110/foundation/error.hpp>
#include <st2110/foundation/rtp_timestamp_anchor_policy.hpp>
#include <st2110/foundation/timestamp.hpp>

#include <cstdint>
#include <expected>
#include <limits>

namespace st2110 {
[[nodiscard]] inline std::expected<std::uint64_t, Error> checked_rtp_timestamp_add_u64(const std::uint64_t a,
                                                                                       const std::uint64_t b) {
    if (a > std::numeric_limits<std::uint64_t>::max() - b) {
        return std::unexpected(Error::InvalidValue);
    }

    return a + b;
}

[[nodiscard]] inline std::expected<std::uint64_t, Error> checked_rtp_timestamp_mul_u64(const std::uint64_t a,
                                                                                       const std::uint64_t b) {
    if (a != 0 && b > std::numeric_limits<std::uint64_t>::max() / a) {
        return std::unexpected(Error::InvalidValue);
    }

    return a * b;
}

[[nodiscard]] inline std::expected<std::uint32_t, Error> forward_rtp_timestamp_delta(const std::uint32_t previous,
                                                                                     const std::uint32_t current) {
    const std::uint32_t raw_delta = current - previous;

    if (raw_delta >= rtpTimestampAmbiguousForwardDelta) {
        return std::unexpected(Error::InvalidValue);
    }

    return raw_delta;
}

[[nodiscard]] inline std::expected<TimestampNs, Error>
rtp_ticks_to_timestamp_ns(const std::uint64_t ticks, const std::uint32_t rtp_clock_rate,
                          const TimestampNs anchor_timestamp_ns) {
    if (rtp_clock_rate == 0) {
        return std::unexpected(Error::InvalidValue);
    }

    const std::uint64_t seconds = ticks / rtp_clock_rate;
    const std::uint64_t remainder_ticks = ticks % rtp_clock_rate;

    auto seconds_ns = checked_rtp_timestamp_mul_u64(seconds, rtpTimestampNanosecondsPerSecond);
    if (!seconds_ns.has_value()) {
        return std::unexpected(seconds_ns.error());
    }

    const std::uint64_t remainder_ns =
        (static_cast<std::uint64_t>(remainder_ticks) * rtpTimestampNanosecondsPerSecond) / rtp_clock_rate;

    auto relative_ns = checked_rtp_timestamp_add_u64(*seconds_ns, remainder_ns);
    if (!relative_ns.has_value()) {
        return std::unexpected(relative_ns.error());
    }

    auto absolute_ns = checked_rtp_timestamp_add_u64(anchor_timestamp_ns, *relative_ns);
    if (!absolute_ns.has_value()) {
        return std::unexpected(absolute_ns.error());
    }

    return *absolute_ns;
}

class RtpTimestampMapper {
  public:
    explicit RtpTimestampMapper(const RtpTimestampMapperConfig &cfg) : cfg_(cfg) {}

    [[nodiscard]] std::expected<TimestampNs, Error> map(const std::uint32_t rtp_timestamp) {
        if (cfg_.rtp_clock_rate == 0) {
            return std::unexpected(Error::InvalidValue);
        }

        if (cfg_.initial_anchor_mode == RtpTimestampInitialAnchorMode::FirstObservedBecomesLocalZero &&
            !has_runtime_origin_) {
            has_runtime_origin_ = true;
            last_raw_rtp_timestamp_ = rtp_timestamp;
            ticks_since_anchor_ = 0;
            return 0;
        }

        const std::uint32_t previous_rtp_timestamp =
            has_runtime_origin_ ? last_raw_rtp_timestamp_ : cfg_.anchor_rtp_timestamp;

        auto delta = forward_rtp_timestamp_delta(previous_rtp_timestamp, rtp_timestamp);
        if (!delta.has_value()) {
            return std::unexpected(delta.error());
        }

        if (ticks_since_anchor_ > std::numeric_limits<std::uint64_t>::max() - static_cast<std::uint64_t>(*delta)) {
            return std::unexpected(Error::InvalidValue);
        }

        ticks_since_anchor_ += static_cast<std::uint64_t>(*delta);
        last_raw_rtp_timestamp_ = rtp_timestamp;
        has_runtime_origin_ = true;

        const TimestampNs effective_anchor_timestamp_ns =
            cfg_.initial_anchor_mode == RtpTimestampInitialAnchorMode::ConfiguredReference ? cfg_.anchor_timestamp_ns
                                                                                           : 0;

        return rtp_ticks_to_timestamp_ns(ticks_since_anchor_, cfg_.rtp_clock_rate, effective_anchor_timestamp_ns);
    }

    void reset(const RtpTimestampMapperConfig &cfg) noexcept {
        cfg_ = cfg;
        has_runtime_origin_ = false;
        last_raw_rtp_timestamp_ = 0;
        ticks_since_anchor_ = 0;
    }

  private:
    RtpTimestampMapperConfig cfg_{};
    bool has_runtime_origin_ = false;
    std::uint32_t last_raw_rtp_timestamp_ = 0;
    std::uint64_t ticks_since_anchor_ = 0;
};

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_RTP_TIMESTAMP_MAPPER_HPP
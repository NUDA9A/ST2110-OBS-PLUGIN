#ifndef ST2110_OBS_PLUGIN_VIDEO_TIMESTAMP_MAPPING_HPP
#define ST2110_OBS_PLUGIN_VIDEO_TIMESTAMP_MAPPING_HPP

#include "timestamp.hpp"
#include "error.hpp"

#include <cstdint>
#include <expected>
#include <limits>

namespace st2110 {
inline constexpr uint64_t videoTimestampNanosecondsPerSecond = 1000000000ULL;

struct VideoRtpTimestampMapperConfig {
  uint32_t rtp_clock_rate = 90000;
  uint32_t anchor_rtp_timestamp = 0;
  TimestampNs anchor_timestamp_ns = 0;
};

[[nodiscard]] inline Error validate_video_rtp_timestamp_mapper_config(const VideoRtpTimestampMapperConfig &cfg) {
  if (cfg.rtp_clock_rate == 0) {
    return Error::InvalidValue;
  }

  return Error::Ok;
}

[[nodiscard]] inline std::expected<uint64_t, Error> checked_video_timestamp_add_u64(uint64_t a, uint64_t b) {
  if (a > std::numeric_limits<uint64_t>::max() - b) {
    return std::unexpected(Error::InvalidValue);
  }
  return a + b;
}

[[nodiscard]] inline std::expected<uint64_t, Error> checked_video_timestamp_mul_u64(uint64_t a, uint64_t b) {
  if (a != 0 && b > std::numeric_limits<uint64_t>::max() / a) {
    return std::unexpected(Error::InvalidValue);
  }

  return a * b;
}

[[nodiscard]] inline std::expected<uint32_t, Error> forward_rtp_timestamp_delta(uint32_t previous, uint32_t current) {
  const uint32_t raw_delta = current - previous;

  // RTP timestamp is 32-bit modulo arithmetic.
  //
  // Deltas below half the 32-bit range are interpreted as forward movement,
  // including normal wraparound, e.g. 0xFFFFFFFE -> 0.
  //
  // Deltas at or above half range are ambiguous / backward for this simple
  // monotonic mapper and are rejected.
  if (raw_delta >= 0x80000000U) {
    return std::unexpected(Error::InvalidValue);
  }

  return raw_delta;
}

[[nodiscard]] inline std::expected<TimestampNs, Error>
rtp_ticks_to_timestamp_ns(uint64_t ticks, uint32_t rtp_clock_rate, TimestampNs anchor_timestamp_ns) {
  if (rtp_clock_rate == 0) {
    return std::unexpected(Error::InvalidValue);
  }

  const uint64_t seconds = ticks / rtp_clock_rate;
  const uint64_t remainder_ticks = ticks % rtp_clock_rate;

  auto seconds_ns = checked_video_timestamp_mul_u64(seconds, videoTimestampNanosecondsPerSecond);

  if (!seconds_ns.has_value()) {
    return std::unexpected(seconds_ns.error());
  }

  // remainder_ticks < rtp_clock_rate <= uint32 max, so this multiplication
  // fits in uint64_t.
  const uint64_t remainder_ns = (remainder_ticks * videoTimestampNanosecondsPerSecond) / rtp_clock_rate;

  auto relative_ns = checked_video_timestamp_add_u64(*seconds_ns, remainder_ns);

  if (!relative_ns.has_value()) {
    return std::unexpected(relative_ns.error());
  }

  auto absolute_ns = checked_video_timestamp_add_u64(anchor_timestamp_ns, *relative_ns);

  if (!absolute_ns.has_value()) {
    return std::unexpected(absolute_ns.error());
  }

  return *absolute_ns;
}

class VideoRtpTimestampMapper {
public:
  explicit VideoRtpTimestampMapper(VideoRtpTimestampMapperConfig cfg) { reset(cfg); }

  [[nodiscard]] std::expected<TimestampNs, Error> map(uint32_t rtp_timestamp) {
    if (config_error_ != Error::Ok) {
      return std::unexpected(config_error_);
    }

    auto delta = forward_rtp_timestamp_delta(last_raw_rtp_timestamp_, rtp_timestamp);

    if (!delta.has_value()) {
      return std::unexpected(delta.error());
    }

    if (ticks_since_anchor_ > std::numeric_limits<uint64_t>::max() - static_cast<uint64_t>(*delta)) {
      return std::unexpected(Error::InvalidValue);
    }

    ticks_since_anchor_ += static_cast<uint64_t>(*delta);
    last_raw_rtp_timestamp_ = rtp_timestamp;

    return rtp_ticks_to_timestamp_ns(ticks_since_anchor_, cfg_.rtp_clock_rate, cfg_.anchor_timestamp_ns);
  }

  void reset(VideoRtpTimestampMapperConfig cfg) {
    cfg_ = cfg;
    config_error_ = validate_video_rtp_timestamp_mapper_config(cfg_);

    last_raw_rtp_timestamp_ = cfg_.anchor_rtp_timestamp;
    ticks_since_anchor_ = 0;
  }

private:
  VideoRtpTimestampMapperConfig cfg_{};
  Error config_error_ = Error::Ok;

  uint32_t last_raw_rtp_timestamp_ = 0;
  uint64_t ticks_since_anchor_ = 0;
};

struct SyntheticVideoTimestampMapperConfig {
  uint32_t fps_num = 0;
  uint32_t fps_den = 1;
  TimestampNs anchor_timestamp_ns = 0;
};

[[nodiscard]] inline Error
validate_synthetic_video_timestamp_mapper_config(const SyntheticVideoTimestampMapperConfig &cfg) {
  if (cfg.fps_num == 0 || cfg.fps_den == 0) {
    return Error::InvalidValue;
  }

  return Error::Ok;
}

[[nodiscard]] inline std::expected<TimestampNs, Error>
synthetic_unit_index_to_timestamp_ns(uint64_t unit_index, const SyntheticVideoTimestampMapperConfig &cfg) {
  if (Error err = validate_synthetic_video_timestamp_mapper_config(cfg); err != Error::Ok) {
    return std::unexpected(err);
  }

#if defined(__SIZEOF_INT128__)
  const __uint128_t numerator = static_cast<__uint128_t>(unit_index) * static_cast<__uint128_t>(cfg.fps_den) *
                                static_cast<__uint128_t>(videoTimestampNanosecondsPerSecond);

  const __uint128_t relative_ns = numerator / static_cast<__uint128_t>(cfg.fps_num);

  const __uint128_t absolute_ns = relative_ns + static_cast<__uint128_t>(cfg.anchor_timestamp_ns);

  if (absolute_ns > static_cast<__uint128_t>(std::numeric_limits<TimestampNs>::max())) {
    return std::unexpected(Error::InvalidValue);
  }

  return static_cast<TimestampNs>(absolute_ns);
#else
  const uint64_t whole_groups = unit_index / cfg.fps_num;
  const uint64_t remainder_units = unit_index % cfg.fps_num;

  auto whole_seconds = checked_video_timestamp_mul_u64(whole_groups, cfg.fps_den);

  if (!whole_seconds.has_value()) {
    return std::unexpected(whole_seconds.error());
  }

  auto whole_ns = checked_video_timestamp_mul_u64(*whole_seconds, videoTimestampNanosecondsPerSecond);

  if (!whole_ns.has_value()) {
    return std::unexpected(whole_ns.error());
  }

  auto fractional_num = checked_video_timestamp_mul_u64(remainder_units, cfg.fps_den);

  if (!fractional_num.has_value()) {
    return std::unexpected(fractional_num.error());
  }

  fractional_num = checked_video_timestamp_mul_u64(*fractional_num, videoTimestampNanosecondsPerSecond);

  if (!fractional_num.has_value()) {
    return std::unexpected(fractional_num.error());
  }

  const uint64_t fractional_ns = *fractional_num / cfg.fps_num;

  auto relative_ns = checked_video_timestamp_add_u64(*whole_ns, fractional_ns);

  if (!relative_ns.has_value()) {
    return std::unexpected(relative_ns.error());
  }

  auto absolute_ns = checked_video_timestamp_add_u64(cfg.anchor_timestamp_ns, *relative_ns);

  if (!absolute_ns.has_value()) {
    return std::unexpected(absolute_ns.error());
  }

  return *absolute_ns;
#endif
}

class SyntheticVideoTimestampMapper {
public:
  explicit SyntheticVideoTimestampMapper(SyntheticVideoTimestampMapperConfig cfg)
      : cfg_(cfg), config_error_(validate_synthetic_video_timestamp_mapper_config(cfg_)) {}

  [[nodiscard]] std::expected<TimestampNs, Error> map_unit_index(uint64_t unit_index) const {
    if (config_error_ != Error::Ok) {
      return std::unexpected(config_error_);
    }

    return synthetic_unit_index_to_timestamp_ns(unit_index, cfg_);
  }

private:
  SyntheticVideoTimestampMapperConfig cfg_{};
  Error config_error_ = Error::Ok;
};

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_VIDEO_TIMESTAMP_MAPPING_HPP
#ifndef ST2110_OBS_PLUGIN_AUDIO_TIMESTAMP_MAPPING_HPP
#define ST2110_OBS_PLUGIN_AUDIO_TIMESTAMP_MAPPING_HPP

#include "error.hpp"
#include "timestamp.hpp"

#include <cstdint>
#include <expected>
#include <limits>

namespace st2110 {
constexpr uint64_t audioTimestampNanosecondsPerSecond = 1'000'000'000ull;
constexpr uint64_t audioRtpTimestampAmbiguousDelta = 0x80000000ull;

struct AudioRtpTimestampMapperConfig {
  uint32_t rtp_clock_rate = 0;
  uint32_t anchor_rtp_timestamp = 0;
  TimestampNs anchor_timestamp_ns = 0;
};

[[nodiscard]] inline Error validate_audio_rtp_timestamp_mapper_config(const AudioRtpTimestampMapperConfig &cfg) {
  if (cfg.rtp_clock_rate == 0) {
    return Error::InvalidValue;
  }

  return Error::Ok;
}

[[nodiscard]] inline std::expected<uint64_t, Error> forward_audio_rtp_timestamp_delta(uint32_t previous,
                                                                                      uint32_t current) {
  const uint32_t delta32 = current - previous;
  const uint64_t delta = static_cast<uint64_t>(delta32);

  if (delta >= audioRtpTimestampAmbiguousDelta) {
    return std::unexpected(Error::InvalidValue);
  }

  return delta;
}

[[nodiscard]] inline std::expected<TimestampNs, Error> audio_rtp_ticks_to_timestamp_ns(uint64_t ticks,
                                                                                       uint32_t rtp_clock_rate) {
  if (rtp_clock_rate == 0) {
    return std::unexpected(Error::InvalidValue);
  }

  const uint64_t whole_seconds = ticks / rtp_clock_rate;
  const uint64_t remainder_ticks = ticks % rtp_clock_rate;

  if (whole_seconds >

      std::numeric_limits<TimestampNs>::max()

          / audioTimestampNanosecondsPerSecond) {
    return std::unexpected(Error::InvalidValue);
  }

  const uint64_t whole_ns = whole_seconds * audioTimestampNanosecondsPerSecond;

  const uint64_t remainder_ns = (remainder_ticks * audioTimestampNanosecondsPerSecond) / rtp_clock_rate;

  if (whole_ns >

      std::numeric_limits<TimestampNs>::max()

          - remainder_ns) {
    return std::unexpected(Error::InvalidValue);
  }

  return whole_ns + remainder_ns;
}

class AudioRtpTimestampMapper {
public:
  explicit AudioRtpTimestampMapper(AudioRtpTimestampMapperConfig cfg) : cfg_(cfg) {}

  [[nodiscard]] std::expected<TimestampNs, Error> map(uint32_t rtp_timestamp) {
    const Error cfg_error = validate_audio_rtp_timestamp_mapper_config(cfg_);
    if (cfg_error != Error::Ok) {
      return std::unexpected(cfg_error);
    }

    uint64_t delta_ticks = 0;
    if (!has_last_timestamp_) {
      const auto delta = forward_audio_rtp_timestamp_delta(cfg_.anchor_rtp_timestamp, rtp_timestamp);
      if (!delta.has_value()) {
        return std::unexpected(delta.error());
      }

      accumulated_ticks_since_anchor_ = *delta;
    } else {
      const auto delta = forward_audio_rtp_timestamp_delta(last_rtp_timestamp_, rtp_timestamp);
      if (!delta.has_value()) {
        return std::unexpected(delta.error());
      }

      delta_ticks = *delta;
      if (accumulated_ticks_since_anchor_ > std::numeric_limits<uint64_t>::max() - delta_ticks) {
        return std::unexpected(Error::InvalidValue);
      }

      accumulated_ticks_since_anchor_ += delta_ticks;
    }

    const auto offset_ns = audio_rtp_ticks_to_timestamp_ns(accumulated_ticks_since_anchor_, cfg_.rtp_clock_rate);
    if (!offset_ns.has_value()) {
      return std::unexpected(offset_ns.error());
    }

    if (cfg_.anchor_timestamp_ns > std::numeric_limits<TimestampNs>::max() - *offset_ns) {
      return std::unexpected(Error::InvalidValue);
    }

    const TimestampNs mapped_timestamp = cfg_.anchor_timestamp_ns + *offset_ns;

    has_last_timestamp_ = true;
    last_rtp_timestamp_ = rtp_timestamp;
    last_timestamp_ns_ = mapped_timestamp;

    return mapped_timestamp;
  }

  [[nodiscard]] Error reset(AudioRtpTimestampMapperConfig cfg) {
    const Error cfg_error = validate_audio_rtp_timestamp_mapper_config(cfg);
    if (cfg_error != Error::Ok) {
      return cfg_error;
    }

    cfg_ = cfg;
    has_last_timestamp_ = false;
    last_rtp_timestamp_ = 0;
    last_timestamp_ns_ = 0;
    accumulated_ticks_since_anchor_ = 0;

    return Error::Ok;
  }

private:
  AudioRtpTimestampMapperConfig cfg_{};
  bool has_last_timestamp_ = false;
  uint32_t last_rtp_timestamp_ = 0;
  TimestampNs last_timestamp_ns_ = 0;
  uint64_t accumulated_ticks_since_anchor_ = 0;
};

struct AudioReceiverPlayoutTimingConfig {
  TimestampNs playout_delay_ns = 0;
};

struct AudioReceiverPlayoutTimingDecision {
  TimestampNs media_timestamp_ns = 0;
  TimestampNs playout_timestamp_ns = 0;
};

[[nodiscard]] inline Error validate_audio_receiver_playout_timing_config(const AudioReceiverPlayoutTimingConfig &cfg) {
  (void)cfg;
  return Error::Ok;
}

[[nodiscard]] inline std::expected<AudioReceiverPlayoutTimingDecision, Error>
audio_receiver_playout_timing_decision(TimestampNs media_timestamp_ns, const AudioReceiverPlayoutTimingConfig &cfg) {
  const Error cfg_error = validate_audio_receiver_playout_timing_config(cfg);
  if (cfg_error != Error::Ok) {
    return std::unexpected(cfg_error);
  }

  if (media_timestamp_ns > std::numeric_limits<TimestampNs>::max() - cfg.playout_delay_ns) {
    return std::unexpected(Error::InvalidValue);
  }

  return AudioReceiverPlayoutTimingDecision{.media_timestamp_ns = media_timestamp_ns,
                                            .playout_timestamp_ns = media_timestamp_ns + cfg.playout_delay_ns};
}

struct AudioBlockTiming {
  uint32_t rtp_timestamp = 0;
  TimestampNs media_timestamp_ns = 0;
  TimestampNs playout_timestamp_ns = 0;
};

[[nodiscard]] inline std::expected<AudioBlockTiming, Error>
audio_block_timing(uint32_t rtp_timestamp, TimestampNs media_timestamp_ns,
                   const AudioReceiverPlayoutTimingConfig &cfg) {
  const auto decision = audio_receiver_playout_timing_decision(media_timestamp_ns, cfg);
  if (!decision.has_value()) {
    return std::unexpected(decision.error());
  }

  return AudioBlockTiming{.rtp_timestamp = rtp_timestamp,
                          .media_timestamp_ns = decision->media_timestamp_ns,
                          .playout_timestamp_ns = decision->playout_timestamp_ns};
}

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_AUDIO_TIMESTAMP_MAPPING_HPP
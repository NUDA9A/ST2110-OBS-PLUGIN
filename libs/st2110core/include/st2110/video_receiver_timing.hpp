#ifndef ST2110_OBS_PLUGIN_VIDEO_RECEIVER_TIMING_HPP
#define ST2110_OBS_PLUGIN_VIDEO_RECEIVER_TIMING_HPP

#include "error.hpp"

namespace st2110 {
struct VideoReceiverTimingCapability {
  bool supports_type_n = true;
  bool supports_type_nl = true;
  bool supports_type_w = true;
};

struct VideoReceiverTimingRequirements {
  bool require_reference_clock = true;
  bool require_media_clock = true;
  bool require_timestamp_mode = true;
  bool consume_ts_delay = true;
  bool consume_sender_troff = true;
  bool consume_sender_cmax = true;
};

struct VideoReceiverTimingConfig {
  VideoReceiverTimingCapability capability{};
  VideoReceiverTimingRequirements requirements{};
};

[[nodiscard]] inline bool has_any_supported_video_sender_type(const VideoReceiverTimingCapability &capability) {
  return capability.supports_type_n || capability.supports_type_nl || capability.supports_type_w;
}

[[nodiscard]] inline Error validate_video_receiver_timing_capability(const VideoReceiverTimingCapability &capability) {
  if (!has_any_supported_video_sender_type(capability)) {
    return Error::InvalidValue;
  }
  return Error::Ok;
}

[[nodiscard]] inline Error validate_video_receiver_timing_config(const VideoReceiverTimingConfig &cfg) {
  if (Error err = validate_video_receiver_timing_capability(cfg.capability); err != Error::Ok) {
    return err;
  }
  return Error::Ok;
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_VIDEO_RECEIVER_TIMING_HPP

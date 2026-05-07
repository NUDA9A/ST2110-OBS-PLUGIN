#ifndef ST2110_OBS_PLUGIN_VIDEO_PLAYOUT_TIMING_HPP
#define ST2110_OBS_PLUGIN_VIDEO_PLAYOUT_TIMING_HPP

#include "st2110/foundation/error.hpp"
#include "st2110/foundation/timestamp.hpp"
#include "video_unit_reconstructor.hpp"

#include <cstdint>
#include <expected>
#include <limits>

namespace st2110 {
struct VideoReceiverPlayoutTimingConfig {
    TimestampNs link_offset_delay_ns = 0;
};

struct VideoReceiverPlayoutTimingDecision {
    TimestampNs media_timestamp_ns = 0;
    TimestampNs reconstruction_timestamp_ns = 0;
};

struct VideoReconstructedFrameTiming {
    uint32_t rtp_timestamp = 0;
    TimestampNs media_timestamp_ns = 0;
    TimestampNs reconstruction_timestamp_ns = 0;
};

[[nodiscard]] inline Error validate_video_receiver_playout_timing_config(const VideoReceiverPlayoutTimingConfig &cfg) {
    (void)cfg;
    return Error::Ok;
}

[[nodiscard]] inline std::expected<VideoReceiverPlayoutTimingDecision, Error>
video_receiver_playout_timing_decision(TimestampNs media_timestamp_ns, const VideoReceiverPlayoutTimingConfig &cfg) {
    if (Error err = validate_video_receiver_playout_timing_config(cfg); err != Error::Ok) {
        return std::unexpected(err);
    }

    if (media_timestamp_ns > std::numeric_limits<TimestampNs>::max() - cfg.link_offset_delay_ns) {
        return std::unexpected(Error::InvalidValue);
    }

    return VideoReceiverPlayoutTimingDecision{media_timestamp_ns, media_timestamp_ns + cfg.link_offset_delay_ns};
}

[[nodiscard]] inline std::expected<VideoReconstructedFrameTiming, Error>
video_reconstructed_frame_timing(const ReconstructedVideoFrame &frame, TimestampNs media_timestamp_ns,
                                 const VideoReceiverPlayoutTimingConfig &cfg) {
    if (Error err = validate_video_receiver_playout_timing_config(cfg); err != Error::Ok) {
        return std::unexpected(err);
    }

    auto expected_timing_decision = video_receiver_playout_timing_decision(media_timestamp_ns, cfg);
    if (!expected_timing_decision) {
        return std::unexpected(expected_timing_decision.error());
    }
    auto timing_decision = *expected_timing_decision;

    return VideoReconstructedFrameTiming{frame.rtp_timestamp, timing_decision.media_timestamp_ns,
                                         timing_decision.reconstruction_timestamp_ns};
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_VIDEO_PLAYOUT_TIMING_HPP

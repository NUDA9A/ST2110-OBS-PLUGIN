#ifndef ST2110_OBS_PLUGIN_VIDEO_RECEIVER_TIMING_SIGNALING_HPP
#define ST2110_OBS_PLUGIN_VIDEO_RECEIVER_TIMING_SIGNALING_HPP

#include "video_signaling.hpp"
#include "video_receiver_timing.hpp"
#include "error.hpp"

namespace st2110 {
    [[nodiscard]] inline bool video_receiver_supports_sender_type(
            const VideoReceiverTimingCapability& capability,
            VideoSenderType sender_type) {
        switch (sender_type) {
            case VideoSenderType::Narrow:
                return capability.supports_type_n;
            case VideoSenderType::NarrowLinear:
                return capability.supports_type_nl;
            case VideoSenderType::Wide:
                return capability.supports_type_w;
            default:
                return false;
        }
    }

    [[nodiscard]] inline Error validate_video_receiver_timing_against_video_stream_signaling(
            const VideoReceiverTimingConfig& cfg,
            const VideoStreamSignaling& signaling) {
        if (Error err = validate_video_receiver_timing_config(cfg); err != Error::Ok) {
            return err;
        }
        if (Error err = validate_video_sender_signaling(signaling.sender_type, signaling.troff_us, signaling.cmax); err != Error::Ok) {
            return err;
        }

        if (cfg.requirements.require_reference_clock) {
            if (Error err = validate_reference_clock(signaling.reference_clock); err != Error::Ok) {
                return err;
            }
        }
        if (cfg.requirements.require_media_clock) {
            if (Error err = validate_media_clock_mode(signaling.media_clock_mode); err != Error::Ok) {
                return err;
            }
        }
        if (cfg.requirements.require_timestamp_mode) {
            if (Error err = validate_timestamp_mode(signaling.timestamp_mode); err != Error::Ok) {
                return err;
            }
        }
        if (!video_receiver_supports_sender_type(cfg.capability, signaling.sender_type)) {
            return Error::Unsupported;
        }
        if (!cfg.requirements.consume_ts_delay && signaling.ts_delay_sender_ticks != 0) {
            return Error::Unsupported;
        }
        if (!cfg.requirements.consume_sender_troff && signaling.troff_us.has_value()) {
            return Error::Unsupported;
        }
        if (!cfg.requirements.consume_sender_cmax && signaling.cmax.has_value()) {
            return Error::Unsupported;
        }
        return Error::Ok;
    }
}

#endif //ST2110_OBS_PLUGIN_VIDEO_RECEIVER_TIMING_SIGNALING_HPP

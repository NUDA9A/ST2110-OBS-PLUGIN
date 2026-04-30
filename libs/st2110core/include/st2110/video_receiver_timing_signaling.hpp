#ifndef ST2110_OBS_PLUGIN_VIDEO_RECEIVER_TIMING_SIGNALING_HPP
#define ST2110_OBS_PLUGIN_VIDEO_RECEIVER_TIMING_SIGNALING_HPP

#include <expected>

#include "video_signaling.hpp"
#include "video_receiver_timing.hpp"
#include "packet_parse.hpp"
#include "rx_config.hpp"
#include "error.hpp"
#include "video_receive_pipeline.hpp"
#include "depacketizer.hpp"
#include "video_unit_reconstructor.hpp"
#include "signaling_structs.hpp"

namespace st2110 {
[[nodiscard]] inline bool video_receiver_supports_sender_type(const VideoReceiverTimingCapability &capability,
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

[[nodiscard]] inline Error
validate_video_receiver_timing_against_video_stream_signaling(const VideoReceiverTimingConfig &cfg,
                                                              const VideoStreamSignaling &signaling) {
    if (Error err = validate_video_receiver_timing_config(cfg); err != Error::Ok) {
        return err;
    }
    if (Error err = validate_video_sender_signaling(signaling.sender_type, signaling.troff_us, signaling.cmax);
        err != Error::Ok) {
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

[[nodiscard]] inline std::expected<VideoReceiverBootstrapConfig, Error>
video_receiver_bootstrap_config_from_video_stream_signaling(const VideoStreamSignaling &signaling,
                                                            const VideoReceiverTimingConfig &timing_cfg,
                                                            uint16_t udp_port, uint8_t payload_type,
                                                            std::string local_ip, std::string dest_ip,
                                                            PartialFramePolicy partial_frame_policy) {
    if (Error err = validate_video_receiver_timing_against_video_stream_signaling(timing_cfg, signaling);
        err != Error::Ok) {
        return std::unexpected(err);
    }

    auto expected_base = video_receiver_bootstrap_config_from_video_stream_signaling(
        signaling, udp_port, payload_type, std::move(local_ip), std::move(dest_ip), partial_frame_policy);

    if (!expected_base.has_value()) {
        return std::unexpected(expected_base.error());
    }

    auto base = *expected_base;

    base.timing_config = timing_cfg;

    return base;
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_VIDEO_RECEIVER_TIMING_SIGNALING_HPP

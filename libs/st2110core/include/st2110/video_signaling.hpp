#ifndef ST2110_OBS_PLUGIN_VIDEO_SIGNALING_HPP
#define ST2110_OBS_PLUGIN_VIDEO_SIGNALING_HPP

#include "delivery/video/pixel_format.hpp"
#include "foundation/error.hpp"
#include "ingress/shared/packet_parse.hpp"
#include "receive/video/depacketizer.hpp"
#include "receive/video/video_unit_reconstructor.hpp"
#include "rx_config.hpp"
#include "st2110/receive/video/video_receive_pipeline.hpp"
#include "video_receive_capability.hpp"
#include <st2110/model/video/video_signaling_types.hpp>
#include <st2110/receive/video/video_receive_description.hpp>

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <utility>

#include "signaling_structs.hpp"

namespace st2110 {
struct VideoReceiveCapabilityProjectionOptions {
    VideoFrameHandoffFormat handoff_format = VideoFrameHandoffFormat::Uyvy;
    VideoReceiveRtpClock rtp_clock{};
    VideoReceiveTopology topology{};
};

/*
 * Current project-delivery projection from signaling into project storage format.
 *
 * This is not a structural signaling-validity boundary.
 * Structurally recognized but not currently project-deliverable media must return
 * Error::Unsupported here rather than being treated as invalid signaling.
 *
 * Backend/runtime implementation support must not be checked here.
 */
inline std::expected<PixelFormat, Error>
pixel_format_from_video_stream_signaling(const VideoStreamSignaling &signaling) {
    if (Error err = validate_video_sampling(signaling.media.sampling); err != Error::Ok) {
        return std::unexpected(err);
    }
    if (Error err = validate_video_bit_depth(signaling.media.depth); err != Error::Ok) {
        return std::unexpected(err);
    }
    switch (signaling.media.sampling.known) {
    case VideoSampling::Known::YCbCr422:
        if (!signaling.media.depth.floating_point && signaling.media.depth.bits == 8) {
            return PixelFormat::UYVY;
        }
        return std::unexpected(Error::Unsupported);

    case VideoSampling::Known::YCbCr444:
    case VideoSampling::Known::YCbCr420:
    case VideoSampling::Known::RGB:
    case VideoSampling::Known::XYZ:
    case VideoSampling::Known::Key:
    case VideoSampling::Known::CLYCbCr444:
    case VideoSampling::Known::CLYCbCr422:
    case VideoSampling::Known::CLYCbCr420:
    case VideoSampling::Known::ICtCp444:
    case VideoSampling::Known::ICtCp422:
    case VideoSampling::Known::ICtCp420:
    case VideoSampling::Known::Other:
    default:
        return std::unexpected(Error::Unsupported);
    }
}

inline Error validate_video_media_description(const VideoMediaDescription &media) {
    if (Error err = validate_video_media_description_structure(media); err != Error::Ok) {
        return err;
    }

    return validate_required_video_signal_standard(media.signal_standard);
}

[[nodiscard]] inline std::expected<VideoReceiveCapability, Error>
video_receive_capability_from_video_stream_signaling(const VideoStreamSignaling &signaling,
                                                     const VideoReceiveCapabilityProjectionOptions &options = {}) {
    if (Error err = validate_video_stream_signaling(signaling); err != Error::Ok) {
        return std::unexpected(err);
    }

    if (Error err = validate_video_receive_rtp_clock(options.rtp_clock); err != Error::Ok) {
        return std::unexpected(err);
    }

    if (Error err = validate_video_receive_topology(options.topology); err != Error::Ok) {
        return std::unexpected(err);
    }

    auto transport_format = video_transport_payload_format_from_media_description(signaling.media);
    if (!transport_format.has_value()) {
        return std::unexpected(transport_format.error());
    }

    VideoReceiveCapability capability{};
    capability.media = signaling.media;
    capability.scan_mode = signaling.scan_mode;
    capability.packing_mode = signaling.packing_mode;
    capability.transport_format = *transport_format;
    capability.handoff_format = options.handoff_format;
    capability.rtp_clock = options.rtp_clock;
    capability.topology = options.topology;

    if (Error err = validate_video_receive_capability_structure(capability); err != Error::Ok) {
        return std::unexpected(err);
    }

    return capability;
}

[[nodiscard]] inline Error validate_video_stream_signaling_against_project_delivery_support(
    const VideoStreamSignaling &signaling, const VideoReceiveCapabilityProjectionOptions &options = {},
    const VideoProjectDeliverySupportPolicy &support = default_video_project_delivery_support_policy()) {
    if (Error err = validate_video_stream_signaling(signaling); err != Error::Ok) {
        return err;
    }

    auto capability = video_receive_capability_from_video_stream_signaling(signaling, options);
    if (!capability.has_value()) {
        return capability.error();
    }

    auto projected_format = project_pixel_format_from_video_frame_handoff_format(capability->handoff_format);
    if (!projected_format.has_value()) {
        return projected_format.error();
    }

    if (support.require_project_handoff_format_support) {
        if (Error err = validate_project_video_frame_handoff_format_matches_pixel_format(capability->handoff_format,
                                                                                         *projected_format);
            err != Error::Ok) {
            return err;
        }
    }

    if (support.require_project_pixel_format_storage_compatibility) {
        if (Error err = validate_project_video_frame_storage_compatibility(*capability, *projected_format);
            err != Error::Ok) {
            return err;
        }
    }

    return Error::Ok;
}

[[nodiscard]] inline Error
validate_video_receive_capability_against_video_stream_signaling(const VideoReceiveCapability &capability,
                                                                 const VideoStreamSignaling &signaling) {
    if (Error err = validate_video_stream_signaling(signaling); err != Error::Ok) {
        return err;
    }

    if (Error err = validate_video_receive_capability_structure(capability); err != Error::Ok) {
        return err;
    }

    if (capability.scan_mode != signaling.scan_mode) {
        return Error::InvalidValue;
    }

    if (capability.packing_mode != signaling.packing_mode) {
        return Error::InvalidValue;
    }

    if (Error err = validate_video_transport_payload_format_matches_media_description(capability.transport_format,
                                                                                      signaling.media);
        err != Error::Ok) {
        return err;
    }

    return Error::Ok;
}

inline PacketParsePolicy packet_parse_policy_from_video_stream_signaling(const VideoStreamSignaling &signaling) {
    return PacketParsePolicy{signaling.max_udp_datagram_bytes};
}

inline Error validate_video_stream_signaling_against_rx_video_config(const VideoStreamSignaling &signaling,
                                                                     const RxVideoConfig &cfg) {
    if (Error err = validate_video_stream_signaling(signaling); err != Error::Ok) {
        return err;
    }

    if (Error err = validate_rx_video_config(cfg); err != Error::Ok) {
        return err;
    }

    auto capability = rx_video_config_effective_receive_capability(cfg);
    if (!capability.has_value()) {
        return capability.error();
    }

    if (Error err = validate_video_receive_capability_against_video_stream_signaling(*capability, signaling);
        err != Error::Ok) {
        return err;
    }

    return Error::Ok;
}

/*
 * Project runtime-config projection from signaling into DepacketizerConfig.
 *
 * This helper must:
 * - validate signaling structurally;
 * - apply current project-delivery projection;
 * - project the recognized packing mode into DepacketizerConfig unchanged.
 *
 * This helper must not reject a structurally recognized packing mode only because
 * current runtime/backend implementation support is narrower.
 * Runtime/backend support is checked later at the relevant support boundary.
 */
[[nodiscard]] inline std::expected<DepacketizerConfig, Error>
depacketizer_config_from_video_stream_signaling(const VideoStreamSignaling &signaling, PartialFramePolicy policy) {
    if (Error err = validate_video_stream_signaling_against_project_delivery_support(signaling); err != Error::Ok) {
        return std::unexpected(err);
    }

    auto expected_format = pixel_format_from_video_stream_signaling(signaling);
    if (!expected_format.has_value()) {
        return std::unexpected(std::move(expected_format.error()));
    }

    PixelFormat format = *expected_format;
    return DepacketizerConfig{.width = signaling.media.width,
                              .height = signaling.media.height,
                              .format = format,
                              .partial_frame_policy = policy,
                              .scan_mode = signaling.scan_mode,
                              .packing_mode = signaling.packing_mode};
}

inline std::expected<VideoUnitReconstructorConfig, Error>
video_unit_reconstructor_config_from_video_stream_signaling(const VideoStreamSignaling &signaling) {
    if (Error err = validate_video_stream_signaling(signaling); err != Error::Ok) {
        return std::unexpected(err);
    }
    auto expected_format = pixel_format_from_video_stream_signaling(signaling);
    if (!expected_format.has_value()) {
        return std::unexpected(std::move(expected_format.error()));
    }
    PixelFormat format = *expected_format;
    return VideoUnitReconstructorConfig{.format = format, .scan_mode = signaling.scan_mode};
}

/*
 * Project runtime-config projection from signaling into VideoReceivePipelineConfig.
 *
 * This helper must:
 * - validate signaling structurally;
 * - apply current project-delivery projection;
 * - preserve the structurally recognized packing mode in the projected pipeline config.
 *
 * It must not use current runtime/backend implementation limits as an early
 * rejection boundary for otherwise recognized signaling.
 */
[[nodiscard]] inline std::expected<VideoReceivePipelineConfig, Error>
video_receive_pipeline_config_from_video_stream_signaling(const VideoStreamSignaling &signaling,
                                                          PartialFramePolicy policy) {
    if (Error err = validate_video_stream_signaling_against_project_delivery_support(signaling); err != Error::Ok) {
        return std::unexpected(err);
    }

    auto expected_format = pixel_format_from_video_stream_signaling(signaling);
    if (!expected_format.has_value()) {
        return std::unexpected(std::move(expected_format.error()));
    }

    PixelFormat format = *expected_format;
    return VideoReceivePipelineConfig{
        .depacketizer = DepacketizerConfig{.width = signaling.media.width,
                                           .height = signaling.media.height,
                                           .format = format,
                                           .partial_frame_policy = policy,
                                           .scan_mode = signaling.scan_mode,
                                           .packing_mode = signaling.packing_mode},
        .reconstructor = VideoUnitReconstructorConfig{.format = format, .scan_mode = signaling.scan_mode}};
}

inline std::expected<RxVideoConfig, Error>
rx_video_config_from_video_stream_signaling(const VideoStreamSignaling &signaling, uint16_t udp_port,
                                            uint8_t payload_type, std::string local_ip, std::string dest_ip,
                                            const VideoReceiveCapabilityProjectionOptions &options = {}) {
    auto capability = video_receive_capability_from_video_stream_signaling(signaling, options);
    if (!capability.has_value()) {
        return std::unexpected(capability.error());
    }

    auto projected_format = project_pixel_format_from_video_frame_handoff_format(capability->handoff_format);
    if (!projected_format.has_value()) {
        return std::unexpected(projected_format.error());
    }

    RxVideoConfig res{.width = capability->media.width,
                      .height = capability->media.height,
                      .fps_num = capability->media.fps_num,
                      .fps_den = capability->media.fps_den,
                      .udp_port = udp_port,
                      .payload_type = payload_type,
                      .local_ip = std::move(local_ip),
                      .dest_ip = std::move(dest_ip),
                      .format = *projected_format,
                      .scan_mode = capability->scan_mode,
                      .packing_mode = capability->packing_mode,
                      .receive_capability = *capability};

    if (Error err = validate_rx_video_config_against_project_delivery_support(
            res, default_video_project_delivery_support_policy());
        err != Error::Ok) {
        return std::unexpected(err);
    }

    return res;
}

[[nodiscard]] inline std::expected<VideoRtpTimestampMapperConfig, Error>
video_rtp_timestamp_mapper_config_from_video_stream_signaling(const VideoStreamSignaling &signaling) {
    if (Error err = validate_video_stream_signaling(signaling); err != Error::Ok) {
        return std::unexpected(err);
    }

    VideoRtpTimestampMapperConfig cfg = video_rtp_timestamp_mapper_config_first_observed_local_zero();

    if (Error err = validate_video_rtp_timestamp_mapper_config(cfg); err != Error::Ok) {
        return std::unexpected(err);
    }

    return cfg;
}

/*
 * Signaling-derived bootstrap projection for the current project runtime model.
 *
 * This helper must remain above backend-specific support validation:
 * - structural signaling validity is checked here;
 * - current project-delivery projection is checked here;
 * - backend/runtime implementation support is not decided here.
 *
 * A structurally recognized mode must not be rejected here solely because the
 * current backend/runtime path does not yet implement that mode.
 */
[[nodiscard]] inline std::expected<VideoReceiverBootstrapConfig, Error>
video_receiver_bootstrap_config_from_video_stream_signaling(const VideoStreamSignaling &signaling, uint16_t udp_port,
                                                            uint8_t payload_type, std::string local_ip,
                                                            std::string dest_ip,
                                                            PartialFramePolicy partial_frame_policy) {
    if (Error err = validate_video_stream_signaling_against_project_delivery_support(signaling); err != Error::Ok) {
        return std::unexpected(err);
    }

    auto policy = packet_parse_policy_from_video_stream_signaling(signaling);

    auto expected_rx_config = rx_video_config_from_video_stream_signaling(signaling, udp_port, payload_type,
                                                                          std::move(local_ip), std::move(dest_ip));
    if (!expected_rx_config.has_value()) {
        return std::unexpected(std::move(expected_rx_config.error()));
    }
    auto rx_config = *expected_rx_config;

    auto expected_receive_pipeline =
        video_receive_pipeline_config_from_video_stream_signaling(signaling, partial_frame_policy);
    if (!expected_receive_pipeline.has_value()) {
        return std::unexpected(std::move(expected_receive_pipeline.error()));
    }
    auto receive_pipeline = *expected_receive_pipeline;

    auto expected_timestamp_mapper_config = video_rtp_timestamp_mapper_config_from_video_stream_signaling(signaling);
    if (!expected_timestamp_mapper_config.has_value()) {
        return std::unexpected(std::move(expected_timestamp_mapper_config.error()));
    }

    return VideoReceiverBootstrapConfig{
        .packet_parse_policy = policy,
        .rx_config = rx_config,
        .receive_pipeline_config = receive_pipeline,
        .timestamp_mapper_config = *expected_timestamp_mapper_config,
        .timing_config = VideoReceiverTimingConfig{},
    };
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_VIDEO_SIGNALING_HPP
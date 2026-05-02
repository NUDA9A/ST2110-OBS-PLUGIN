#ifndef ST2110_OBS_PLUGIN_VIDEO_SIGNALING_HPP
#define ST2110_OBS_PLUGIN_VIDEO_SIGNALING_HPP

#include "config_validation.hpp"
#include "depacketizer.hpp"
#include "error.hpp"
#include "packet_parse.hpp"
#include "pixel_format.hpp"
#include "rx_config.hpp"
#include "video_receive_pipeline.hpp"
#include "video_scan_mode.hpp"
#include "video_unit_reconstructor.hpp"

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>

#include "signaling_structs.hpp"

namespace st2110 {
inline Error validate_video_sender_signaling(VideoSenderType sender_type, const std::optional<uint32_t> &troff_us,
                                             const std::optional<uint32_t> &cmax) {
    switch (sender_type) {
    case VideoSenderType::Narrow:
        if (troff_us != std::nullopt || cmax != std::nullopt) {
            return Error::InvalidValue;
        }
        return Error::Ok;

    case VideoSenderType::NarrowLinear:
        if (troff_us != std::nullopt || cmax != std::nullopt) {
            return Error::InvalidValue;
        }
        return Error::Ok;

    case VideoSenderType::Wide:
        // Receiver-side structural SDP ingestion must not require
        // optional sender/conformance parameters merely because the
        // sender type is Wide. A stricter sender-profile/conformance
        // policy can be layered separately later.
        if (cmax.has_value() && *cmax == 0) {
            return Error::InvalidValue;
        }
        return Error::Ok;

    default:
        return Error::InvalidValue;
    }
}

inline Error validate_reference_clock(const ReferenceClock &clock) {
    switch (clock.kind) {
    case ReferenceClockKind::Ptp: {
        if (!clock.ptp.has_value() || clock.local_mac.has_value() || clock.raw_token.has_value()) {
            return Error::InvalidValue;
        }
        return Error::Ok;
    }
    case ReferenceClockKind::LocalMac: {
        if (clock.ptp.has_value() || !clock.local_mac.has_value() || clock.raw_token.has_value()) {
            return Error::InvalidValue;
        }
        return Error::Ok;
    }
    case ReferenceClockKind::Other: {
        if (clock.ptp.has_value() || clock.local_mac.has_value() || !clock.raw_token.has_value() ||
            clock.raw_token->empty()) {
            return Error::InvalidValue;
        }
        return Error::Ok;
    }
    default:
        return Error::InvalidValue;
    }
}

inline Error validate_media_clock_mode(MediaClockMode mode) {
    switch (mode) {
    case MediaClockMode::Direct:
    case MediaClockMode::Sender:
        return Error::Ok;
    default:
        return Error::InvalidValue;
    }
}

inline Error validate_timestamp_mode(TimestampMode mode) {
    switch (mode) {
    case TimestampMode::New:
    case TimestampMode::Pres:
    case TimestampMode::Samp:
        return Error::Ok;
    default:
        return Error::InvalidValue;
    }
}

inline Error validate_video_timing_signaling(MediaClockMode media_clock_mode, TimestampMode timestamp_mode,
                                             uint32_t ts_delay_sender_ticks) {
    if (Error err = validate_media_clock_mode(media_clock_mode); err != Error::Ok) {
        return err;
    }
    if (Error err = validate_timestamp_mode(timestamp_mode); err != Error::Ok) {
        return err;
    }
    (void)ts_delay_sender_ticks;
    return Error::Ok;
}

inline Error validate_video_sampling(const VideoSampling &sampling) {
    if (sampling.known == VideoSampling::Known::Other) {
        if (!sampling.raw_token.has_value() || sampling.raw_token->empty()) {
            return Error::InvalidValue;
        }
    } else {
        if (sampling.raw_token.has_value()) {
            return Error::InvalidValue;
        }
    }
    return Error::Ok;
}

inline Error validate_video_colorimetry(const VideoColorimetry &colorimetry) {
    if (colorimetry.known == VideoColorimetry::Known::Other) {
        if (!colorimetry.raw_token.has_value() || colorimetry.raw_token->empty()) {
            return Error::InvalidValue;
        }
    } else {
        if (colorimetry.raw_token.has_value()) {
            return Error::InvalidValue;
        }
    }
    return Error::Ok;
}

inline Error validate_video_transfer_characteristic_system(const VideoTransferCharacteristicSystem &tcs) {
    if (tcs.known == VideoTransferCharacteristicSystem::Known::Other) {
        if (!tcs.raw_token.has_value() || tcs.raw_token->empty()) {
            return Error::InvalidValue;
        }
    } else {
        if (tcs.raw_token.has_value()) {
            return Error::InvalidValue;
        }
    }
    return Error::Ok;
}

inline Error validate_video_signal_standard(const VideoSignalStandard &ssn) {
    if (ssn.known == VideoSignalStandard::Known::Other) {
        if (!ssn.raw_token.has_value() || ssn.raw_token->empty()) {
            return Error::InvalidValue;
        }
    } else {
        if (ssn.raw_token.has_value()) {
            return Error::InvalidValue;
        }
    }
    return Error::Ok;
}

inline Error validate_video_range(const VideoRange &range) {
    if (range.known == VideoRange::Known::Other) {
        if (!range.raw_token.has_value() || range.raw_token->empty()) {
            return Error::InvalidValue;
        }
    } else {
        if (range.raw_token.has_value()) {
            return Error::InvalidValue;
        }
    }
    return Error::Ok;
}

inline Error validate_video_pixel_aspect_ratio(const VideoPixelAspectRatio &par) {
    if (par.width == 0 || par.height == 0) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

[[nodiscard]] inline Error validate_video_media_description_dimensions(uint32_t width, uint32_t height) {
    constexpr uint32_t max_signaled_video_dimension = 32767;

    if (width == 0 || height == 0) {
        return Error::InvalidValue;
    }

    if (width > max_signaled_video_dimension || height > max_signaled_video_dimension) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

inline Error validate_video_bit_depth(const VideoBitDepth &depth) {
    if (depth.floating_point) {
        if (depth.bits != 16) {
            return Error::InvalidValue;
        }
    } else {
        if (depth.bits != 8 && depth.bits != 10 && depth.bits != 12 && depth.bits != 16) {
            return Error::InvalidValue;
        }
    }
    return Error::Ok;
}

inline bool is_420_video_sampling(const VideoSampling &sampling) {
    switch (sampling.known) {
    case VideoSampling::Known::YCbCr420:
    case VideoSampling::Known::CLYCbCr420:
    case VideoSampling::Known::ICtCp420:
        return true;

    default:
        return false;
    }
}

inline Error validate_video_media_description_cross_field_constraints(const VideoMediaDescription &media,
                                                                      VideoScanMode scan_mode) {
    // ST 2110-20 4:2:0 sampling variants are progressive-only at the
    // signaling/model layer. Runtime support may still reject them later
    // through PixelFormat projection boundaries.
    if (is_420_video_sampling(media.sampling) && scan_mode != VideoScanMode::Progressive) {
        return Error::InvalidValue;
    }

    // KEY is an alpha/key signal, not a normal image signal.
    // Keep this as signaling-model validation instead of runtime projection.
    if (media.sampling.known == VideoSampling::Known::Key) {
        if (media.colorimetry.known != VideoColorimetry::Known::Alpha || media.colorimetry.raw_token.has_value()) {
            return Error::InvalidValue;
        }

        if (media.transfer_characteristic_system.has_value()) {
            return Error::InvalidValue;
        }
    }

    const bool uses_alpha_colorimetry = (media.colorimetry.known == VideoColorimetry::Known::Alpha);
    const bool uses_st2115logs3_tcs =
        media.transfer_characteristic_system.has_value() &&
        media.transfer_characteristic_system->known == VideoTransferCharacteristicSystem::Known::St2115LogS3;

    const bool requires_st2110_20_2022 = uses_alpha_colorimetry || uses_st2115logs3_tcs;

    if (media.signal_standard.has_value()) {
        switch (media.signal_standard->known) {
        case VideoSignalStandard::Known::Other:
            break;

        case VideoSignalStandard::Known::St2110_20_2017:
            if (requires_st2110_20_2022) {
                return Error::InvalidValue;
            }
            break;

        case VideoSignalStandard::Known::St2110_20_2022:
            if (!requires_st2110_20_2022) {
                return Error::InvalidValue;
            }
            break;

        default:
            return Error::InvalidValue;
        }
    }

    if (!media.range.has_value() || media.range->known == VideoRange::Known::Other) {
        return Error::Ok;
    }

    if (media.colorimetry.known == VideoColorimetry::Known::Bt2100) {
        switch (media.range->known) {
        case VideoRange::Known::Narrow:
        case VideoRange::Known::Full:
            return Error::Ok;

        case VideoRange::Known::FullProtect:
        default:
            return Error::InvalidValue;
        }
    }

    switch (media.range->known) {
    case VideoRange::Known::Narrow:
    case VideoRange::Known::FullProtect:
    case VideoRange::Known::Full:
        return Error::Ok;

    default:
        return Error::InvalidValue;
    }
}

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
    if (Error err = validate_video_sampling(media.sampling); err != Error::Ok) {
        return err;
    }
    if (Error err = validate_video_bit_depth(media.depth); err != Error::Ok) {
        return err;
    }
    if (Error err = validate_video_colorimetry(media.colorimetry); err != Error::Ok) {
        return err;
    }
    if (media.transfer_characteristic_system.has_value()) {
        if (Error err = validate_video_transfer_characteristic_system(*media.transfer_characteristic_system);
            err != Error::Ok) {
            return err;
            }
    }
    if (media.signal_standard.has_value()) {
        if (Error err = validate_video_signal_standard(*media.signal_standard); err != Error::Ok) {
            return err;
        }
    }
    if (media.range.has_value()) {
        if (Error err = validate_video_range(*media.range); err != Error::Ok) {
            return err;
        }
    }
    if (Error err = validate_video_pixel_aspect_ratio(media.pixel_aspect_ratio); err != Error::Ok) {
        return err;
    }
    if (Error err = validate_video_media_description_dimensions(media.width, media.height); err != Error::Ok) {
        return err;
    }
    if (Error err = config_validation::validate_frame_rate(media.fps_num, media.fps_den); err != Error::Ok) {
        return err;
    }
    return Error::Ok;
}

inline Error validate_video_stream_signaling(const VideoStreamSignaling &signaling) {
    if (Error err = validate_video_media_description(signaling.media); err != Error::Ok) {
        return err;
    }
    if (Error err = config_validation::validate_video_scan_mode(signaling.scan_mode); err != Error::Ok) {
        return err;
    }
    if (Error err = validate_video_media_description_cross_field_constraints(signaling.media, signaling.scan_mode);
        err != Error::Ok) {
        return err;
    }
    if (Error err = validate_video_timing_signaling(signaling.media_clock_mode, signaling.timestamp_mode,
                                                    signaling.ts_delay_sender_ticks);
        err != Error::Ok) {
        return err;
    }
    if (Error err = validate_video_sender_signaling(signaling.sender_type, signaling.troff_us, signaling.cmax);
        err != Error::Ok) {
        return err;
    }
    if (Error err = validate_reference_clock(signaling.reference_clock); err != Error::Ok) {
        return err;
    }
    if (Error err = validate_packet_parse_policy_config(PacketParsePolicy{signaling.max_udp_datagram_bytes});
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

    auto projected_format = pixel_format_from_video_stream_signaling(signaling);
    if (!projected_format.has_value()) {
        return projected_format.error();
    }

    if (cfg.format != *projected_format) {
        return Error::InvalidValue;
    }
    if (cfg.scan_mode != signaling.scan_mode) {
        return Error::InvalidValue;
    }
    if (cfg.width != signaling.media.width) {
        return Error::InvalidValue;
    }
    if (cfg.height != signaling.media.height) {
        return Error::InvalidValue;
    }
    if (cfg.fps_num != signaling.media.fps_num) {
        return Error::InvalidValue;
    }
    if (cfg.fps_den != signaling.media.fps_den) {
        return Error::InvalidValue;
    }
    if (cfg.packing_mode != signaling.packing_mode) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

inline std::expected<DepacketizerConfig, Error>
depacketizer_config_from_video_stream_signaling(const VideoStreamSignaling &signaling, PartialFramePolicy policy) {
    if (Error err = validate_video_stream_signaling(signaling); err != Error::Ok) {
        return std::unexpected(err);
    }
    if (Error err = validate_runtime_video_packing_mode_support(signaling.packing_mode); err != Error::Ok) {
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

inline std::expected<VideoReceivePipelineConfig, Error>
video_receive_pipeline_config_from_video_stream_signaling(const VideoStreamSignaling &signaling,
                                                          PartialFramePolicy policy) {
    if (Error err = validate_video_stream_signaling(signaling); err != Error::Ok) {
        return std::unexpected(err);
    }
    if (Error err = validate_runtime_video_packing_mode_support(signaling.packing_mode); err != Error::Ok) {
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
                                            uint8_t payload_type, std::string local_ip, std::string dest_ip) {
    if (Error err = validate_video_stream_signaling(signaling); err != Error::Ok) {
        return std::unexpected(err);
    }

    auto projected_format = pixel_format_from_video_stream_signaling(signaling);
    if (!projected_format.has_value()) {
        return std::unexpected(projected_format.error());
    }

    RxVideoConfig res{.width = signaling.media.width,
                      .height = signaling.media.height,
                      .fps_num = signaling.media.fps_num,
                      .fps_den = signaling.media.fps_den,
                      .udp_port = udp_port,
                      .payload_type = payload_type,
                      .local_ip = std::move(local_ip),
                      .dest_ip = std::move(dest_ip),
                      .format = *projected_format,
                      .scan_mode = signaling.scan_mode,
                      .packing_mode = signaling.packing_mode};

    if (Error err = validate_rx_video_config(res); err != Error::Ok) {
        return std::unexpected(err);
    }
    return res;
}

inline std::expected<VideoReceiverBootstrapConfig, Error> video_receiver_bootstrap_config_from_video_stream_signaling(
    const VideoStreamSignaling &signaling, uint16_t udp_port, uint8_t payload_type, std::string local_ip,
    std::string dest_ip, PartialFramePolicy partial_frame_policy) {
    if (Error err = validate_video_stream_signaling(signaling); err != Error::Ok) {
        return std::unexpected(err);
    }
    if (Error err = validate_runtime_video_packing_mode_support(signaling.packing_mode); err != Error::Ok) {
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

    return VideoReceiverBootstrapConfig{
        .packet_parse_policy = policy, .rx_config = rx_config, .receive_pipeline_config = receive_pipeline};
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_VIDEO_SIGNALING_HPP

#ifndef ST2110_OBS_PLUGIN_RX_CONFIG_HPP
#define ST2110_OBS_PLUGIN_RX_CONFIG_HPP

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>

#include "delivery/video/pixel_format.hpp"
#include "model/audio/audio_signaling.hpp"
#include "model/video/video_packing_mode.hpp"
#include "model/video/video_scan_mode.hpp"
#include "video_receive_capability.hpp"
#include "st2110/foundation/derived_values.hpp"

namespace st2110 {
struct RxVideoConfig;

/*
 * Structural/common validation entry point.
 * This must remain equivalent to validate_rx_video_config_structure(...)
 * and must not apply backend-specific or project-delivery support limits.
 */
[[nodiscard]] Error validate_rx_video_config(const RxVideoConfig &cfg);

struct RxVideoConfig {
    uint32_t width;
    uint32_t height;
    uint32_t fps_num;
    uint32_t fps_den;
    uint16_t udp_port;
    uint8_t payload_type;
    std::string local_ip;
    std::string dest_ip;

    /*
     * Project storage format axis.
     * This is the currently implemented VideoFrame/VideoFrameView storage format.
     * Common ST 2110 media and backend projection state live in receive_capability.
     */
    PixelFormat format;

    VideoScanMode scan_mode = VideoScanMode::Progressive;
    VideoPackingMode packing_mode = VideoPackingMode::Gpm;

    /*
     * Optional common receive-capability model.
     * When present, this is the source of truth for common media/mode axes.
     *
     * Validation boundaries:
     * - validate_rx_video_config_structure(...)
     *     common structural validity only;
     * - validate_rx_video_config_against_project_delivery_support(...)
     *     current project handoff/storage support only;
     * - backend-specific support helpers
     *     selected-backend implementation support only.
     */
    std::optional<VideoReceiveCapability> receive_capability{};

    [[nodiscard]] bool is_valid() const { return (validate_rx_video_config(*this) == Error::Ok); }
};

struct VideoProjectDeliverySupportPolicy {
    bool require_project_pixel_format_storage_compatibility = true;
    bool require_project_handoff_format_support = true;
};

/*
 * Compatibility-layer aggregate for callers that still want a combined
 * project-delivery + runtime-support check in one helper.
 *
 * This is not the primary common validation boundary.
 * New backend-specific code should prefer:
 * - validate_rx_video_config(...)
 * - validate_rx_video_config_against_project_delivery_support(...)
 * - backend-local support helpers
 */
struct VideoRuntimeSupportPolicy {
    VideoProjectDeliverySupportPolicy project_delivery{};
    bool require_runtime_packing_mode_support = true;
};

[[nodiscard]] inline VideoProjectDeliverySupportPolicy default_video_project_delivery_support_policy() {
    return VideoProjectDeliverySupportPolicy{};
}

[[nodiscard]] inline VideoRuntimeSupportPolicy default_video_rx_runtime_support_policy() {
    return VideoRuntimeSupportPolicy{
        .project_delivery = default_video_project_delivery_support_policy(),
        .require_runtime_packing_mode_support = true,
    };
}

[[nodiscard]] inline Error validate_rx_video_config_common_transport_fields(const RxVideoConfig &cfg) {
    if (cfg.fps_num == 0 || cfg.fps_den == 0 || cfg.udp_port == 0 || cfg.width == 0 || cfg.height == 0) {
        return Error::InvalidValue;
    }

    if (cfg.payload_type < 96 || cfg.payload_type > 127) {
        return Error::InvalidValue;
    }

    if (cfg.dest_ip.empty()) {
        return Error::InvalidValue;
    }

    if (Error err = validate_video_packing_mode(cfg.packing_mode); err != Error::Ok) {
        return err;
    }

    return Error::Ok;
}

[[nodiscard]] inline std::expected<VideoReceiveCapability, Error>
video_receive_capability_from_rx_video_config_project_fields(const RxVideoConfig &cfg) {
    auto handoff_format = video_frame_handoff_format_from_project_pixel_format(cfg.format);
    if (!handoff_format.has_value()) {
        return std::unexpected(handoff_format.error());
    }

    VideoMediaDescription media{};
    media.width = cfg.width;
    media.height = cfg.height;
    media.fps_num = cfg.fps_num;
    media.fps_den = cfg.fps_den;

    switch (cfg.format) {
    case PixelFormat::UYVY:
        media.sampling.known = VideoSampling::Known::YCbCr422;
        media.depth.bits = 8;
        media.depth.floating_point = false;
        media.colorimetry.known = VideoColorimetry::Known::Bt709;
        media.signal_standard = VideoSignalStandard{.known = VideoSignalStandard::Known::St2110_20_2017};
        media.range = VideoRange{.known = VideoRange::Known::Narrow};
        media.pixel_aspect_ratio = VideoPixelAspectRatio{.width = 1, .height = 1};
        break;

    default:
        return std::unexpected(Error::InvalidValue);
    }

    auto transport_format = video_transport_payload_format_from_media_description(media);
    if (!transport_format.has_value()) {
        return std::unexpected(transport_format.error());
    }

    VideoReceiveCapability capability{};
    capability.media = media;
    capability.scan_mode = cfg.scan_mode;
    capability.packing_mode = cfg.packing_mode;
    capability.transport_format = *transport_format;
    capability.handoff_format = *handoff_format;
    capability.rtp_clock = VideoReceiveRtpClock{};
    capability.topology = VideoReceiveTopology{};

    if (Error err = validate_video_receive_capability_structure(capability); err != Error::Ok) {
        return std::unexpected(err);
    }

    return capability;
}

[[nodiscard]] inline std::expected<VideoReceiveCapability, Error>
rx_video_config_effective_receive_capability(const RxVideoConfig &cfg) {
    if (cfg.receive_capability.has_value()) {
        return *cfg.receive_capability;
    }

    return video_receive_capability_from_rx_video_config_project_fields(cfg);
}

[[nodiscard]] inline Error
validate_rx_video_config_matches_explicit_receive_capability(const RxVideoConfig &cfg,
                                                             const VideoReceiveCapability &capability) {
    if (cfg.width != capability.media.width) {
        return Error::InvalidValue;
    }

    if (cfg.height != capability.media.height) {
        return Error::InvalidValue;
    }

    if (cfg.fps_num != capability.media.fps_num) {
        return Error::InvalidValue;
    }

    if (cfg.fps_den != capability.media.fps_den) {
        return Error::InvalidValue;
    }

    if (cfg.scan_mode != capability.scan_mode) {
        return Error::InvalidValue;
    }

    if (cfg.packing_mode != capability.packing_mode) {
        return Error::InvalidValue;
    }

    if (Error err = validate_video_transport_payload_format_matches_media_description(capability.transport_format,
                                                                                      capability.media);
        err != Error::Ok) {
        return err;
    }

    return Error::Ok;
}

[[nodiscard]] inline Error validate_rx_video_config_structure(const RxVideoConfig &cfg) {
    if (Error err = validate_rx_video_config_common_transport_fields(cfg); err != Error::Ok) {
        return err;
    }

    /*
     * This validates only that the project storage enum value is known.
     * It intentionally does not validate VideoFrame storage constraints here.
     */
    if (auto project_handoff = video_frame_handoff_format_from_project_pixel_format(cfg.format); !project_handoff) {
        return project_handoff.error();
    }

    auto capability = rx_video_config_effective_receive_capability(cfg);
    if (!capability.has_value()) {
        return capability.error();
    }

    if (Error err = validate_video_receive_capability_structure(*capability); err != Error::Ok) {
        return err;
    }

    if (cfg.receive_capability.has_value()) {
        if (Error err = validate_rx_video_config_matches_explicit_receive_capability(cfg, *capability);
            err != Error::Ok) {
            return err;
        }
    }

    return Error::Ok;
}

[[nodiscard]] inline Error
validate_rx_video_config_against_project_delivery_support(const RxVideoConfig &cfg,
                                                          const VideoProjectDeliverySupportPolicy &support) {
    if (Error err = validate_rx_video_config_structure(cfg); err != Error::Ok) {
        return err;
    }

    auto capability = rx_video_config_effective_receive_capability(cfg);
    if (!capability.has_value()) {
        return capability.error();
    }

    if (support.require_project_handoff_format_support) {
        if (Error err = validate_project_video_frame_handoff_format_matches_pixel_format(capability->handoff_format,
                                                                                         cfg.format);
            err != Error::Ok) {
            return err;
        }
    }

    if (support.require_project_pixel_format_storage_compatibility) {
        if (Error err = validate_project_video_frame_storage_compatibility(*capability, cfg.format); err != Error::Ok) {
            return err;
        }
    }

    return Error::Ok;
}

/*
 * Compatibility wrapper that composes:
 * 1) project-delivery support;
 * 2) generic runtime packing-mode support.
 *
 * This helper must not be used to reintroduce backend-specific narrowing
 * into common validation paths.
 */
[[nodiscard]] inline Error validate_rx_video_config_against_runtime_support(const RxVideoConfig &cfg,
                                                                            const VideoRuntimeSupportPolicy &support) {
    if (Error err = validate_rx_video_config_against_project_delivery_support(cfg, support.project_delivery);
        err != Error::Ok) {
        return err;
    }

    auto capability = rx_video_config_effective_receive_capability(cfg);
    if (!capability.has_value()) {
        return capability.error();
    }

    if (support.require_runtime_packing_mode_support) {
        if (Error err = validate_runtime_video_packing_mode_support(capability->packing_mode); err != Error::Ok) {
            return err;
        }
    }

    return Error::Ok;
}

enum class AudioSampleFormat { LinearPcm };

struct RxAudioConfig;
[[nodiscard]] Error validate_rx_audio_config(const RxAudioConfig &cfg);

struct RxAudioConfig {
    uint32_t sampling_rate_hz = 0;
    uint32_t packet_time_us = 0;
    uint32_t samples_per_packet = 0;
    uint16_t channel_count = 0;

    uint16_t udp_port = 0;
    uint8_t payload_type = 0;
    std::string local_ip;
    std::string dest_ip;

    AudioSampleFormat format = AudioSampleFormat::LinearPcm;
    AudioPcmBitDepth pcm_bit_depth = AudioPcmBitDepth::Bits24;

    [[nodiscard]] bool is_valid() const { return validate_rx_audio_config(*this) == Error::Ok; }
};

struct AudioRuntimeSupportPolicy {
    std::span<const AudioSampleFormat> sample_formats;
    std::span<const AudioConformanceRange> conformance_ranges;
};

namespace audio_runtime_support {
inline constexpr auto default_sample_formats = std::array{AudioSampleFormat::LinearPcm};

inline constexpr auto default_conformance_ranges = std::array{audio_level_a_receiver_baseline()};
} // namespace audio_runtime_support

[[nodiscard]] inline bool audio_sample_format_supported(AudioSampleFormat format,
                                                        std::span<const AudioSampleFormat> supported_formats) {
    for (AudioSampleFormat supported : supported_formats) {
        if (format == supported) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] inline std::expected<AudioMediaDescription, Error>
audio_media_description_from_rx_audio_config(const RxAudioConfig &cfg) {
    AudioPcmEncoding pcmEncoding;
    switch (cfg.format) {
    case AudioSampleFormat::LinearPcm:
        pcmEncoding = AudioPcmEncoding::LinearPcm;
        break;
    default:
        return std::unexpected(Error::Unsupported);
    }

    return AudioMediaDescription{
        .pcm_encoding = pcmEncoding,
        .pcm_bit_depth = cfg.pcm_bit_depth,
        .sampling_rate_hz = cfg.sampling_rate_hz,
        .packet_time_us = cfg.packet_time_us,
        .channel_count = cfg.channel_count,
    };
}

[[nodiscard]] inline bool rx_audio_config_matches_any_conformance_range(const RxAudioConfig &cfg,
                                                                        std::span<const AudioConformanceRange> ranges) {
    auto media = audio_media_description_from_rx_audio_config(cfg);
    if (!media) {
        return false;
    }

    for (const AudioConformanceRange &range : ranges) {
        if (audio_media_description_matches_conformance_range(*media, range)) {
            return true;
        }
    }

    return false;
}

[[nodiscard]] inline Error validate_rx_audio_config_against_runtime_support(const RxAudioConfig &cfg,
                                                                            const AudioRuntimeSupportPolicy &support) {
    if (!audio_sample_format_supported(cfg.format, support.sample_formats)) {
        return Error::Unsupported;
    }

    if (Error err = validate_audio_pcm_bit_depth(cfg.pcm_bit_depth); err != Error::Ok) {
        return err;
    }

    if (!rx_audio_config_matches_any_conformance_range(cfg, support.conformance_ranges)) {
        return Error::Unsupported;
    }

    auto expected_samples_per_packet =
        audio_samples_per_packet_from_rate_and_packet_time(cfg.sampling_rate_hz, cfg.packet_time_us);

    if (!expected_samples_per_packet || cfg.samples_per_packet != *expected_samples_per_packet) {
        return Error::InvalidValue;
    }

    if (cfg.udp_port == 0) {
        return Error::InvalidValue;
    }

    if (cfg.payload_type < 96 || cfg.payload_type > 127) {
        return Error::InvalidValue;
    }

    if (cfg.dest_ip.empty()) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

[[nodiscard]] inline AudioRuntimeSupportPolicy default_audio_rx_runtime_support_policy() {
    return AudioRuntimeSupportPolicy{
        std::span<const AudioSampleFormat>{audio_runtime_support::default_sample_formats},
        std::span<const AudioConformanceRange>{audio_runtime_support::default_conformance_ranges}};
}

[[nodiscard]] inline Error validate_rx_audio_config(const RxAudioConfig &cfg) {
    return validate_rx_audio_config_against_runtime_support(cfg, default_audio_rx_runtime_support_policy());
}

[[nodiscard]] inline Error validate_rx_video_config(const RxVideoConfig &cfg) {
    return validate_rx_video_config_structure(cfg);
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_RX_CONFIG_HPP
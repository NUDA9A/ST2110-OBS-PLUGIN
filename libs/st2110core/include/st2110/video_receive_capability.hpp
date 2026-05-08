#ifndef ST2110_OBS_PLUGIN_VIDEO_RECEIVE_CAPABILITY_HPP
#define ST2110_OBS_PLUGIN_VIDEO_RECEIVE_CAPABILITY_HPP

#include <cstdint>
#include <expected>
#include <optional>
#include <string>

#include "delivery/video/pixel_format.hpp"
#include "foundation/error.hpp"
#include <st2110/model/video/video_media_types.hpp>
#include <st2110/receive/video/video_receive_description.hpp>

namespace st2110 {
enum class VideoFrameHandoffFormat {
    Uyvy,
    Yuv422Planar8,
    Yuv422Planar10Le,
    Yuv422Planar12Le,
    Yuv422Planar16Le,
    V210,
    Y210,
    Yuv422Rfc4175Pgroup2Be10,
    Yuv422Rfc4175Pgroup2Be12,
    Yuv444Planar10Le,
    Yuv444Planar12Le,
    Yuv444Rfc4175Pgroup4Be10,
    Yuv444Rfc4175Pgroup2Be12,
    Yuv420Planar8,
    Argb,
    Bgra,
    Rgb8,
    GbrPlanar10Le,
    GbrPlanar12Le,
    RgbRfc4175Pgroup4Be10,
    RgbRfc4175Pgroup2Be12,
};

enum class VideoReceiveTopologyKind {
    SingleStream,
    RedundantStreams,
};

struct VideoReceiveTopology {
    VideoReceiveTopologyKind kind = VideoReceiveTopologyKind::SingleStream;
    std::uint8_t stream_count = 1;
    std::optional<std::string> primary_mid{};
    std::optional<std::string> redundant_mid{};
};

struct VideoReceiveCapability {
    VideoMediaDescription media{};
    VideoScanMode scan_mode = VideoScanMode::Progressive;
    VideoPackingMode packing_mode = VideoPackingMode::Gpm;
    VideoTransportPayloadFormat transport_format = VideoTransportPayloadFormat::Rfc4175Ycbcr422_8Bit;
    VideoFrameHandoffFormat handoff_format = VideoFrameHandoffFormat::Uyvy;
    VideoReceiveRtpClock rtp_clock{};
    VideoReceiveTopology topology{};
};

[[nodiscard]] inline Error validate_video_receive_rtp_clock(const VideoReceiveRtpClock &clock) noexcept {
    if (clock.rtp_clock_rate == 0) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

[[nodiscard]] inline Error validate_video_receive_topology(const VideoReceiveTopology &topology) noexcept {
    switch (topology.kind) {
    case VideoReceiveTopologyKind::SingleStream:
        if (topology.stream_count != 1) {
            return Error::InvalidValue;
        }
        if (topology.redundant_mid.has_value()) {
            return Error::InvalidValue;
        }
        return Error::Ok;

    case VideoReceiveTopologyKind::RedundantStreams:
        if (topology.stream_count < 2) {
            return Error::InvalidValue;
        }
        if (topology.primary_mid.has_value() && topology.redundant_mid.has_value() &&
            *topology.primary_mid == *topology.redundant_mid) {
            return Error::InvalidValue;
        }
        return Error::Ok;

    default:
        return Error::InvalidValue;
    }
}

[[nodiscard]] inline bool video_media_description_is_integer_sampling_depth(const VideoMediaDescription &media,
                                                                            VideoSampling::Known sampling,
                                                                            std::uint8_t bits) {
    return media.sampling.known == sampling && !media.depth.floating_point && media.depth.bits == bits;
}

[[nodiscard]] inline Error
validate_video_frame_handoff_format_matches_media_description(VideoFrameHandoffFormat handoff_format,
                                                              const VideoMediaDescription &media) {
    if (Error err = validate_video_media_description_structure(media); err != Error::Ok) {
        return err;
    }

    switch (handoff_format) {
    case VideoFrameHandoffFormat::Uyvy:
    case VideoFrameHandoffFormat::Yuv422Planar8:
        return video_media_description_is_integer_sampling_depth(media, VideoSampling::Known::YCbCr422, 8)
                   ? Error::Ok
                   : Error::InvalidValue;

    case VideoFrameHandoffFormat::Yuv422Planar10Le:
    case VideoFrameHandoffFormat::V210:
    case VideoFrameHandoffFormat::Y210:
    case VideoFrameHandoffFormat::Yuv422Rfc4175Pgroup2Be10:
        return video_media_description_is_integer_sampling_depth(media, VideoSampling::Known::YCbCr422, 10)
                   ? Error::Ok
                   : Error::InvalidValue;

    case VideoFrameHandoffFormat::Yuv422Planar12Le:
    case VideoFrameHandoffFormat::Yuv422Rfc4175Pgroup2Be12:
        return video_media_description_is_integer_sampling_depth(media, VideoSampling::Known::YCbCr422, 12)
                   ? Error::Ok
                   : Error::InvalidValue;

    case VideoFrameHandoffFormat::Yuv422Planar16Le:
        return video_media_description_is_integer_sampling_depth(media, VideoSampling::Known::YCbCr422, 16)
                   ? Error::Ok
                   : Error::InvalidValue;

    case VideoFrameHandoffFormat::Yuv444Planar10Le:
    case VideoFrameHandoffFormat::Yuv444Rfc4175Pgroup4Be10:
        return video_media_description_is_integer_sampling_depth(media, VideoSampling::Known::YCbCr444, 10)
                   ? Error::Ok
                   : Error::InvalidValue;

    case VideoFrameHandoffFormat::Yuv444Planar12Le:
    case VideoFrameHandoffFormat::Yuv444Rfc4175Pgroup2Be12:
        return video_media_description_is_integer_sampling_depth(media, VideoSampling::Known::YCbCr444, 12)
                   ? Error::Ok
                   : Error::InvalidValue;

    case VideoFrameHandoffFormat::Yuv420Planar8:
        return video_media_description_is_integer_sampling_depth(media, VideoSampling::Known::YCbCr420, 8)
                   ? Error::Ok
                   : Error::InvalidValue;

    case VideoFrameHandoffFormat::Argb:
    case VideoFrameHandoffFormat::Bgra:
    case VideoFrameHandoffFormat::Rgb8:
        return video_media_description_is_integer_sampling_depth(media, VideoSampling::Known::RGB, 8)
                   ? Error::Ok
                   : Error::InvalidValue;

    case VideoFrameHandoffFormat::GbrPlanar10Le:
    case VideoFrameHandoffFormat::RgbRfc4175Pgroup4Be10:
        return video_media_description_is_integer_sampling_depth(media, VideoSampling::Known::RGB, 10)
                   ? Error::Ok
                   : Error::InvalidValue;

    case VideoFrameHandoffFormat::GbrPlanar12Le:
    case VideoFrameHandoffFormat::RgbRfc4175Pgroup2Be12:
        return video_media_description_is_integer_sampling_depth(media, VideoSampling::Known::RGB, 12)
                   ? Error::Ok
                   : Error::InvalidValue;

    default:
        return Error::InvalidValue;
    }
}

[[nodiscard]] inline Error
validate_video_receive_capability_structure(const VideoReceiveCapability &capability) noexcept {
    if (Error err = validate_video_media_description_structure(capability.media); err != Error::Ok) {
        return err;
    }

    if (Error err = validate_video_media_description_cross_field_constraints(capability.media, capability.scan_mode);
        err != Error::Ok) {
        return err;
    }

    if (Error err = validate_video_receive_rtp_clock(capability.rtp_clock); err != Error::Ok) {
        return err;
    }

    if (Error err = validate_video_receive_topology(capability.topology); err != Error::Ok) {
        return err;
    }

    if (Error err = validate_video_transport_payload_format_matches_media_description(capability.transport_format,
                                                                                      capability.media);
        err != Error::Ok) {
        return err;
    }

    return Error::Ok;
}

[[nodiscard]] inline std::expected<VideoFrameHandoffFormat, Error>
video_frame_handoff_format_from_project_pixel_format(PixelFormat format) {
    switch (format) {
    case PixelFormat::UYVY:
        return VideoFrameHandoffFormat::Uyvy;
    default:
        return std::unexpected(Error::InvalidValue);
    }
}

[[nodiscard]] inline std::expected<PixelFormat, Error>
project_pixel_format_from_video_frame_handoff_format(VideoFrameHandoffFormat format) {
    switch (format) {
    case VideoFrameHandoffFormat::Uyvy:
        return PixelFormat::UYVY;
    default:
        return std::unexpected(Error::Unsupported);
    }
}

[[nodiscard]] inline bool
video_frame_handoff_format_maps_to_project_pixel_format(VideoFrameHandoffFormat format) noexcept {
    return project_pixel_format_from_video_frame_handoff_format(format).has_value();
}

[[nodiscard]] inline Error validate_project_video_frame_handoff_format_support(VideoFrameHandoffFormat handoff_format) {
    auto project_format = project_pixel_format_from_video_frame_handoff_format(handoff_format);
    if (!project_format.has_value()) {
        return project_format.error();
    }

    return Error::Ok;
}

[[nodiscard]] inline Error
validate_project_video_frame_handoff_format_matches_pixel_format(VideoFrameHandoffFormat handoff_format,
                                                                 PixelFormat pixel_format) {
    auto project_format = project_pixel_format_from_video_frame_handoff_format(handoff_format);
    if (!project_format.has_value()) {
        return project_format.error();
    }

    if (pixel_format != *project_format) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

[[nodiscard]] inline Error validate_video_format_constraints(PixelFormat fmt, uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        return Error::InvalidValue;
    }

    switch (fmt) {
    case PixelFormat::UYVY:
        if ((width % 2) != 0) {
            return Error::InvalidValue;
        }
        return Error::Ok;
    default:
        return Error::Unsupported;
    }
}

[[nodiscard]] inline Error validate_project_video_frame_storage_compatibility(const VideoReceiveCapability &capability,
                                                                              PixelFormat pixel_format) {
    if (Error err = validate_video_receive_capability_structure(capability); err != Error::Ok) {
        return err;
    }

    switch (pixel_format) {
    case PixelFormat::UYVY:
        /*
         * Current project storage path:
         * - supports only integer YCbCr 4:2:2 8-bit in UYVY storage;
         * - structurally valid but non-deliverable media must return Unsupported;
         * - malformed/contradictory capability values must already have been rejected
         *   by validate_video_receive_capability_structure(...).
         */
        if (capability.media.depth.floating_point) {
            return Error::Unsupported;
        }

        switch (capability.media.sampling.known) {
        case VideoSampling::Known::YCbCr422:
            switch (capability.media.depth.bits) {
            case 8:
                break;
            case 10:
            case 12:
            case 16:
                return Error::Unsupported;
            default:
                return Error::InvalidValue;
            }
            break;

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
            return Error::Unsupported;

        default:
            return Error::InvalidValue;
        }

        return validate_video_format_constraints(pixel_format, capability.media.width,
                                                                    capability.media.height);

    default:
        return Error::InvalidValue;
    }
}

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_VIDEO_RECEIVE_CAPABILITY_HPP
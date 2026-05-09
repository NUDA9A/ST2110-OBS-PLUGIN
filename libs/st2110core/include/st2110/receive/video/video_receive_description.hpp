#ifndef ST2110_OBS_VIDEO_RECEIVE_DESCRIPTION_HPP
#define ST2110_OBS_VIDEO_RECEIVE_DESCRIPTION_HPP

#include <st2110/delivery/video/pixel_format.hpp>
#include <st2110/delivery/video/video_handoff_format.hpp>
#include <st2110/model/video/video_media_types.hpp>

#include <cstdint>
#include <expected>
#include <optional>
#include <string>

namespace st2110 {

struct VideoReceiveRtpClock {
    std::uint32_t rtp_clock_rate = 90000;
};

enum class VideoTransportPayloadFormat {
    /*
     * Generic RFC 4175 / ST 2110-20 payload transport.
     * Exact sampling/depth/colorimetry/range semantics remain in VideoMediaDescription.
     */
    Rfc4175,

    Rfc4175Ycbcr422_8Bit,
    Rfc4175Ycbcr422_10Bit,
    Rfc4175Ycbcr422_12Bit,
    Rfc4175Ycbcr422_16Bit,
    Rfc4175Ycbcr420_8Bit,
    Rfc4175Ycbcr420_10Bit,
    Rfc4175Ycbcr420_12Bit,
    Rfc4175Ycbcr420_16Bit,
    Rfc4175Rgb_8Bit,
    Rfc4175Rgb_10Bit,
    Rfc4175Rgb_12Bit,
    Rfc4175Rgb_16Bit,
    Rfc4175Ycbcr444_8Bit,
    Rfc4175Ycbcr444_10Bit,
    Rfc4175Ycbcr444_12Bit,
    Rfc4175Ycbcr444_16Bit,

    CustomYcbcr422Planar10Le,
    CustomV210,
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

[[nodiscard]] inline std::expected<VideoTransportPayloadFormat, Error>
video_transport_payload_format_from_media_description(const VideoMediaDescription &media) {
    if (Error err = validate_video_media_description_structure(media); err != Error::Ok) {
        return std::unexpected(err);
    }

    if (media.depth.floating_point) {
        switch (media.sampling.known) {
        case VideoSampling::Known::YCbCr422:
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
            return VideoTransportPayloadFormat::Rfc4175;

        case VideoSampling::Known::Other:
            return std::unexpected(Error::Unsupported);

        default:
            return std::unexpected(Error::InvalidValue);
        }
    }

    switch (media.sampling.known) {
    case VideoSampling::Known::YCbCr422:
        switch (media.depth.bits) {
        case 8:
            return VideoTransportPayloadFormat::Rfc4175Ycbcr422_8Bit;
        case 10:
            return VideoTransportPayloadFormat::Rfc4175Ycbcr422_10Bit;
        case 12:
            return VideoTransportPayloadFormat::Rfc4175Ycbcr422_12Bit;
        case 16:
            return VideoTransportPayloadFormat::Rfc4175Ycbcr422_16Bit;
        default:
            return std::unexpected(Error::InvalidValue);
        }

    case VideoSampling::Known::YCbCr420:
        switch (media.depth.bits) {
        case 8:
            return VideoTransportPayloadFormat::Rfc4175Ycbcr420_8Bit;
        case 10:
            return VideoTransportPayloadFormat::Rfc4175Ycbcr420_10Bit;
        case 12:
            return VideoTransportPayloadFormat::Rfc4175Ycbcr420_12Bit;
        case 16:
            return VideoTransportPayloadFormat::Rfc4175Ycbcr420_16Bit;
        default:
            return std::unexpected(Error::InvalidValue);
        }

    case VideoSampling::Known::RGB:
        switch (media.depth.bits) {
        case 8:
            return VideoTransportPayloadFormat::Rfc4175Rgb_8Bit;
        case 10:
            return VideoTransportPayloadFormat::Rfc4175Rgb_10Bit;
        case 12:
            return VideoTransportPayloadFormat::Rfc4175Rgb_12Bit;
        case 16:
            return VideoTransportPayloadFormat::Rfc4175Rgb_16Bit;
        default:
            return std::unexpected(Error::InvalidValue);
        }

    case VideoSampling::Known::YCbCr444:
        switch (media.depth.bits) {
        case 8:
            return VideoTransportPayloadFormat::Rfc4175Ycbcr444_8Bit;
        case 10:
            return VideoTransportPayloadFormat::Rfc4175Ycbcr444_10Bit;
        case 12:
            return VideoTransportPayloadFormat::Rfc4175Ycbcr444_12Bit;
        case 16:
            return VideoTransportPayloadFormat::Rfc4175Ycbcr444_16Bit;
        default:
            return std::unexpected(Error::InvalidValue);
        }

    case VideoSampling::Known::XYZ:
    case VideoSampling::Known::Key:
    case VideoSampling::Known::CLYCbCr444:
    case VideoSampling::Known::CLYCbCr422:
    case VideoSampling::Known::CLYCbCr420:
    case VideoSampling::Known::ICtCp444:
    case VideoSampling::Known::ICtCp422:
    case VideoSampling::Known::ICtCp420:
        return VideoTransportPayloadFormat::Rfc4175;

    case VideoSampling::Known::Other:
        return std::unexpected(Error::Unsupported);

    default:
        return std::unexpected(Error::InvalidValue);
    }
}

[[nodiscard]] inline bool video_media_description_is_ycbcr422_integer_depth(const VideoMediaDescription &media,
                                                                            std::uint8_t bits) {
    return media.sampling.known == VideoSampling::Known::YCbCr422 && !media.depth.floating_point &&
           media.depth.bits == bits;
}

[[nodiscard]] inline Error
validate_video_transport_payload_format_matches_media_description(VideoTransportPayloadFormat format,
                                                                  const VideoMediaDescription &media) {
    if (Error err = validate_video_media_description_structure(media); err != Error::Ok) {
        return err;
    }

    switch (format) {
    case VideoTransportPayloadFormat::Rfc4175: {
        auto projected_format = video_transport_payload_format_from_media_description(media);
        if (!projected_format.has_value()) {
            return projected_format.error();
        }
        return Error::Ok;
    }

    case VideoTransportPayloadFormat::CustomYcbcr422Planar10Le:
    case VideoTransportPayloadFormat::CustomV210:
        return video_media_description_is_ycbcr422_integer_depth(media, 10) ? Error::Ok : Error::InvalidValue;

    default:
        break;
    }

    auto expected_format = video_transport_payload_format_from_media_description(media);
    if (!expected_format.has_value()) {
        return expected_format.error();
    }

    if (format != *expected_format) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

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

    if (Error err =
            validate_video_frame_handoff_format_matches_media_description(capability.handoff_format, capability.media);
        err != Error::Ok) {
        return err;
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

        return validate_video_format_constraints(pixel_format, capability.media.width, capability.media.height);

    default:
        return Error::InvalidValue;
    }
}

} // namespace st2110

#endif // ST2110_OBS_VIDEO_RECEIVE_DESCRIPTION_HPP
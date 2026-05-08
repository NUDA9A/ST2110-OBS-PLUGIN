#ifndef ST2110_OBS_VIDEO_RECEIVE_DESCRIPTION_HPP
#define ST2110_OBS_VIDEO_RECEIVE_DESCRIPTION_HPP

#include <st2110/model/video/video_media_types.hpp>

#include <expected>

namespace st2110 {
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
        /*
         * Generic RFC 4175 / ST 2110-20 transport marker.
         * It is valid for known media shapes that can be projected onto RFC 4175 transport.
         */
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
}

#endif // ST2110_OBS_VIDEO_RECEIVE_DESCRIPTION_HPP

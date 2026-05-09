#ifndef ST2110_OBS_VIDEO_HANDOFF_FORMAT_HPP
#define ST2110_OBS_VIDEO_HANDOFF_FORMAT_HPP

#include <st2110/delivery/video/pixel_format.hpp>
#include <st2110/foundation/error.hpp>
#include <st2110/model/video/video_media_types.hpp>

#include <cstdint>
#include <expected>

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

[[nodiscard]] inline Error
validate_project_video_frame_handoff_format_support(VideoFrameHandoffFormat handoff_format) {
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

} // namespace st2110

#endif // ST2110_OBS_VIDEO_HANDOFF_FORMAT_HPP
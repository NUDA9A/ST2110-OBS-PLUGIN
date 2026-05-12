#ifndef ST2110_OBS_VIDEO_RECEICE_PIPELINE_CONFIG_HPP
#define ST2110_OBS_VIDEO_RECEICE_PIPELINE_CONFIG_HPP

#include <st2110/contracts/video/depacketizer_config.hpp>
#include <st2110/contracts/video/video_unit_reconstructor_config.hpp>
#include <st2110/model/video/video_media_types.hpp>

namespace st2110 {
struct VideoReceivePipelineConfig {
    DepacketizerConfig depacketizer{};
    VideoUnitReconstructorConfig reconstructor{};
};

[[nodiscard]] inline PixelFormat pixel_format_from_video_stream_signaling(const VideoMediaDescription &media) {
    switch (media.sampling.known) {
    case VideoSampling::Known::YCbCr422:
        switch (media.depth.bits) {
        case 8:
            return PixelFormat::UYVY;
        case 10:
            return PixelFormat::YUV422RFC4175PG2BE10;
        case 12:
            return PixelFormat::YUV422RFC4175PG2BE12;
        default:
            throw std::runtime_error("Unsupported format");
        }
    case VideoSampling::Known::YCbCr444:
        switch (media.depth.bits) {
        case 10:
            return PixelFormat::YUV444RFC4175PG4BE10;
        case 12:
            return PixelFormat::YUV444RFC4175PG2BE12;
        default:
            throw std::runtime_error("Unsupported format");
        }
    case VideoSampling::Known::RGB:
        switch (media.depth.bits) {
        case 8:
            return PixelFormat::RGB8;
        case 10:
            return PixelFormat::RGBRFC4175PG4BE10;
        case 12:
            return PixelFormat::RGBRFC4175PG2BE12;
        default:
            throw std::runtime_error("Unsupported format");
        }
    case VideoSampling::Known::YCbCr420:
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
        throw std::runtime_error("Unsupported format");
    }
}

[[nodiscard]] inline VideoReceivePipelineConfig make_video_receive_pipeline_config(const VideoScanMode scan_mode,
                                                                                   const VideoMediaDescription &media,
                                                                                   const PartialUnitPolicy policy) {
    const PixelFormat format = pixel_format_from_video_stream_signaling(media);
    return VideoReceivePipelineConfig{
        .depacketizer = make_depacketizer_config(scan_mode, format, media.width, media.height, policy),
        .reconstructor = make_video_unit_reconstructorConfig(format, media.width, media.height)};
}
} // namespace st2110

#endif // ST2110_OBS_VIDEO_RECEICE_PIPELINE_CONFIG_HPP

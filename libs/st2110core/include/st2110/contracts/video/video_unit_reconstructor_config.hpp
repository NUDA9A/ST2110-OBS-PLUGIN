#ifndef ST2110_OBS_VIDEO_UNIT_RECONSTRUCTOR_CONFIG_HPP
#define ST2110_OBS_VIDEO_UNIT_RECONSTRUCTOR_CONFIG_HPP

#include <cstdint>
#include <st2110/delivery/video/pixel_format.hpp>

namespace st2110 {
struct VideoUnitReconstructorConfig {
    PixelFormat format = PixelFormat::UYVY;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

[[nodiscard]] inline VideoUnitReconstructorConfig
make_video_unit_reconstructorConfig(const PixelFormat format, const std::uint32_t width, const std::uint32_t height) {
    return VideoUnitReconstructorConfig{.format = format, .width = width, .height = height};
}
} // namespace st2110

#endif // ST2110_OBS_VIDEO_UNIT_RECONSTRUCTOR_CONFIG_HPP

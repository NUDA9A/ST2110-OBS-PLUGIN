#ifndef ST2110_OBS_PLUGIN_VIDEO_PACKING_MODE_HPP
#define ST2110_OBS_PLUGIN_VIDEO_PACKING_MODE_HPP

#include "error.hpp"

namespace st2110 {
enum class VideoPackingMode { Gpm, Bpm, GpmSingleLine };

[[nodiscard]] inline Error validate_video_packing_mode(VideoPackingMode mode) {
    switch (mode) {
    case VideoPackingMode::Gpm:
    case VideoPackingMode::Bpm:
    case VideoPackingMode::GpmSingleLine:
        return Error::Ok;
    default:
        return Error::InvalidValue;
    }
}

[[nodiscard]] inline Error validate_runtime_video_packing_mode_support(VideoPackingMode mode) {
    switch (mode) {
    case VideoPackingMode::Gpm:
        return Error::Ok;
    case VideoPackingMode::Bpm:
    case VideoPackingMode::GpmSingleLine:
        return Error::Unsupported;
    default:
        return Error::InvalidValue;
    }
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_VIDEO_PACKING_MODE_HPP
#ifndef ST2110_OBS_PLUGIN_VIDEO_PACKING_MODE_HPP
#define ST2110_OBS_PLUGIN_VIDEO_PACKING_MODE_HPP

#include "error.hpp"

namespace st2110 {
enum class VideoPackingMode { Gpm, Bpm };

inline Error validate_runtime_video_packing_mode_support(VideoPackingMode mode) {
    switch (mode) {
    case VideoPackingMode::Gpm:
        return Error::Ok;
    case VideoPackingMode::Bpm:
        return Error::Unsupported;
    default:
        return Error::InvalidValue;
    }
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_VIDEO_PACKING_MODE_HPP

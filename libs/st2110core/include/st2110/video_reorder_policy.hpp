#ifndef ST2110_OBS_PLUGIN_VIDEO_REORDER_POLICY_HPP
#define ST2110_OBS_PLUGIN_VIDEO_REORDER_POLICY_HPP

#include "error.hpp"

#include <cstdint>

namespace st2110 {

inline constexpr std::uint32_t defaultVideoReorderWindowPackets = 32;

struct VideoReorderBufferConfig {
    std::uint32_t window_size_packets = defaultVideoReorderWindowPackets;
};

[[nodiscard]] inline Error validate_video_reorder_buffer_config(const VideoReorderBufferConfig& cfg) noexcept {
    if (cfg.window_size_packets > 0) {
        return Error::Ok;
    }

    return Error::InvalidValue;
}

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_VIDEO_REORDER_POLICY_HPP
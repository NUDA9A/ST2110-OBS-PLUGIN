#ifndef ST2110_OBS_PLUGIN_VIDEO_REORDER_POLICY_HPP
#define ST2110_OBS_PLUGIN_VIDEO_REORDER_POLICY_HPP

#include "st2110/foundation/error.hpp"
#include "st2110/receive/shared/receive_reorder_tolerance_policy.hpp"

#include <cstdint>

namespace st2110 {

inline constexpr std::uint32_t defaultVideoReorderWindowPackets = 32;

struct VideoReorderBufferConfig {
    std::uint32_t window_size_packets = defaultVideoReorderWindowPackets;
    ReceiveReorderTolerancePolicy reorder_tolerance_policy{};
};

[[nodiscard]] inline Error validate_video_reorder_buffer_config(const VideoReorderBufferConfig &cfg) noexcept {
    if (cfg.window_size_packets == 0) {
        return Error::InvalidValue;
    }

    return validate_receive_reorder_tolerance_policy(cfg.reorder_tolerance_policy);
}

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_VIDEO_REORDER_POLICY_HPP
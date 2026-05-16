#ifndef ST2110_OBS_PLUGIN_RECEIVE_REORDER_TOLERANCE_POLICY_HPP
#define ST2110_OBS_PLUGIN_RECEIVE_REORDER_TOLERANCE_POLICY_HPP

#include <cstdint>

namespace st2110 {
enum class ReceiveReorderGapPolicy {
    WaitForMissing,
    FlushGapOnce,
    FlushAlways,
    FlushAfterTimeout,
    DropFrameOnGap,
    FlushOnMarkerBoundary,
    TopologyAwareWait,
    FlushAfterNPackets
};

inline constexpr std::uint32_t defaultReorderWindowPackets = 32;
inline constexpr std::uint32_t defaultFlushAfterNPackets = 8;

struct ReorderBufferConfig {
    std::uint32_t window_size_packets = defaultReorderWindowPackets;
    ReceiveReorderGapPolicy reorder_tolerance_policy = ReceiveReorderGapPolicy::WaitForMissing;

    std::uint32_t flush_after_n_packets = defaultFlushAfterNPackets;

    friend bool operator==(const ReorderBufferConfig &, const ReorderBufferConfig &) = default;
};
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_RECEIVE_REORDER_TOLERANCE_POLICY_HPP
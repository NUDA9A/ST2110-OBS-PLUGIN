#ifndef ST2110_OBS_PLUGIN_RECEIVE_REORDER_TOLERANCE_POLICY_HPP
#define ST2110_OBS_PLUGIN_RECEIVE_REORDER_TOLERANCE_POLICY_HPP

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

struct ReorderBufferConfig {
    std::uint32_t window_size_packets = defaultReorderWindowPackets;
    ReceiveReorderGapPolicy reorder_tolerance_policy = ReceiveReorderGapPolicy::WaitForMissing;
};
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_RECEIVE_REORDER_TOLERANCE_POLICY_HPP
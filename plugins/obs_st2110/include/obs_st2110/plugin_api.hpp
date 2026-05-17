#ifndef ST2110_OBS_PLUGIN_PLUGIN_API_HPP
#define ST2110_OBS_PLUGIN_PLUGIN_API_HPP

namespace obs_st2110 {

inline constexpr const char *pluginId = "st2110_obs";
inline constexpr const char *pluginName = "ST 2110 OBS Plugin";
inline constexpr const char *pluginDescription = "Receives SMPTE ST 2110 media streams into OBS.";

inline constexpr const char *sourceId = "st2110_source";
inline constexpr const char *sourceName = "ST 2110 Source";

inline constexpr const char *sourceSelectionPropertyId = "st2110_source_selection";

inline constexpr const char *sourceBackendPropertyId = "st2110_receive_backend";
inline constexpr const char *sourceBackendSocketValue = "socket";
inline constexpr const char *sourceBackendMtlValue = "mtl";

inline constexpr const char *sourceReorderGapPolicyPropertyId = "st2110_reorder_gap_policy";
inline constexpr const char *sourcePartialUnitPolicyPropertyId = "st2110_partial_unit_policy";

inline constexpr const char *sourceReorderGapPolicyWaitForMissingValue = "wait_for_missing";
inline constexpr const char *sourceReorderGapPolicyFlushGapOnceValue = "flush_gap_once";
inline constexpr const char *sourceReorderGapPolicyFlushAlwaysValue = "flush_always";
inline constexpr const char *sourceReorderGapPolicyFlushAfterTimeoutValue = "flush_after_timeout";
inline constexpr const char *sourceReorderGapPolicyDropFrameOnGapValue = "drop_frame_on_gap";
inline constexpr const char *sourceReorderGapPolicyFlushOnMarkerBoundaryValue = "flush_on_marker_boundary";
inline constexpr const char *sourceReorderGapPolicyTopologyAwareWaitValue = "topology_aware_wait";
inline constexpr const char *sourceReorderGapPolicyFlushAfterNPacketsValue = "flush_after_n_packets";

inline constexpr const char *sourcePartialUnitPolicyEmitWithFlagValue = "emit_with_flag";
inline constexpr const char *sourcePartialUnitPolicyDropValue = "drop";

inline constexpr const char *sourceRuntimeStatusPropertyId = "st2110_runtime_status";

inline constexpr const char *sourceRuntimeDebugCountersPropertyId = "st2110_runtime_debug_counters";
inline constexpr const char *sourceRefreshDebugCountersButtonPropertyId = "st2110_refresh_debug_counters";

inline constexpr const char *sourceStartReceiveButtonPropertyId = "st2110_start_receive";
inline constexpr const char *sourceStopReceiveButtonPropertyId = "st2110_stop_receive";

} // namespace obs_st2110

#endif // ST2110_OBS_PLUGIN_PLUGIN_API_HPP
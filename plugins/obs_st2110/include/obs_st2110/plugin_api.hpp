#ifndef ST2110_OBS_PLUGIN_PLUGIN_API_HPP
#define ST2110_OBS_PLUGIN_PLUGIN_API_HPP

namespace obs_st2110 {

inline constexpr const char *pluginId = "st2110_obs";
inline constexpr const char *pluginName = "ST 2110 OBS Plugin";
inline constexpr const char *pluginDescription = "Receives SMPTE ST 2110 media streams into OBS.";

inline constexpr const char *sourceId = "st2110_source";
inline constexpr const char *sourceName = "ST 2110 Source";

inline constexpr const char *sourceSelectionPropertyId = "st2110_source_selection";

inline constexpr const char *sourceStartWhenActivePropertyId = "st2110_start_when_active";

inline constexpr const char *sourceBackendPropertyId = "st2110_receive_backend";
inline constexpr const char *sourceBackendSocketValue = "socket";
inline constexpr const char *sourceBackendMtlValue = "mtl";

inline constexpr const char *sourcePlayoutDelayMsPropertyId = "st2110_playout_delay_ms";
inline constexpr const char *sourceReorderWindowPacketsPropertyId = "st2110_reorder_window_packets";

} // namespace obs_st2110

#endif // ST2110_OBS_PLUGIN_PLUGIN_API_HPP
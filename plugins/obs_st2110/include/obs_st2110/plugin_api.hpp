#ifndef ST2110_OBS_PLUGIN_PLUGIN_API_HPP
#define ST2110_OBS_PLUGIN_PLUGIN_API_HPP

namespace obs_st2110 {

inline constexpr const char *pluginId = "st2110_obs";
inline constexpr const char *pluginName = "ST 2110 OBS Plugin";
inline constexpr const char *pluginDescription = "Receives SMPTE ST 2110 media streams into OBS.";

inline constexpr const char *sourceId = "st2110_source";
inline constexpr const char *sourceName = "ST 2110 Source";

inline constexpr const char *sourceSelectionPropertyId = "st2110_source_selection";

} // namespace obs_st2110

#endif // ST2110_OBS_PLUGIN_PLUGIN_API_HPP
#include "st2110-source.hpp"

#include <obs_st2110/plugin_api.hpp>

#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("st2110_obs", "en-US")

namespace {
obs_source_info st2110_source_info{};
} // namespace

MODULE_EXPORT const char *obs_module_name() { return obs_st2110::pluginName; }

MODULE_EXPORT const char *obs_module_description() { return obs_st2110::pluginDescription; }

bool obs_module_load() {
    st2110_source_info = create_st2110_source_info();
    obs_register_source(&st2110_source_info);
    return true;
}

void obs_module_unload() {}
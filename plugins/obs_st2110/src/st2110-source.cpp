#include "st2110-source.hpp"

#include <obs_st2110/discovery_provider.hpp>
#include <obs_st2110/plugin_api.hpp>
#include <obs_st2110/source_config.hpp>
#include <obs_st2110/source_runtime.hpp>

#include <obs-module.h>

#include <cstdint>
#include <memory>
#include <string_view>

namespace {
struct St2110Source {
    std::unique_ptr<obs_st2110::SourceRuntime> runtime{};

    explicit St2110Source(obs_source_t *source) : runtime(std::make_unique<obs_st2110::SourceRuntime>(source)) {}
};

[[nodiscard]] obs_st2110::IDiscoveryProvider &discovery_provider() {
    static auto provider = obs_st2110::create_discovery_provider();
    return *provider;
}

[[nodiscard]] obs_st2110::SourceConfig read_source_config(obs_data_t *settings) {
    obs_st2110::SourceConfig config{};

    if (!settings) {
        return config;
    }

    const char *selection_key_text = obs_data_get_string(settings, obs_st2110::sourceSelectionPropertyId);
    const std::string_view selection_key = selection_key_text ? std::string_view(selection_key_text) : std::string_view{};

    if (!selection_key.empty()) {
        config.selected_source = discovery_provider().resolve_source(selection_key);
    }

    return config;
}

const char *st2110_source_get_name(void *) {
    return obs_st2110::sourceName;
}

void *st2110_source_create(obs_data_t *settings, obs_source_t *source) {
    auto ctx = std::make_unique<St2110Source>(source);
    ctx->runtime->update(read_source_config(settings));

    return ctx.release();
}

void st2110_source_destroy(void *data) {
    auto *ctx = static_cast<St2110Source *>(data);
    if (!ctx) {
        return;
    }

    ctx->runtime->stop();
    delete ctx;
}

void st2110_source_update(void *data, obs_data_t *settings) {
    auto *ctx = static_cast<St2110Source *>(data);
    if (!ctx) {
        return;
    }

    ctx->runtime->update(read_source_config(settings));
}

void st2110_source_activate(void *data) {
    auto *ctx = static_cast<St2110Source *>(data);
    if (!ctx) {
        return;
    }

    ctx->runtime->start();
}

void st2110_source_deactivate(void *data) {
    auto *ctx = static_cast<St2110Source *>(data);
    if (!ctx) {
        return;
    }

    ctx->runtime->stop();
}

void st2110_source_show(void *data) {
    auto *ctx = static_cast<St2110Source *>(data);
    if (!ctx) {
        return;
    }

    ctx->runtime->start();
}

void st2110_source_hide(void *data) {
    auto *ctx = static_cast<St2110Source *>(data);
    if (!ctx) {
        return;
    }

    ctx->runtime->stop();
}

obs_properties_t *st2110_source_get_properties(void *data) {
    (void)data;

    obs_properties_t *properties = obs_properties_create();

    obs_property_t *source_list =
        obs_properties_add_list(properties, obs_st2110::sourceSelectionPropertyId, "Source", OBS_COMBO_TYPE_LIST,
                                OBS_COMBO_FORMAT_STRING);

    obs_property_list_add_string(source_list, "No ST 2110 sources discovered", "");

    for (const auto &item : discovery_provider().list_sources()) {
        obs_property_list_add_string(source_list, item.display_name.c_str(), item.selection_key.c_str());
    }

    return properties;
}

void st2110_source_get_defaults(obs_data_t *settings) {
    obs_data_set_default_string(settings, obs_st2110::sourceSelectionPropertyId, "");
}

std::uint32_t st2110_source_get_width(void *data) {
    const auto *ctx = static_cast<const St2110Source *>(data);
    return ctx ? ctx->runtime->width() : 0;
}

std::uint32_t st2110_source_get_height(void *data) {
    const auto *ctx = static_cast<const St2110Source *>(data);
    return ctx ? ctx->runtime->height() : 0;
}
} // namespace

obs_source_info create_st2110_source_info() {
    obs_source_info info{};

    info.id = obs_st2110::sourceId;
    info.type = OBS_SOURCE_TYPE_INPUT;
    info.output_flags = OBS_SOURCE_ASYNC | OBS_SOURCE_VIDEO | OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_DUPLICATE;

    info.get_name = st2110_source_get_name;
    info.create = st2110_source_create;
    info.destroy = st2110_source_destroy;
    info.update = st2110_source_update;

    info.activate = st2110_source_activate;
    info.deactivate = st2110_source_deactivate;
    info.show = st2110_source_show;
    info.hide = st2110_source_hide;

    info.get_properties = st2110_source_get_properties;
    info.get_defaults = st2110_source_get_defaults;

    info.get_width = st2110_source_get_width;
    info.get_height = st2110_source_get_height;

    return info;
}
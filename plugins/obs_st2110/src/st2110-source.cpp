#include "st2110-source.hpp"

#include <obs_st2110/discovery_provider.hpp>
#include <obs_st2110/plugin_api.hpp>
#include <obs_st2110/source_config.hpp>
#include <obs_st2110/source_runtime.hpp>

#include <obs-module.h>

#include <cstdint>
#include <memory>
#include <string_view>

#ifndef ST2110_HAS_MTL_BACKEND
#define ST2110_HAS_MTL_BACKEND 0
#endif

namespace {
struct St2110Source {
    std::unique_ptr<obs_st2110::SourceRuntime> runtime{};

    explicit St2110Source(obs_source_t *source) : runtime(std::make_unique<obs_st2110::SourceRuntime>(source)) {}
};

[[nodiscard]] obs_st2110::IDiscoveryProvider &discovery_provider() {
    static auto provider = obs_st2110::create_discovery_provider();
    return *provider;
}

[[nodiscard]] st2110::ReceiveBackendKind read_receive_backend_kind(obs_data_t *settings) {
    const char *backend_text = obs_data_get_string(settings, obs_st2110::sourceBackendPropertyId);
    const std::string_view backend = backend_text ? std::string_view(backend_text) : std::string_view{};

    if (backend == obs_st2110::sourceBackendMtlValue) {
        return st2110::ReceiveBackendKind::Mtl;
    }

    return st2110::ReceiveBackendKind::Socket;
}

[[nodiscard]] st2110::TimestampNs read_playout_delay_ns(obs_data_t *settings) {
    const long long delay_ms = obs_data_get_int(settings, obs_st2110::sourcePlayoutDelayMsPropertyId);
    if (delay_ms <= 0) {
        return 0;
    }

    return static_cast<st2110::TimestampNs>(delay_ms) * 1'000'000ULL;
}

[[nodiscard]] std::uint32_t read_reorder_window_packets(obs_data_t *settings) {
    const long long value = obs_data_get_int(settings, obs_st2110::sourceReorderWindowPacketsPropertyId);
    if (value <= 0) {
        return st2110::defaultReorderWindowPackets;
    }

    return static_cast<std::uint32_t>(value);
}

[[nodiscard]] obs_st2110::SourceConfig read_source_config(obs_data_t *settings) {
    obs_st2110::SourceConfig config{};

    if (!settings) {
        return config;
    }

    config.start_when_active = obs_data_get_bool(settings, obs_st2110::sourceStartWhenActivePropertyId);

    const char *selection_key_text = obs_data_get_string(settings, obs_st2110::sourceSelectionPropertyId);
    const std::string_view selection_key = selection_key_text ? std::string_view(selection_key_text) : std::string_view{};

    if (!selection_key.empty()) {
        config.selected_source = discovery_provider().resolve_source(selection_key);
    }

    config.receive_settings.backend_kind = read_receive_backend_kind(settings);
    config.receive_settings.reorder_buffer_config.window_size_packets = read_reorder_window_packets(settings);
    config.playout_delay_ns = read_playout_delay_ns(settings);

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

    obs_properties_add_bool(properties, obs_st2110::sourceStartWhenActivePropertyId, "Start when active");

    obs_property_t *backend_list =
        obs_properties_add_list(properties, obs_st2110::sourceBackendPropertyId, "Receive backend", OBS_COMBO_TYPE_LIST,
                                OBS_COMBO_FORMAT_STRING);

    obs_property_list_add_string(backend_list, "Socket", obs_st2110::sourceBackendSocketValue);

#if ST2110_HAS_MTL_BACKEND
    obs_property_list_add_string(backend_list, "MTL", obs_st2110::sourceBackendMtlValue);
#endif

    obs_properties_add_int(properties, obs_st2110::sourcePlayoutDelayMsPropertyId, "Playout delay (ms)", 0, 5000, 1);

    obs_properties_add_int(properties, obs_st2110::sourceReorderWindowPacketsPropertyId, "Socket reorder window packets",
                           1, 4096, 1);

    return properties;
}

void st2110_source_get_defaults(obs_data_t *settings) {
    obs_data_set_default_string(settings, obs_st2110::sourceSelectionPropertyId, "");
    obs_data_set_default_bool(settings, obs_st2110::sourceStartWhenActivePropertyId, true);
    obs_data_set_default_string(settings, obs_st2110::sourceBackendPropertyId, obs_st2110::sourceBackendSocketValue);
    obs_data_set_default_int(settings, obs_st2110::sourcePlayoutDelayMsPropertyId, 0);
    obs_data_set_default_int(settings, obs_st2110::sourceReorderWindowPacketsPropertyId,
                             st2110::defaultReorderWindowPackets);
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
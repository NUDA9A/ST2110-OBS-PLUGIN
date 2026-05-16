#include "st2110-source.hpp"

#include <obs_st2110/discovery_provider.hpp>
#include <obs_st2110/plugin_api.hpp>
#include <obs_st2110/source_config.hpp>
#include <obs_st2110/source_runtime.hpp>

#include <obs-module.h>

#include <cstdint>
#include <memory>
#include <string>
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

[[nodiscard]] std::string runtime_status_text(const St2110Source *ctx) {
    if (!ctx || !ctx->runtime) {
        return "Runtime status is unavailable before the source instance is created.";
    }

    const bool running = ctx->runtime->running();
    const bool configured = ctx->runtime->configured();
    const std::string &last_error = ctx->runtime->last_error();

    const char *state_text = "Receive graph is stopped or idle.";
    if (running) {
        state_text = "Receive graph is running.";
    } else if (configured) {
        state_text = "Receive graph is configured but stopped.";
    }

    if (last_error.empty()) {
        return state_text;
    }

    return std::string(state_text) + " " + last_error;
}

[[nodiscard]] obs_text_info_type runtime_status_info_type(const St2110Source *ctx) {
    if (!ctx || !ctx->runtime) {
        return OBS_TEXT_INFO_NORMAL;
    }

    const std::string &last_error = ctx->runtime->last_error();

    if (last_error.empty()) {
        return OBS_TEXT_INFO_NORMAL;
    }

    if (ctx->runtime->running() || ctx->runtime->configured()) {
        return OBS_TEXT_INFO_NORMAL;
    }

    return OBS_TEXT_INFO_WARNING;
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

[[nodiscard]] std::uint32_t read_flush_after_n_packets(obs_data_t *settings) {
    const long long value = obs_data_get_int(settings, obs_st2110::sourceFlushAfterNPacketsPropertyId);
    if (value <= 0) {
        return st2110::defaultFlushAfterNPackets;
    }

    return static_cast<std::uint32_t>(value);
}

[[nodiscard]] st2110::ReceiveReorderGapPolicy read_reorder_gap_policy(obs_data_t *settings) {
    const char *policy_text = obs_data_get_string(settings, obs_st2110::sourceReorderGapPolicyPropertyId);
    const std::string_view policy = policy_text ? std::string_view(policy_text) : std::string_view{};

    if (policy == obs_st2110::sourceReorderGapPolicyFlushGapOnceValue) {
        return st2110::ReceiveReorderGapPolicy::FlushGapOnce;
    }

    if (policy == obs_st2110::sourceReorderGapPolicyFlushAlwaysValue) {
        return st2110::ReceiveReorderGapPolicy::FlushAlways;
    }

    if (policy == obs_st2110::sourceReorderGapPolicyFlushAfterTimeoutValue) {
        return st2110::ReceiveReorderGapPolicy::FlushAfterTimeout;
    }

    if (policy == obs_st2110::sourceReorderGapPolicyDropFrameOnGapValue) {
        return st2110::ReceiveReorderGapPolicy::DropFrameOnGap;
    }

    if (policy == obs_st2110::sourceReorderGapPolicyFlushOnMarkerBoundaryValue) {
        return st2110::ReceiveReorderGapPolicy::FlushOnMarkerBoundary;
    }

    if (policy == obs_st2110::sourceReorderGapPolicyTopologyAwareWaitValue) {
        return st2110::ReceiveReorderGapPolicy::TopologyAwareWait;
    }

    if (policy == obs_st2110::sourceReorderGapPolicyFlushAfterNPacketsValue) {
        return st2110::ReceiveReorderGapPolicy::FlushAfterNPackets;
    }

    return st2110::ReceiveReorderGapPolicy::WaitForMissing;
}

[[nodiscard]] st2110::PartialUnitPolicy read_partial_unit_policy(obs_data_t *settings) {
    const char *policy_text = obs_data_get_string(settings, obs_st2110::sourcePartialUnitPolicyPropertyId);
    const std::string_view policy = policy_text ? std::string_view(policy_text) : std::string_view{};

    if (policy == obs_st2110::sourcePartialUnitPolicyDropValue) {
        return st2110::PartialUnitPolicy::Drop;
    }

    return st2110::PartialUnitPolicy::EmitWithFlag;
}

[[nodiscard]] obs_st2110::SourceConfig read_source_config(obs_data_t *settings) {
    obs_st2110::SourceConfig config{};

    if (!settings) {
        return config;
    }

    const char *selection_key_text = obs_data_get_string(settings, obs_st2110::sourceSelectionPropertyId);
    const std::string_view selection_key =
        selection_key_text ? std::string_view(selection_key_text) : std::string_view{};

    if (!selection_key.empty()) {
        config.selected_source = discovery_provider().resolve_source(selection_key);
    }

    config.receive_settings.backend_kind = read_receive_backend_kind(settings);
    config.receive_settings.reorder_buffer_config.window_size_packets = read_reorder_window_packets(settings);
    config.receive_settings.reorder_buffer_config.reorder_tolerance_policy = read_reorder_gap_policy(settings);
    config.receive_settings.reorder_buffer_config.flush_after_n_packets = read_flush_after_n_packets(settings);
    config.receive_settings.partial_unit_policy = read_partial_unit_policy(settings);
    config.playout_delay_ns = read_playout_delay_ns(settings);

    return config;
}

const char *st2110_source_get_name(void *) { return obs_st2110::sourceName; }

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

    delete ctx;
}

void st2110_source_update(void *data, obs_data_t *settings) {
    auto *ctx = static_cast<St2110Source *>(data);
    if (!ctx) {
        return;
    }

    ctx->runtime->update(read_source_config(settings));
}

void st2110_source_activate(void *data) { (void)data; }

void st2110_source_deactivate(void *data) { (void)data; }

void st2110_source_show(void *data) { (void)data; }

void st2110_source_hide(void *data) { (void)data; }

bool st2110_source_start_receive_clicked(obs_properties_t *, obs_property_t *, void *data) {
    auto *ctx = static_cast<St2110Source *>(data);
    if (!ctx || !ctx->runtime) {
        return false;
    }

    ctx->runtime->start_receive();
    return true;
}

bool st2110_source_stop_receive_clicked(obs_properties_t *, obs_property_t *, void *data) {
    auto *ctx = static_cast<St2110Source *>(data);
    if (!ctx || !ctx->runtime) {
        return false;
    }

    ctx->runtime->stop_receive();
    return true;
}

obs_properties_t *st2110_source_get_properties(void *data) {
    const auto *ctx = static_cast<const St2110Source *>(data);

    obs_properties_t *properties = obs_properties_create();

    obs_property_t *source_list = obs_properties_add_list(properties, obs_st2110::sourceSelectionPropertyId, "Source",
                                                          OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

    obs_property_list_add_string(source_list, "No ST 2110 sources discovered", "");

    for (const auto &item : discovery_provider().list_sources()) {
        obs_property_list_add_string(source_list, item.display_name.c_str(), item.selection_key.c_str());
    }

    obs_property_t *status_text = obs_properties_add_text(properties, obs_st2110::sourceRuntimeStatusPropertyId,
                                                          runtime_status_text(ctx).c_str(), OBS_TEXT_INFO);
    obs_property_text_set_info_type(status_text, runtime_status_info_type(ctx));
    obs_property_text_set_info_word_wrap(status_text, true);

    obs_properties_add_button(properties, obs_st2110::sourceStartReceiveButtonPropertyId, "Start receive",
                              st2110_source_start_receive_clicked);

    obs_properties_add_button(properties, obs_st2110::sourceStopReceiveButtonPropertyId, "Stop receive",
                              st2110_source_stop_receive_clicked);

    obs_property_t *backend_list =
        obs_properties_add_list(properties, obs_st2110::sourceBackendPropertyId, "Receive backend", OBS_COMBO_TYPE_LIST,
                                OBS_COMBO_FORMAT_STRING);

    obs_property_list_add_string(backend_list, "Socket", obs_st2110::sourceBackendSocketValue);

#if ST2110_HAS_MTL_BACKEND
    obs_property_list_add_string(backend_list, "MTL", obs_st2110::sourceBackendMtlValue);
#endif

    obs_properties_add_int(properties, obs_st2110::sourcePlayoutDelayMsPropertyId, "Playout delay (ms)", 0, 5000, 1);

    obs_properties_add_int(properties, obs_st2110::sourceReorderWindowPacketsPropertyId,
                           "Socket reorder window packets", 1, 4096, 1);

    obs_property_t *reorder_policy_list =
        obs_properties_add_list(properties, obs_st2110::sourceReorderGapPolicyPropertyId, "Receive reorder gap policy",
                                OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

    obs_property_list_add_string(reorder_policy_list, "Wait for missing",
                                 obs_st2110::sourceReorderGapPolicyWaitForMissingValue);
    obs_property_list_add_string(reorder_policy_list, "Flush gap once",
                                 obs_st2110::sourceReorderGapPolicyFlushGapOnceValue);
    obs_property_list_add_string(reorder_policy_list, "Flush always",
                                 obs_st2110::sourceReorderGapPolicyFlushAlwaysValue);
    obs_property_list_add_string(reorder_policy_list, "Flush after timeout",
                                 obs_st2110::sourceReorderGapPolicyFlushAfterTimeoutValue);
    obs_property_list_add_string(reorder_policy_list, "Drop frame on gap",
                                 obs_st2110::sourceReorderGapPolicyDropFrameOnGapValue);
    obs_property_list_add_string(reorder_policy_list, "Flush on marker boundary",
                                 obs_st2110::sourceReorderGapPolicyFlushOnMarkerBoundaryValue);
    obs_property_list_add_string(reorder_policy_list, "Topology-aware wait",
                                 obs_st2110::sourceReorderGapPolicyTopologyAwareWaitValue);
    obs_property_list_add_string(reorder_policy_list, "Flush after N packets",
                                 obs_st2110::sourceReorderGapPolicyFlushAfterNPacketsValue);

    obs_properties_add_int(properties, obs_st2110::sourceFlushAfterNPacketsPropertyId, "Flush after N packets", 1, 4096,
                           1);

    obs_property_t *partial_policy_list =
        obs_properties_add_list(properties, obs_st2110::sourcePartialUnitPolicyPropertyId, "Partial unit policy",
                                OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

    obs_property_list_add_string(partial_policy_list, "Emit with partial flag",
                                 obs_st2110::sourcePartialUnitPolicyEmitWithFlagValue);
    obs_property_list_add_string(partial_policy_list, "Drop partial unit",
                                 obs_st2110::sourcePartialUnitPolicyDropValue);

    return properties;
}

void st2110_source_get_defaults(obs_data_t *settings) {
    obs_data_set_default_string(settings, obs_st2110::sourceSelectionPropertyId, "");
    obs_data_set_default_string(settings, obs_st2110::sourceBackendPropertyId, obs_st2110::sourceBackendSocketValue);
    obs_data_set_default_int(settings, obs_st2110::sourcePlayoutDelayMsPropertyId, 0);
    obs_data_set_default_int(settings, obs_st2110::sourceReorderWindowPacketsPropertyId,
                             st2110::defaultReorderWindowPackets);
    obs_data_set_default_string(settings, obs_st2110::sourceReorderGapPolicyPropertyId,
                            obs_st2110::sourceReorderGapPolicyWaitForMissingValue);
    obs_data_set_default_int(settings, obs_st2110::sourceFlushAfterNPacketsPropertyId,
                             st2110::defaultFlushAfterNPackets);
    obs_data_set_default_string(settings, obs_st2110::sourcePartialUnitPolicyPropertyId,
                                obs_st2110::sourcePartialUnitPolicyEmitWithFlagValue);
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
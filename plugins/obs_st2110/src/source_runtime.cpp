#include <obs_st2110/source_runtime.hpp>

#include "obs-synchronized-frame-sink.hpp"

#include <obs_st2110/sdp_media_selection.hpp>
#include <obs_st2110/sdp_parser_dispatch.hpp>

#include <st2110/backends/mtl/mtl_worker_graph_client.hpp>
#include <st2110/backends/socket/socket_rx_audio_backend.hpp>
#include <st2110/backends/socket/socket_rx_video_backend.hpp>
#include <st2110/contracts/backend/backend.hpp>
#include <st2110/delivery/audio/socket_audio_start_config.hpp>
#include <st2110/delivery/synchronized_frame_sink.hpp>
#include <st2110/delivery/synchronized_playout_tuning.hpp>
#include <st2110/delivery/video/socket_video_start_config.hpp>
#include <st2110/foundation/error.hpp>
#include <st2110/foundation/rtp_timestamp_anchor_policy.hpp>
#include <st2110/receive/audio/audio_receive_bootstrap.hpp>
#include <st2110/receive/shared/receive_start_request.hpp>
#include <st2110/receive/video/video_receive_bootstrap.hpp>

#if ST2110_HAS_MTL_BACKEND
#include <st2110/backends/mtl/mtl_rx_audio_backend_proxy.hpp>
#include <st2110/backends/mtl/mtl_rx_video_backend_proxy.hpp>
#include <st2110/delivery/audio/mtl_audio_start_config.hpp>
#include <st2110/delivery/video/mtl_video_start_config.hpp>
#endif

#include <cstdint>
#include <exception>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#ifndef ST2110_HAS_MTL_BACKEND
#define ST2110_HAS_MTL_BACKEND 0
#endif

namespace obs_st2110 {
namespace {

[[nodiscard]] bool has_provider_selected_sdp(const SourceConfig &config) {
    return config.selected_source.has_value() && !config.selected_source->sdp_objects.empty();
}

[[nodiscard]] const char *describe_parsed_stream_composition(const ParsedSelectedSourceStreams &streams) noexcept {
    if (streams.has_video() && streams.has_audio()) {
        return "audio+video";
    }

    if (streams.has_video()) {
        return "video-only";
    }

    if (streams.has_audio()) {
        return "audio-only";
    }

    return "empty";
}

[[nodiscard]] std::expected<st2110::ReceiveStartRequest, st2110::Error>
make_video_receive_start_request(const st2110::ParsedSdpStreamSet &parsed) {
    st2110::VideoReceiveBootstrap bootstrap = st2110::project_parsed_video_sdp_to_receive_bootstrap(parsed);

    auto local = st2110::auto_select_receive_local_policy(bootstrap.receive_bootstrap);
    if (!local.has_value()) {
        return std::unexpected(local.error());
    }

    return st2110::ReceiveStartRequest{
        .media = std::move(bootstrap),
        .local = std::move(*local),
    };
}

[[nodiscard]] std::expected<st2110::ReceiveStartRequest, st2110::Error>
make_audio_receive_start_request(const st2110::ParsedSdpStreamSet &parsed) {
    st2110::AudioReceiveBootstrap bootstrap = st2110::project_parsed_audio_sdp_to_receive_bootstrap(parsed);

    auto local = st2110::auto_select_receive_local_policy(bootstrap.receive_bootstrap);
    if (!local.has_value()) {
        return std::unexpected(local.error());
    }

    return st2110::ReceiveStartRequest{
        .media = std::move(bootstrap),
        .local = std::move(*local),
    };
}

[[nodiscard]] std::expected<std::unique_ptr<st2110::IRxBackend>, st2110::Error>
make_video_backend(const st2110::ReceiveStartRequest &request, const st2110::Settings &settings,
                   const std::shared_ptr<st2110::MtlWorkerGraphClient> &mtl_graph_client) {
    (void)mtl_graph_client;
    switch (settings.backend_kind) {
    case st2110::ReceiveBackendKind::Socket: {
        return std::make_unique<st2110::SocketRxVideoBackend>(
            st2110::project_receive_start_request_to_socket_video_start(request, settings));
    }

    case st2110::ReceiveBackendKind::Mtl: {
#if ST2110_HAS_MTL_BACKEND
        auto mtl_cfg = st2110::project_receive_start_request_to_mtl_video_start(request, {});
        if (!mtl_cfg.has_value()) {
            return std::unexpected(mtl_cfg.error());
        }

        if (!mtl_graph_client) {
            return std::unexpected(st2110::Error::InvalidBackendState);
        }

        auto configured = mtl_graph_client->configure_video(*mtl_cfg);
        if (!configured.has_value()) {
            return std::unexpected(configured.error());
        }

        return std::make_unique<st2110::MtlRxVideoBackendProxy>(std::move(*mtl_cfg), mtl_graph_client);
#else
        return std::unexpected(st2110::Error::Unsupported);
#endif
    }
    }

    return std::unexpected(st2110::Error::Unsupported);
}

[[nodiscard]] std::expected<std::unique_ptr<st2110::IRxBackend>, st2110::Error>
make_audio_backend(const st2110::ReceiveStartRequest &request, const st2110::Settings &settings,
                   const std::shared_ptr<st2110::MtlWorkerGraphClient> &mtl_graph_client) {
    (void)mtl_graph_client;
    switch (settings.backend_kind) {
    case st2110::ReceiveBackendKind::Socket: {
        return std::make_unique<st2110::SocketRxAudioBackend>(
            st2110::project_receive_start_request_to_socket_audio_start(request, settings));
    }

    case st2110::ReceiveBackendKind::Mtl: {
#if ST2110_HAS_MTL_BACKEND
        auto mtl_cfg = st2110::project_receive_start_request_to_mtl_audio_start(request, {});
        if (!mtl_cfg.has_value()) {
            return std::unexpected(mtl_cfg.error());
        }

        if (!mtl_graph_client) {
            return std::unexpected(st2110::Error::InvalidBackendState);
        }

        auto configured = mtl_graph_client->configure_audio(*mtl_cfg);
        if (!configured.has_value()) {
            return std::unexpected(configured.error());
        }

        return std::make_unique<st2110::MtlRxAudioBackendProxy>(std::move(*mtl_cfg), mtl_graph_client);
#else
        return std::unexpected(st2110::Error::Unsupported);
#endif
    }
    }

    return std::unexpected(st2110::Error::Unsupported);
}

[[nodiscard]] st2110::SynchronizedFrameSinkConfig
make_sink_config(const std::optional<st2110::VideoReceiveBootstrap> &video_bootstrap,
                 const std::optional<st2110::AudioReceiveBootstrap> &audio_bootstrap) {
    const st2110::SynchronizedPlayoutTuning tuning =
        st2110::derive_synchronized_playout_tuning(video_bootstrap, audio_bootstrap);

    st2110::SynchronizedFrameSinkConfig cfg{};
    cfg.enable_video = video_bootstrap.has_value();
    cfg.enable_audio = audio_bootstrap.has_value();
    cfg.playout_delay_ns = tuning.playout_delay_ns;
    cfg.max_queued_video_frames = tuning.max_queued_video_frames;
    cfg.max_queued_audio_blocks = tuning.max_queued_audio_blocks;

    const bool av_sync_domain = video_bootstrap.has_value() && audio_bootstrap.has_value();

    if (video_bootstrap.has_value()) {
        cfg.video_timestamp_mapper.rtp_clock_rate =
            video_bootstrap->stream.receive_signaled_stream.timing.rtp_clock_rate;

        if (av_sync_domain) {
            cfg.video_timestamp_mapper.initial_anchor_mode = st2110::RtpTimestampInitialAnchorMode::ConfiguredReference;
            cfg.video_timestamp_mapper.anchor_rtp_timestamp =
                video_bootstrap->stream.receive_signaled_stream.timing.media_clock.direct->rtp_clock_offset;
            cfg.video_timestamp_mapper.anchor_timestamp_ns = 0;
        }
    }

    if (audio_bootstrap.has_value()) {
        cfg.audio_timestamp_mapper.rtp_clock_rate =
            audio_bootstrap->stream.receive_signaled_stream.timing.rtp_clock_rate;

        if (av_sync_domain) {
            cfg.audio_timestamp_mapper.initial_anchor_mode = st2110::RtpTimestampInitialAnchorMode::ConfiguredReference;
            cfg.audio_timestamp_mapper.anchor_rtp_timestamp =
                audio_bootstrap->stream.receive_signaled_stream.timing.media_clock.direct->rtp_clock_offset;
            cfg.audio_timestamp_mapper.anchor_timestamp_ns = 0;
        }
    }

    return cfg;
}

[[nodiscard]] std::string backend_error_detail(const st2110::IRxBackend *backend) {
#if ST2110_HAS_MTL_BACKEND
    if (const auto *video = dynamic_cast<const st2110::MtlRxVideoBackendProxy *>(backend)) {
        return video->last_error_message();
    }

    if (const auto *audio = dynamic_cast<const st2110::MtlRxAudioBackendProxy *>(backend)) {
        return audio->last_error_message();
    }
#else
    (void)backend;
#endif

    return {};
}

#if ST2110_HAS_MTL_BACKEND
[[nodiscard]] std::expected<std::optional<st2110::MtlRuntimeConfig>, st2110::Error>
resolve_configured_mtl_runtime_key(const std::shared_ptr<st2110::MtlWorkerGraphClient> &graph_client) {
    if (!graph_client) {
        return std::optional<st2110::MtlRuntimeConfig>{};
    }

    const auto &video = graph_client->video_config();
    const auto &audio = graph_client->audio_config();

    if (!video.has_value() && !audio.has_value()) {
        return std::unexpected(st2110::Error::InvalidBackendState);
    }

    if (video.has_value()) {
        st2110::MtlRuntimeConfig runtime = video->runtime;

        if (audio.has_value() && audio->runtime != runtime) {
            return std::unexpected(st2110::Error::InvalidValue);
        }

        return std::optional<st2110::MtlRuntimeConfig>{std::move(runtime)};
    }

    return std::optional<st2110::MtlRuntimeConfig>{audio->runtime};
}
#endif

[[nodiscard]] const char *backend_kind_name(const st2110::ReceiveBackendKind kind) noexcept {
    switch (kind) {
    case st2110::ReceiveBackendKind::Socket:
        return "Socket";
    case st2110::ReceiveBackendKind::Mtl:
        return "MTL";
    }

    return "Unknown";
}

void append_counter(std::string &out, const char *name, const std::uint64_t value) {
    out += "  ";
    out += name;
    out += ": ";
    out += std::to_string(value);
    out += "\n";
}

void append_backend_stats(std::string &out, const char *title, const st2110::IRxBackend *backend) {
    out += title;
    out += "\n";

    if (!backend) {
        out += "  unavailable\n";
        return;
    }

    out += "  health: ";
    out += backend->healthy() ? "healthy" : "unhealthy";
    out += "\n";

    const std::string backend_error = backend->last_error_message();
    if (!backend_error.empty()) {
        out += "  last_error: ";
        out += backend_error;
        out += "\n";
    }

    const st2110::BackendStats stats = backend->stats_snapshot();

    append_counter(out, "datagrams_received", stats.datagrams_received);
    append_counter(out, "bytes_received", stats.bytes_received);
    append_counter(out, "control_datagrams_ignored", stats.control_datagrams_ignored);
    append_counter(out, "nonmedia_datagrams_ignored", stats.nonmedia_datagrams_ignored);
    append_counter(out, "packets_parsed_ok", stats.packets_parsed_ok);
    append_counter(out, "packets_rejected", stats.packets_rejected);
    append_counter(out, "datagrams_dropped", stats.datagrams_dropped);
    append_counter(out, "frames_delivered", stats.frames_delivered);
    append_counter(out, "media_units_delivered", stats.media_units_delivered);
}

#if ST2110_HAS_MTL_BACKEND
void append_rx_port_stats(std::string &out, const char *prefix, const st2110::MtlWorkerRxPortStats &stats) {
    out += "  ";
    out += prefix;
    out += ".packets: ";
    out += std::to_string(stats.packets);
    out += "\n";

    out += "  ";
    out += prefix;
    out += ".bytes: ";
    out += std::to_string(stats.bytes);
    out += "\n";

    out += "  ";
    out += prefix;
    out += ".frames: ";
    out += std::to_string(stats.frames);
    out += "\n";

    out += "  ";
    out += prefix;
    out += ".incomplete_frames: ";
    out += std::to_string(stats.incomplete_frames);
    out += "\n";

    out += "  ";
    out += prefix;
    out += ".err_packets: ";
    out += std::to_string(stats.err_packets);
    out += "\n";

    out += "  ";
    out += prefix;
    out += ".out_of_order_packets: ";
    out += std::to_string(stats.out_of_order_packets);
    out += "\n";
}

void append_device_port_stats(std::string &out, const char *prefix, const st2110::MtlWorkerDeviceRxPortStats &stats) {
    out += "  ";
    out += prefix;
    out += ".rx_packets: ";
    out += std::to_string(stats.rx_packets);
    out += "\n";

    out += "  ";
    out += prefix;
    out += ".rx_bytes: ";
    out += std::to_string(stats.rx_bytes);
    out += "\n";

    out += "  ";
    out += prefix;
    out += ".rx_err_packets: ";
    out += std::to_string(stats.rx_err_packets);
    out += "\n";

    out += "  ";
    out += prefix;
    out += ".rx_hw_dropped_packets: ";
    out += std::to_string(stats.rx_hw_dropped_packets);
    out += "\n";

    out += "  ";
    out += prefix;
    out += ".rx_nombuf_packets: ";
    out += std::to_string(stats.rx_nombuf_packets);
    out += "\n";
}

[[nodiscard]] std::string format_mtl_worker_stats(const st2110::MtlWorkerStatsEvent &stats) {
    std::string out{};

    out += "MTL graph\n";
    append_counter(out, "graph_id", stats.graph_id);

    out += "\nVideo counters\n";
    append_counter(out, "video_frames_received", stats.video_frames_received);
    append_counter(out, "video_frames_dropped", stats.video_frames_dropped);
    append_counter(out, "video_frames_delivered", stats.video_frames_delivered);
    append_counter(out, "frame_ready_events", stats.frame_ready_events);
    append_counter(out, "video_frame_packets_total", stats.video_frame_packets_total);
    append_counter(out, "video_frame_packets_received_primary", stats.video_frame_packets_received_primary);
    append_counter(out, "video_frame_packets_received_redundant", stats.video_frame_packets_received_redundant);
    append_counter(out, "video_reconstructed_frames", stats.video_reconstructed_frames);
    append_counter(out, "video_corrupted_frames", stats.video_corrupted_frames);
    append_counter(out, "video_complete_frames", stats.video_complete_frames);

    out += "\nVideo session counters\n";
    if (stats.video_session_stats_available) {
        append_counter(out, "video_session_packets_received", stats.video_session_packets_received);
        append_counter(out, "video_session_packets_out_of_order", stats.video_session_packets_out_of_order);
        append_counter(out, "video_session_packets_wrong_ssrc_dropped", stats.video_session_packets_wrong_ssrc_dropped);
        append_counter(out, "video_session_packets_wrong_payload_type_dropped",
                       stats.video_session_packets_wrong_payload_type_dropped);
        append_counter(out, "video_session_bytes_received", stats.video_session_bytes_received);
        append_counter(out, "video_session_frames_dropped", stats.video_session_frames_dropped);
        append_counter(out, "video_session_frames_packets_missed", stats.video_session_frames_packets_missed);
        append_counter(out, "video_session_packets_wrong_length_dropped",
                       stats.video_session_packets_wrong_length_dropped);
        append_counter(out, "video_session_slot_get_frame_failures", stats.video_session_slot_get_frame_failures);

        append_rx_port_stats(out, "video_session_primary", stats.video_session_primary);
        append_rx_port_stats(out, "video_session_redundant", stats.video_session_redundant);
    } else {
        out += "  video_session: unavailable\n";
    }
    append_counter(out, "video_session_stats_query_failures", stats.video_session_stats_query_failures);

    out += "\nAudio counters\n";
    append_counter(out, "audio_blocks_received", stats.audio_blocks_received);
    append_counter(out, "audio_blocks_dropped", stats.audio_blocks_dropped);
    append_counter(out, "audio_blocks_delivered", stats.audio_blocks_delivered);
    append_counter(out, "audio_block_ready_events", stats.audio_block_ready_events);
    append_counter(out, "audio_block_bytes_received", stats.audio_block_bytes_received);
    append_counter(out, "audio_block_packets_total", stats.audio_block_packets_total);
    append_counter(out, "audio_block_packets_received_primary", stats.audio_block_packets_received_primary);
    append_counter(out, "audio_block_packets_received_redundant", stats.audio_block_packets_received_redundant);

    out += "\nAudio session counters\n";
    if (stats.audio_session_stats_available) {
        append_counter(out, "audio_session_packets_received", stats.audio_session_packets_received);
        append_counter(out, "audio_session_packets_out_of_order", stats.audio_session_packets_out_of_order);
        append_counter(out, "audio_session_packets_wrong_ssrc_dropped", stats.audio_session_packets_wrong_ssrc_dropped);
        append_counter(out, "audio_session_packets_wrong_payload_type_dropped",
                       stats.audio_session_packets_wrong_payload_type_dropped);
        append_counter(out, "audio_session_packets_redundant", stats.audio_session_packets_redundant);
        append_counter(out, "audio_session_packets_dropped", stats.audio_session_packets_dropped);
        append_counter(out, "audio_session_packets_length_mismatch_dropped",
                       stats.audio_session_packets_length_mismatch_dropped);
        append_counter(out, "audio_session_slot_get_frame_failures", stats.audio_session_slot_get_frame_failures);

        append_rx_port_stats(out, "audio_session_primary", stats.audio_session_primary);
        append_rx_port_stats(out, "audio_session_redundant", stats.audio_session_redundant);
    } else {
        out += "  audio_session: unavailable\n";
    }
    append_counter(out, "audio_session_stats_query_failures", stats.audio_session_stats_query_failures);

    out += "\nShared-memory delivery counters\n";
    append_counter(out, "released_slots", stats.released_slots);
    append_counter(out, "malformed_ready_events", stats.malformed_ready_events);
    append_counter(out, "stale_ready_events", stats.stale_ready_events);
    append_counter(out, "delivery_failures", stats.delivery_failures);
    append_counter(out, "release_failures", stats.release_failures);
    append_counter(out, "ignored_events", stats.ignored_events);

    out += "\nMTL device port counters\n";
    if (stats.mtl_primary_port_stats_available) {
        append_device_port_stats(out, "mtl_primary_port", stats.mtl_primary_port);
    } else {
        out += "  mtl_primary_port: unavailable\n";
    }

    if (stats.mtl_redundant_port_stats_available) {
        append_device_port_stats(out, "mtl_redundant_port", stats.mtl_redundant_port);
    } else {
        out += "  mtl_redundant_port: unavailable\n";
    }
    append_counter(out, "mtl_port_stats_query_failures", stats.mtl_port_stats_query_failures);

    return out;
}
#endif

} // namespace

class SourceRuntime::Impl {
  public:
    explicit Impl(obs_source_t *source) : source_(source) {}

    ~Impl() { destroy_receive_graph_noexcept(); }

    void update(SourceConfig config) {
        const bool graph_relevant_changed = graph_relevant_config_changed(config, config_);

        config_ = std::move(config);

        if (!graph_relevant_changed) {
            if (receive_requested_ && !configured_graph_exists()) {
                start_receive_graph();
            }

            return;
        }

        destroy_configured_graph_noexcept();

        if (receive_requested_) {
            start_receive_graph();
        } else {
            last_error_.clear();
        }
    }

    void start_receive() {
        receive_requested_ = true;
        start_receive_graph();
    }

    void stop_receive() noexcept {
        receive_requested_ = false;
        stop_active_sessions_noexcept();
    }

    [[nodiscard]] std::uint32_t width() const noexcept { return width_; }

    [[nodiscard]] std::uint32_t height() const noexcept { return height_; }

    [[nodiscard]] bool running() const noexcept { return active_sessions_running(); }

    [[nodiscard]] bool configured() const noexcept { return configured_graph_exists(); }

    [[nodiscard]] const std::string &last_error() const noexcept { return last_error_; }

    [[nodiscard]] std::string debug_status() { return make_debug_status(); }

  private:
    struct ConfiguredReceiveGraph {
        std::unique_ptr<ObsSynchronizedFrameSink> sink{};

        std::shared_ptr<st2110::MtlWorkerGraphClient> mtl_graph_client{};
        std::optional<st2110::MtlRuntimeConfig> mtl_runtime{};

        std::unique_ptr<st2110::IRxBackend> video_backend{};
        std::unique_ptr<st2110::IRxBackend> audio_backend{};

        std::string description{};

        std::uint32_t width = 0;
        std::uint32_t height = 0;
    };

    [[nodiscard]] static bool graph_relevant_config_changed(const SourceConfig &next, const SourceConfig &current) {
        return next.selected_source != current.selected_source || next.receive_settings != current.receive_settings;
    }

    [[nodiscard]] bool configured_graph_exists() const noexcept {
        return static_cast<bool>(sink_) && (static_cast<bool>(video_backend_) || static_cast<bool>(audio_backend_));
    }

    [[nodiscard]] bool active_sessions_running() const noexcept { return active_sessions_running_; }

    [[nodiscard]] bool mtl_graph_client_running() const noexcept {
#if ST2110_HAS_MTL_BACKEND
        return mtl_graph_client_ && mtl_graph_client_->running();
#else
        return false;
#endif
    }

    [[nodiscard]] bool sessions_may_be_running() const noexcept {
        return active_sessions_running_ || mtl_graph_client_running();
    }

    void set_error(const std::string &message) { last_error_ = message; }

    void set_error(const std::string &message, const st2110::Error error) {
        last_error_ = message + ": " + st2110::to_string(error);
    }

    void set_backend_error(const std::string &message, const st2110::IRxBackend *backend, const st2110::Error error) {
        const std::string detail = backend_error_detail(backend);

        if (!detail.empty()) {
            last_error_ = message + ": " + st2110::to_string(error) + "; " + detail;
            return;
        }

        set_error(message, error);
    }

    [[nodiscard]] std::string make_debug_status() {
        std::string out{};

        out += "Receive state: ";
        if (running()) {
            out += "running";
        } else if (configured()) {
            out += "configured/stopped";
        } else if (receive_requested_) {
            out += "requested/unconfigured";
        } else {
            out += "idle";
        }
        out += "\n";

        out += "Configured graph: ";
        out += configured_graph_exists() ? "yes" : "no";
        out += "\n";

        out += "Configured media: ";
        out += configured_graph_description_.empty() ? "unavailable" : configured_graph_description_;
        out += "\n";

        out += "Backend: ";
        out += backend_kind_name(config_.receive_settings.backend_kind);
        out += "\n";

        if (!last_error_.empty()) {
            out += "Last status/error: ";
            out += last_error_;
            out += "\n";
        }

#if ST2110_HAS_MTL_BACKEND
        if (config_.receive_settings.backend_kind == st2110::ReceiveBackendKind::Mtl) {
            out += "\n";
            out += make_mtl_debug_status();
            return out;
        }
#endif

        out += "\n";
        out += make_socket_debug_status();

        return out;
    }

#if ST2110_HAS_MTL_BACKEND
    [[nodiscard]] std::string make_mtl_debug_status() {
        std::string out{};

        if (!mtl_graph_client_) {
            out += "MTL graph client: unavailable\n";
            out += "MTL counters: unavailable because MTL graph is not configured.\n";
            return out;
        }

        out += "MTL graph client: available\n";
        out += "MTL graph id: ";
        out += std::to_string(mtl_graph_client_->graph_id());
        out += "\n";

        out += "MTL graph running: ";
        out += mtl_graph_client_->running() ? "yes" : "no";
        out += "\n";

        if (!configured_graph_exists()) {
            out += "MTL counters: unavailable because receive graph is not configured.\n";
            return out;
        }

        if (!mtl_graph_client_->running()) {
            out += "MTL counters: unavailable because receive graph is configured but stopped.\n";
            return out;
        }

        auto stats = mtl_graph_client_->stats();
        if (!stats.has_value()) {
            const std::string detail = mtl_graph_client_->last_error_message();

            last_error_ = "MTL worker stats query failed: ";
            last_error_ += st2110::to_string(stats.error());

            if (!detail.empty()) {
                last_error_ += "; ";
                last_error_ += detail;
            }

            out += "MTL counters: unavailable because stats query failed.\n";
            out += last_error_;
            out += "\n";

            /*
             * stats() failure may invalidate the worker and clear the graph-client
             * lease. Destroy the configured graph so proxy started_ state cannot
             * remain stale after worker invalidation.
             */
            destroy_configured_graph_noexcept();

            return out;
        }

        out += "\n";
        out += format_mtl_worker_stats(*stats);

        return out;
    }
#endif

    [[nodiscard]] std::string make_socket_debug_status() const {
        std::string out{};

        out += "Socket backend diagnostics\n";

        append_backend_stats(out, "Socket video backend", video_backend_.get());
        out += "\n";
        append_backend_stats(out, "Socket audio backend", audio_backend_.get());

        out += "\nMTL counters: unavailable because current backend is not MTL.\n";

        return out;
    }

    void start_receive_graph() {
        if (!ensure_configured_graph()) {
            return;
        }

        if (!start_active_sessions()) {
            destroy_configured_graph_noexcept();
        }
    }

    [[nodiscard]] bool ensure_configured_graph() {
        if (configured_graph_exists()) {
            return true;
        }

        auto staged_graph = build_configured_graph(config_);
        if (!staged_graph.has_value()) {
            return false;
        }

        commit_configured_graph(std::move(*staged_graph));
        return true;
    }

    [[nodiscard]] std::optional<ConfiguredReceiveGraph> build_configured_graph(const SourceConfig &config) {
        last_error_.clear();

        if (!has_provider_selected_sdp(config)) {
            set_error("Cannot build receive graph: no selected source with SDP is available");
            return std::nullopt;
        }

        auto media_set = resolve_selected_source_media_set(*config.selected_source);
        if (!media_set.has_value()) {
            set_error("Selected provider SDP media selection failed", media_set.error());
            return std::nullopt;
        }

        auto parsed_streams = parse_selected_source_streams(*media_set);
        if (!parsed_streams.has_value()) {
            set_error("Selected provider SDP parser dispatch failed", parsed_streams.error());
            return std::nullopt;
        }

        if (parsed_streams->empty()) {
            set_error("Selected provider SDP parser dispatch produced an empty stream set");
            return std::nullopt;
        }

        std::optional<st2110::VideoReceiveBootstrap> video_bootstrap{};
        std::optional<st2110::AudioReceiveBootstrap> audio_bootstrap{};

        std::optional<st2110::ReceiveStartRequest> video_request{};
        std::optional<st2110::ReceiveStartRequest> audio_request{};

        std::unique_ptr<st2110::IRxBackend> staged_video_backend{};
        std::unique_ptr<st2110::IRxBackend> staged_audio_backend{};

        std::shared_ptr<st2110::MtlWorkerGraphClient> mtl_graph_client{};

#if ST2110_HAS_MTL_BACKEND
        if (config.receive_settings.backend_kind == st2110::ReceiveBackendKind::Mtl) {
            mtl_graph_client = std::make_shared<st2110::MtlWorkerGraphClient>();
        }
#endif

        try {
            if (parsed_streams->has_video()) {
                video_bootstrap = st2110::project_parsed_video_sdp_to_receive_bootstrap(*parsed_streams->video);

                auto local = st2110::auto_select_receive_local_policy(video_bootstrap->receive_bootstrap);
                if (!local.has_value()) {
                    set_error("Video receive local policy selection failed", local.error());
                    return std::nullopt;
                }

                video_request = st2110::ReceiveStartRequest{
                    .media = *video_bootstrap,
                    .local = std::move(*local),
                };

                auto backend = make_video_backend(*video_request, config.receive_settings, mtl_graph_client);
                if (!backend.has_value()) {
                    set_error("Video receive backend construction failed", backend.error());
                    return std::nullopt;
                }

                staged_video_backend = std::move(*backend);
            }

            if (parsed_streams->has_audio()) {
                audio_bootstrap = st2110::project_parsed_audio_sdp_to_receive_bootstrap(*parsed_streams->audio);

                auto local = st2110::auto_select_receive_local_policy(audio_bootstrap->receive_bootstrap);
                if (!local.has_value()) {
                    set_error("Audio receive local policy selection failed", local.error());
                    return std::nullopt;
                }

                audio_request = st2110::ReceiveStartRequest{
                    .media = *audio_bootstrap,
                    .local = std::move(*local),
                };

                auto backend = make_audio_backend(*audio_request, config.receive_settings, mtl_graph_client);
                if (!backend.has_value()) {
                    set_error("Audio receive backend construction failed", backend.error());
                    return std::nullopt;
                }

                staged_audio_backend = std::move(*backend);
            }
        } catch (const std::exception &ex) {
            set_error(std::string("Receive graph construction failed: ") + ex.what());
            return std::nullopt;
        } catch (...) {
            set_error("Receive graph construction failed with an unknown exception");
            return std::nullopt;
        }

        std::optional<st2110::MtlRuntimeConfig> staged_mtl_runtime{};

#if ST2110_HAS_MTL_BACKEND
        if (mtl_graph_client) {
            auto runtime_key = resolve_configured_mtl_runtime_key(mtl_graph_client);
            if (!runtime_key.has_value()) {
                set_error("MTL receive graph runtime-key resolution failed", runtime_key.error());
                return std::nullopt;
            }

            staged_mtl_runtime = std::move(*runtime_key);
        }
#endif

        auto staged_sink =
            std::make_unique<ObsSynchronizedFrameSink>(source_, make_sink_config(video_bootstrap, audio_bootstrap));

        return ConfiguredReceiveGraph{
            .sink = std::move(staged_sink),
            .mtl_graph_client = std::move(mtl_graph_client),
            .mtl_runtime = std::move(staged_mtl_runtime),
            .video_backend = std::move(staged_video_backend),
            .audio_backend = std::move(staged_audio_backend),
            .description = describe_parsed_stream_composition(*parsed_streams),
            .width = video_bootstrap.has_value() ? video_bootstrap->stream.media.width : 0,
            .height = video_bootstrap.has_value() ? video_bootstrap->stream.media.height : 0,
        };
    }

    void commit_configured_graph(ConfiguredReceiveGraph graph) {
        active_sessions_running_ = false;

        sink_ = std::move(graph.sink);
        mtl_graph_client_ = std::move(graph.mtl_graph_client);
        configured_mtl_runtime_ = std::move(graph.mtl_runtime);
        video_backend_ = std::move(graph.video_backend);
        audio_backend_ = std::move(graph.audio_backend);

        configured_graph_description_ = std::move(graph.description);
        width_ = graph.width;
        height_ = graph.height;
    }

    [[nodiscard]] bool start_active_sessions() {
        if (active_sessions_running()) {
            return true;
        }

        if (!sink_) {
            set_error("Cannot start receive sessions without a configured sink");
            return false;
        }

        if (!video_backend_ && !audio_backend_) {
            set_error("Cannot start receive sessions without configured media backends");
            return false;
        }

        active_sessions_running_ = false;

        if (sink_) {
            sink_->start();
        }

        const auto cleanup_started_sessions = [this]() noexcept {
            active_sessions_running_ = false;

            if (video_backend_) {
                (void)video_backend_->stop();
            }

            if (audio_backend_) {
                (void)audio_backend_->stop();
            }

            if (sink_) {
                sink_->stop();
            }
        };

        if (video_backend_) {
            auto started = video_backend_->start(sink_.get());
            if (!started.has_value()) {
                cleanup_started_sessions();
                set_backend_error("Video receive backend start failed", video_backend_.get(), started.error());
                return false;
            }

            if (!*started) {
                cleanup_started_sessions();
                set_error("Video receive backend start returned false");
                return false;
            }
        }

        if (audio_backend_) {
            auto started = audio_backend_->start(sink_.get());
            if (!started.has_value()) {
                cleanup_started_sessions();
                set_backend_error("Audio receive backend start failed", audio_backend_.get(), started.error());
                return false;
            }

            if (!*started) {
                cleanup_started_sessions();
                set_error("Audio receive backend start returned false");
                return false;
            }
        }

        active_sessions_running_ = true;
        last_error_ = std::string("Receive graph started for ") + configured_graph_description_ + ".";

        return true;
    }

    void stop_active_sessions_noexcept() noexcept {
        if (!sessions_may_be_running()) {
            return;
        }

        active_sessions_running_ = false;

        if (video_backend_) {
            (void)video_backend_->stop();
        }

        if (audio_backend_) {
            (void)audio_backend_->stop();
        }

        if (sink_) {
            sink_->stop();
        }
    }

    void destroy_configured_graph_noexcept() noexcept {
        stop_active_sessions_noexcept();

        video_backend_.reset();
        audio_backend_.reset();
        mtl_graph_client_.reset();
        configured_mtl_runtime_.reset();
        sink_.reset();

        width_ = 0;
        height_ = 0;
        configured_graph_description_.clear();
        active_sessions_running_ = false;
    }

    void destroy_receive_graph_noexcept() noexcept { destroy_configured_graph_noexcept(); }

    obs_source_t *source_ = nullptr;

    SourceConfig config_{};
    bool receive_requested_ = false;

    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;

    std::string last_error_{};
    std::string configured_graph_description_{};

    std::unique_ptr<ObsSynchronizedFrameSink> sink_{};

    std::shared_ptr<st2110::MtlWorkerGraphClient> mtl_graph_client_{};
    std::optional<st2110::MtlRuntimeConfig> configured_mtl_runtime_{};

    std::unique_ptr<st2110::IRxBackend> video_backend_{};
    std::unique_ptr<st2110::IRxBackend> audio_backend_{};
    bool active_sessions_running_ = false;
};

SourceRuntime::SourceRuntime(obs_source_t *source) : impl_(std::make_unique<Impl>(source)) {}

SourceRuntime::~SourceRuntime() = default;

void SourceRuntime::update(const SourceConfig &config) { impl_->update(config); }

void SourceRuntime::start_receive() { impl_->start_receive(); }

void SourceRuntime::stop_receive() noexcept { impl_->stop_receive(); }

std::uint32_t SourceRuntime::width() const noexcept { return impl_->width(); }

std::uint32_t SourceRuntime::height() const noexcept { return impl_->height(); }

bool SourceRuntime::running() const noexcept { return impl_->running(); }

bool SourceRuntime::configured() const noexcept { return impl_->configured(); }

const std::string &SourceRuntime::last_error() const noexcept { return impl_->last_error(); }

std::string SourceRuntime::debug_status() { return impl_->debug_status(); }

} // namespace obs_st2110
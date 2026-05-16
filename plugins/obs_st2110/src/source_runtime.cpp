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
#include <st2110/delivery/video/socket_video_start_config.hpp>
#include <st2110/foundation/error.hpp>
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
                 const std::optional<st2110::AudioReceiveBootstrap> &audio_bootstrap,
                 const st2110::TimestampNs playout_delay_ns) {
    st2110::SynchronizedFrameSinkConfig cfg{};
    cfg.enable_video = video_bootstrap.has_value();
    cfg.enable_audio = audio_bootstrap.has_value();
    cfg.playout_delay_ns = playout_delay_ns;

    if (video_bootstrap.has_value()) {
        cfg.video_timestamp_mapper.rtp_clock_rate =
            video_bootstrap->stream.receive_signaled_stream.timing.rtp_clock_rate;
    }

    if (audio_bootstrap.has_value()) {
        cfg.audio_timestamp_mapper.rtp_clock_rate =
            audio_bootstrap->stream.receive_signaled_stream.timing.rtp_clock_rate;
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

} // namespace

class SourceRuntime::Impl {
  public:
    explicit Impl(obs_source_t *source) : source_(source) {}

    ~Impl() { destroy_receive_graph_noexcept(); }

    void update(SourceConfig config) {
        const bool graph_relevant_changed = graph_relevant_config_changed(config, config_);

        if (!graph_relevant_changed) {
            config_ = std::move(config);
            return;
        }

        if (!receive_requested_) {
            destroy_receive_graph_noexcept();
            config_ = std::move(config);
            return;
        }

        auto staged_graph = build_configured_graph(config);
        if (!staged_graph.has_value()) {
            destroy_receive_graph_noexcept();
            config_ = std::move(config);
            return;
        }

        destroy_configured_graph_noexcept();

        config_ = std::move(config);
        commit_configured_graph(std::move(*staged_graph));

        if (!start_active_sessions()) {
            destroy_configured_graph_noexcept();
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
        return next.selected_source != current.selected_source || next.receive_settings != current.receive_settings ||
               next.playout_delay_ns != current.playout_delay_ns;
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

        auto staged_sink = std::make_unique<ObsSynchronizedFrameSink>(
            source_, make_sink_config(video_bootstrap, audio_bootstrap, config.playout_delay_ns));

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

} // namespace obs_st2110
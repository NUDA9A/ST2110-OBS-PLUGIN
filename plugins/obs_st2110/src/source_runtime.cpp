#include <obs_st2110/source_runtime.hpp>

#include "obs-synchronized-frame-sink.hpp"

#include <obs_st2110/sdp_media_selection.hpp>
#include <obs_st2110/sdp_parser_dispatch.hpp>

#include <st2110/backends/receive_local_policy.hpp>
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
#include <st2110/backends/mtl/mtl_rx_audio_backend.hpp>
#include <st2110/backends/mtl/mtl_rx_video_backend.hpp>
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
make_video_backend(const st2110::ReceiveStartRequest &request, const st2110::Settings &settings) {
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

        return std::make_unique<st2110::MtlRxVideoBackend>(std::move(*mtl_cfg));
#else
        return std::unexpected(st2110::Error::Unsupported);
#endif
    }
    }

    return std::unexpected(st2110::Error::Unsupported);
}

[[nodiscard]] std::expected<std::unique_ptr<st2110::IRxBackend>, st2110::Error>
make_audio_backend(const st2110::ReceiveStartRequest &request, const st2110::Settings &settings) {
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

        return std::make_unique<st2110::MtlRxAudioBackend>(std::move(*mtl_cfg));
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

} // namespace

class SourceRuntime::Impl {
  public:
    explicit Impl(obs_source_t *source) : source_(source) {}

    ~Impl() { stop(); }

    void update(SourceConfig config) {
        const bool graph_relevant_config_changed = config != config_;

        if (graph_relevant_config_changed && receive_graph_running()) {
            stop_receive_graph_noexcept();
        }

        config_ = std::move(config);

        if (lifecycle_active_ && config_.start_when_active && graph_relevant_config_changed) {
            start_receive_graph();
        }
    }

    void start() {
        lifecycle_active_ = true;

        if (!config_.start_when_active) {
            return;
        }

        start_receive_graph();
    }

    void stop() noexcept {
        lifecycle_active_ = false;
        stop_receive_graph_noexcept();
    }

    [[nodiscard]] std::uint32_t width() const noexcept { return width_; }

    [[nodiscard]] std::uint32_t height() const noexcept { return height_; }

    [[nodiscard]] bool running() const noexcept { return receive_graph_running(); }

    [[nodiscard]] const std::string &last_error() const noexcept { return last_error_; }

  private:
    [[nodiscard]] bool receive_graph_running() const noexcept {
        return static_cast<bool>(sink_) || static_cast<bool>(video_backend_) || static_cast<bool>(audio_backend_);
    }

    void set_error(const std::string &message) { last_error_ = message; }

    void set_error(const std::string &message, const st2110::Error error) {
        last_error_ = message + ": " + st2110::to_string(error);
    }

    void start_receive_graph() {
        if (receive_graph_running()) {
            return;
        }

        last_error_.clear();

        if (!has_provider_selected_sdp(config_)) {
            /*
             * Correct idle state:
             *
             * The source is active, but no provider-selected sender exists yet.
             * Do not create fake SDP, fake sink, or fake backends.
             */
            return;
        }

        auto media_set = resolve_selected_source_media_set(*config_.selected_source);
        if (!media_set.has_value()) {
            set_error("Selected provider SDP media selection failed", media_set.error());
            return;
        }

        auto parsed_streams = parse_selected_source_streams(*media_set);
        if (!parsed_streams.has_value()) {
            set_error("Selected provider SDP parser dispatch failed", parsed_streams.error());
            return;
        }

        if (parsed_streams->empty()) {
            set_error("Selected provider SDP parser dispatch produced an empty stream set");
            return;
        }

        std::optional<st2110::VideoReceiveBootstrap> video_bootstrap{};
        std::optional<st2110::AudioReceiveBootstrap> audio_bootstrap{};

        std::optional<st2110::ReceiveStartRequest> video_request{};
        std::optional<st2110::ReceiveStartRequest> audio_request{};

        std::unique_ptr<st2110::IRxBackend> staged_video_backend{};
        std::unique_ptr<st2110::IRxBackend> staged_audio_backend{};

        try {
            if (parsed_streams->has_video()) {
                video_bootstrap = st2110::project_parsed_video_sdp_to_receive_bootstrap(*parsed_streams->video);

                auto local = st2110::auto_select_receive_local_policy(video_bootstrap->receive_bootstrap);
                if (!local.has_value()) {
                    set_error("Video receive local policy selection failed", local.error());
                    return;
                }

                video_request = st2110::ReceiveStartRequest{
                    .media = *video_bootstrap,
                    .local = std::move(*local),
                };

                auto backend = make_video_backend(*video_request, config_.receive_settings);
                if (!backend.has_value()) {
                    set_error("Video receive backend construction failed", backend.error());
                    return;
                }

                staged_video_backend = std::move(*backend);
            }

            if (parsed_streams->has_audio()) {
                audio_bootstrap = st2110::project_parsed_audio_sdp_to_receive_bootstrap(*parsed_streams->audio);

                auto local = st2110::auto_select_receive_local_policy(audio_bootstrap->receive_bootstrap);
                if (!local.has_value()) {
                    set_error("Audio receive local policy selection failed", local.error());
                    return;
                }

                audio_request = st2110::ReceiveStartRequest{
                    .media = *audio_bootstrap,
                    .local = std::move(*local),
                };

                auto backend = make_audio_backend(*audio_request, config_.receive_settings);
                if (!backend.has_value()) {
                    set_error("Audio receive backend construction failed", backend.error());
                    return;
                }

                staged_audio_backend = std::move(*backend);
            }
        } catch (const std::exception &ex) {
            set_error(std::string("Receive graph construction failed: ") + ex.what());
            return;
        } catch (...) {
            set_error("Receive graph construction failed with an unknown exception");
            return;
        }

        auto staged_sink = std::make_unique<ObsSynchronizedFrameSink>(
            source_, make_sink_config(video_bootstrap, audio_bootstrap, config_.playout_delay_ns));

        staged_sink->start();

        const auto cleanup_staged_graph = [&staged_video_backend, &staged_audio_backend, &staged_sink]() noexcept {
            if (staged_video_backend) {
                (void)staged_video_backend->stop();
            }

            if (staged_audio_backend) {
                (void)staged_audio_backend->stop();
            }

            if (staged_sink) {
                staged_sink->stop();
            }
        };

        if (staged_video_backend) {
            auto started = staged_video_backend->start(staged_sink.get());
            if (!started.has_value()) {
                cleanup_staged_graph();
                set_error("Video receive backend start failed", started.error());
                return;
            }

            if (!*started) {
                cleanup_staged_graph();
                set_error("Video receive backend start returned false");
                return;
            }
        }

        if (staged_audio_backend) {
            auto started = staged_audio_backend->start(staged_sink.get());
            if (!started.has_value()) {
                cleanup_staged_graph();
                set_error("Audio receive backend start failed", started.error());
                return;
            }

            if (!*started) {
                cleanup_staged_graph();
                set_error("Audio receive backend start returned false");
                return;
            }
        }

        width_ = video_bootstrap.has_value() ? video_bootstrap->stream.media.width : 0;
        height_ = video_bootstrap.has_value() ? video_bootstrap->stream.media.height : 0;

        sink_ = std::move(staged_sink);
        video_backend_ = std::move(staged_video_backend);
        audio_backend_ = std::move(staged_audio_backend);

        last_error_ = std::string("Receive graph started for ") + describe_parsed_stream_composition(*parsed_streams) +
                      ".";
    }

    void stop_receive_graph_noexcept() noexcept {
        /*
         * Stop order is part of the ownership contract:
         *
         * 1. backends stop first;
         * 2. sink stops after callbacks can no longer be emitted.
         */
        if (video_backend_) {
            (void)video_backend_->stop();
            video_backend_.reset();
        }

        if (audio_backend_) {
            (void)audio_backend_->stop();
            audio_backend_.reset();
        }

        if (sink_) {
            sink_->stop();
            sink_.reset();
        }

        width_ = 0;
        height_ = 0;
    }

    obs_source_t *source_ = nullptr;

    SourceConfig config_{};
    bool lifecycle_active_ = false;

    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;

    std::string last_error_{};

    std::unique_ptr<ObsSynchronizedFrameSink> sink_{};
    std::unique_ptr<st2110::IRxBackend> video_backend_{};
    std::unique_ptr<st2110::IRxBackend> audio_backend_{};
};

SourceRuntime::SourceRuntime(obs_source_t *source) : impl_(std::make_unique<Impl>(source)) {}

SourceRuntime::~SourceRuntime() = default;

void SourceRuntime::update(const SourceConfig &config) {
    impl_->update(config);
}

void SourceRuntime::start() {
    impl_->start();
}

void SourceRuntime::stop() noexcept {
    impl_->stop();
}

std::uint32_t SourceRuntime::width() const noexcept {
    return impl_->width();
}

std::uint32_t SourceRuntime::height() const noexcept {
    return impl_->height();
}

bool SourceRuntime::running() const noexcept {
    return impl_->running();
}

const std::string &SourceRuntime::last_error() const noexcept {
    return impl_->last_error();
}

} // namespace obs_st2110
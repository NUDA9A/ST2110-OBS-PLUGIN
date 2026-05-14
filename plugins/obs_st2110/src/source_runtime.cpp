#include <obs_st2110/source_runtime.hpp>

#include "obs-synchronized-frame-sink.hpp"

#include <obs_st2110/sdp_media_selection.hpp>
#include <obs_st2110/sdp_parser_dispatch.hpp>

#include <st2110/contracts/backend/backend.hpp>
#include <st2110/delivery/synchronized_frame_sink.hpp>
#include <st2110/foundation/error.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

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
            last_error_ = std::string("Selected provider SDP media selection failed: ") +
                          st2110::to_string(media_set.error());
            return;
        }

        auto parsed_streams = parse_selected_source_streams(*media_set);
        if (!parsed_streams.has_value()) {
            last_error_ = std::string("Selected provider SDP parser dispatch failed: ") +
                          st2110::to_string(parsed_streams.error());
            return;
        }

        /*
         * The next step starts from this typed parser result:
         *
         *   parsed_streams.video -> project_parsed_video_sdp_to_receive_bootstrap(...)
         *   parsed_streams.audio -> project_parsed_audio_sdp_to_receive_bootstrap(...)
         *
         * Then:
         *   ReceiveBootstrap + local policy -> ReceiveStartRequest
         *   ReceiveStartRequest + Settings -> backend start config
         *   one shared ObsSynchronizedFrameSink -> video/audio backend start
         */
        last_error_ =
            std::string("Receive graph construction after media-specific SDP parser dispatch is not implemented yet for ") +
            describe_parsed_stream_composition(*parsed_streams) + ".";
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
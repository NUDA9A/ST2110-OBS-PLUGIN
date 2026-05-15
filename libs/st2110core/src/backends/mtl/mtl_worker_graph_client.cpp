#include <st2110/backends/mtl/mtl_worker_graph_client.hpp>

#include <atomic>
#include <type_traits>
#include <utility>
#include <variant>

namespace st2110 {
namespace {

[[nodiscard]] MtlWorkerGraphId next_graph_id() noexcept {
    static std::atomic<MtlWorkerGraphId> next{1};
    return next.fetch_add(1, std::memory_order_relaxed);
}

[[nodiscard]] MtlWorkerRequestId next_request_id() noexcept {
    static std::atomic<MtlWorkerRequestId> next{1};
    return next.fetch_add(1, std::memory_order_relaxed);
}

[[nodiscard]] std::expected<MtlRuntimeConfig, Error>
resolve_graph_runtime_config(const std::optional<MtlVideoStartConfig> &video,
                             const std::optional<MtlAudioStartConfig> &audio) {
    if (!video.has_value() && !audio.has_value()) {
        return std::unexpected(Error::InvalidValue);
    }

    if (video.has_value()) {
        MtlRuntimeConfig runtime = video->runtime;

        if (audio.has_value() && audio->runtime != runtime) {
            return std::unexpected(Error::InvalidValue);
        }

        return runtime;
    }

    return audio->runtime;
}

[[nodiscard]] std::expected<bool, Error> interpret_start_sessions_event(const MtlWorkerControlEvent &event,
                                                                        const MtlWorkerGraphId expected_graph_id) {
    return std::visit(
        [expected_graph_id](const auto &typed_event) -> std::expected<bool, Error> {
            using Event = std::decay_t<decltype(typed_event)>;

            if constexpr (std::is_same_v<Event, MtlWorkerStartedEvent>) {
                if (typed_event.graph_id != expected_graph_id) {
                    return std::unexpected(Error::InvalidBackendState);
                }

                return true;
            } else if constexpr (std::is_same_v<Event, MtlWorkerErrorEvent>) {
                return std::unexpected(typed_event.error);
            } else {
                return std::unexpected(Error::InvalidBackendState);
            }
        },
        event);
}

[[nodiscard]] std::expected<bool, Error> interpret_stop_sessions_event(const MtlWorkerControlEvent &event,
                                                                       const MtlWorkerGraphId expected_graph_id) {
    return std::visit(
        [expected_graph_id](const auto &typed_event) -> std::expected<bool, Error> {
            using Event = std::decay_t<decltype(typed_event)>;

            if constexpr (std::is_same_v<Event, MtlWorkerStoppedEvent>) {
                if (typed_event.graph_id != expected_graph_id) {
                    return std::unexpected(Error::InvalidBackendState);
                }

                return true;
            } else if constexpr (std::is_same_v<Event, MtlWorkerErrorEvent>) {
                return std::unexpected(typed_event.error);
            } else {
                return std::unexpected(Error::InvalidBackendState);
            }
        },
        event);
}

} // namespace

struct MtlWorkerGraphClient::Impl {
    MtlWorkerGraphId graph_id = next_graph_id();

    std::optional<MtlVideoStartConfig> video{};
    std::optional<MtlAudioStartConfig> audio{};

    IFrameSink *sink = nullptr;

    std::optional<MtlWorkerManager::WorkerLease> worker_lease{};
    bool running = false;
};

MtlWorkerGraphClient::MtlWorkerGraphClient() : impl_(std::make_unique<Impl>()) {}

MtlWorkerGraphClient::~MtlWorkerGraphClient() { stop_noexcept(); }

std::expected<bool, Error> MtlWorkerGraphClient::configure_video(MtlVideoStartConfig cfg) {
    if (impl_->running) {
        return std::unexpected(Error::InvalidBackendState);
    }

    if (impl_->audio.has_value() && impl_->audio->runtime != cfg.runtime) {
        return std::unexpected(Error::InvalidValue);
    }

    impl_->video = std::move(cfg);
    return true;
}

std::expected<bool, Error> MtlWorkerGraphClient::configure_audio(MtlAudioStartConfig cfg) {
    if (impl_->running) {
        return std::unexpected(Error::InvalidBackendState);
    }

    if (impl_->video.has_value() && impl_->video->runtime != cfg.runtime) {
        return std::unexpected(Error::InvalidValue);
    }

    impl_->audio = std::move(cfg);
    return true;
}

std::expected<bool, Error> MtlWorkerGraphClient::attach_sink(IFrameSink *sink) {
    if (!sink) {
        return std::unexpected(Error::InvalidValue);
    }

    if (impl_->sink && impl_->sink != sink) {
        return std::unexpected(Error::InvalidBackendState);
    }

    impl_->sink = sink;
    return true;
}

void MtlWorkerGraphClient::detach_sink_noexcept(IFrameSink *sink) noexcept {
    if (!sink || impl_->sink == sink) {
        impl_->sink = nullptr;
    }
}

std::expected<bool, Error> MtlWorkerGraphClient::start() {
    if (impl_->running) {
        return true;
    }

    if (!impl_->sink) {
        return std::unexpected(Error::InvalidBackendState);
    }

    auto runtime = resolve_graph_runtime_config(impl_->video, impl_->audio);
    if (!runtime.has_value()) {
        return std::unexpected(runtime.error());
    }

    auto lease = default_mtl_worker_manager().acquire_or_spawn_compatible_worker(*runtime);
    if (!lease.has_value()) {
        return std::unexpected(lease.error());
    }

    impl_->worker_lease = *lease;

    if (!impl_->worker_lease->control_channel) {
        return std::unexpected(Error::InvalidBackendState);
    }

    auto request = make_start_sessions_request();
    if (!request.has_value()) {
        return std::unexpected(request.error());
    }

    auto event = impl_->worker_lease->control_channel->transact(MtlWorkerControlRequest{*request});
    if (!event.has_value()) {
        return std::unexpected(event.error());
    }

    auto started = interpret_start_sessions_event(*event, impl_->graph_id);
    if (!started.has_value()) {
        return std::unexpected(started.error());
    }

    impl_->running = true;
    return true;
}

std::expected<bool, Error> MtlWorkerGraphClient::stop() {
    if (!impl_->running) {
        impl_->worker_lease.reset();
        return true;
    }

    if (!impl_->worker_lease.has_value() || !impl_->worker_lease->control_channel) {
        impl_->running = false;
        impl_->worker_lease.reset();
        return std::unexpected(Error::InvalidBackendState);
    }

    auto request = make_stop_sessions_request();

    auto event = impl_->worker_lease->control_channel->transact(MtlWorkerControlRequest{request});
    if (!event.has_value()) {
        impl_->running = false;
        impl_->worker_lease.reset();
        return std::unexpected(event.error());
    }

    auto stopped = interpret_stop_sessions_event(*event, impl_->graph_id);
    if (!stopped.has_value()) {
        impl_->running = false;
        impl_->worker_lease.reset();
        return std::unexpected(stopped.error());
    }

    impl_->running = false;
    impl_->worker_lease.reset();

    return true;
}

void MtlWorkerGraphClient::stop_noexcept() noexcept { (void)stop(); }

std::expected<MtlWorkerStartSessionsRequest, Error> MtlWorkerGraphClient::make_start_sessions_request() const {
    if (!configured()) {
        return std::unexpected(Error::InvalidBackendState);
    }

    auto runtime = resolve_graph_runtime_config(impl_->video, impl_->audio);
    if (!runtime.has_value()) {
        return std::unexpected(runtime.error());
    }

    return MtlWorkerStartSessionsRequest{
        .request_id = next_request_id(),
        .graph_id = impl_->graph_id,
        .video = impl_->video,
        .audio = impl_->audio,
    };
}

MtlWorkerStopSessionsRequest MtlWorkerGraphClient::make_stop_sessions_request() const {
    return MtlWorkerStopSessionsRequest{
        .request_id = next_request_id(),
        .graph_id = impl_->graph_id,
    };
}

bool MtlWorkerGraphClient::configured() const noexcept { return impl_->video.has_value() || impl_->audio.has_value(); }

bool MtlWorkerGraphClient::running() const noexcept { return impl_->running; }

MtlWorkerGraphId MtlWorkerGraphClient::graph_id() const noexcept { return impl_->graph_id; }

const std::optional<MtlVideoStartConfig> &MtlWorkerGraphClient::video_config() const noexcept { return impl_->video; }

const std::optional<MtlAudioStartConfig> &MtlWorkerGraphClient::audio_config() const noexcept { return impl_->audio; }

IFrameSink *MtlWorkerGraphClient::sink() const noexcept { return impl_->sink; }

} // namespace st2110
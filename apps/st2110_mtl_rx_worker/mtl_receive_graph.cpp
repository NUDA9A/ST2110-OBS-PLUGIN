#include "mtl_receive_graph.hpp"

#include <utility>

namespace st2110_mtl_rx_worker {
namespace {

[[nodiscard]] std::expected<st2110::MtlRuntimeConfig, st2110::Error>
resolve_graph_runtime_config(const MtlReceiveGraphConfig &cfg) {
    if (!cfg.video.has_value() && !cfg.audio.has_value()) {
        return std::unexpected(st2110::Error::InvalidValue);
    }

    if (cfg.video.has_value()) {
        st2110::MtlRuntimeConfig runtime = cfg.video->runtime;

        if (cfg.audio.has_value() && cfg.audio->runtime != runtime) {
            return std::unexpected(st2110::Error::InvalidValue);
        }

        return runtime;
    }

    return cfg.audio->runtime;
}

} // namespace

struct MtlReceiveGraph::Impl {
    /*
     * Declaration order matters:
     *
     * Destruction happens in reverse member order, so audio/video sessions are
     * destroyed before this graph object finishes destruction.
     *
     * The MTL runtime is worker-owned and intentionally not owned here.
     */
    MtlReceiveGraphConfig cfg{};
    MtlRuntimeContext *runtime = nullptr;
    std::unique_ptr<MtlVideoRxSession> video{};
    std::unique_ptr<MtlAudioRxSession> audio{};

    Impl(MtlRuntimeContext &runtime_context, MtlReceiveGraphConfig graph_cfg)
        : cfg(std::move(graph_cfg)), runtime(&runtime_context) {}
};

std::expected<std::unique_ptr<MtlReceiveGraph>, st2110::Error> MtlReceiveGraph::create(MtlRuntimeContext &runtime,
                                                                                       MtlReceiveGraphConfig cfg) {
    auto runtime_cfg = resolve_graph_runtime_config(cfg);
    if (!runtime_cfg.has_value()) {
        return std::unexpected(runtime_cfg.error());
    }

    if (runtime.config() != *runtime_cfg) {
        return std::unexpected(st2110::Error::InvalidValue);
    }

    auto impl = std::make_unique<Impl>(runtime, std::move(cfg));
    auto graph = std::unique_ptr<MtlReceiveGraph>(new MtlReceiveGraph(std::move(impl)));

    auto started = graph->start_sessions();
    if (!started.has_value()) {
        return std::unexpected(started.error());
    }

    return graph;
}

MtlReceiveGraph::MtlReceiveGraph(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

MtlReceiveGraph::~MtlReceiveGraph() = default;

std::expected<bool, st2110::Error> MtlReceiveGraph::start_sessions() {
    if (!impl_->runtime || !impl_->runtime->handle()) {
        return std::unexpected(st2110::Error::InvalidBackendState);
    }

    if (sessions_running()) {
        return true;
    }

    std::unique_ptr<MtlVideoRxSession> staged_video{};
    std::unique_ptr<MtlAudioRxSession> staged_audio{};

    if (impl_->cfg.video.has_value()) {
        auto video_session = MtlVideoRxSession::create(*impl_->runtime, *impl_->cfg.video);
        if (!video_session.has_value()) {
            return std::unexpected(video_session.error());
        }

        staged_video = std::move(*video_session);
    }

    if (impl_->cfg.audio.has_value()) {
        auto audio_session = MtlAudioRxSession::create(*impl_->runtime, *impl_->cfg.audio);
        if (!audio_session.has_value()) {
            return std::unexpected(audio_session.error());
        }

        staged_audio = std::move(*audio_session);
    }

    impl_->video = std::move(staged_video);
    impl_->audio = std::move(staged_audio);

    return true;
}

void MtlReceiveGraph::stop_sessions_noexcept() noexcept {
    wake_block();

    impl_->audio.reset();
    impl_->video.reset();
}

void MtlReceiveGraph::wake_block() noexcept {
    if (impl_->video) {
        impl_->video->wake_block();
    }

    if (impl_->audio) {
        impl_->audio->wake_block();
    }
}

bool MtlReceiveGraph::sessions_running() const noexcept {
    return static_cast<bool>(impl_->video) || static_cast<bool>(impl_->audio);
}

const MtlReceiveGraphConfig &MtlReceiveGraph::config() const noexcept { return impl_->cfg; }

} // namespace st2110_mtl_rx_worker
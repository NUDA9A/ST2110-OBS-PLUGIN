#include "mtl_receive_graph.hpp"

#include "mtl_worker_event_writer.hpp"

#include <st2110/backends/mtl/mtl_worker_shared_memory_ring.hpp>

#include <span>
#include <utility>
#include <vector>

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

[[nodiscard]] std::expected<std::vector<st2110::MtlWorkerSharedMemoryRingMap>, st2110::Error>
import_shared_memory_rings(const MtlReceiveGraphConfig &cfg, std::span<const int> ancillary_file_descriptors) {
    std::vector<st2110::MtlWorkerSharedMemoryRingMap> rings{};

    if (cfg.media_rings.empty()) {
        if (!ancillary_file_descriptors.empty()) {
            return std::unexpected(st2110::Error::InvalidValue);
        }

        return rings;
    }

    rings.reserve(cfg.media_rings.size());

    for (const auto &descriptor : cfg.media_rings) {
        if (descriptor.fd_index >= ancillary_file_descriptors.size()) {
            return std::unexpected(st2110::Error::InvalidValue);
        }

        auto mapped = st2110::MtlWorkerSharedMemoryRingMap::map_from_descriptor(
            descriptor, ancillary_file_descriptors[descriptor.fd_index]);
        if (!mapped.has_value()) {
            return std::unexpected(mapped.error());
        }

        auto valid_headers = mapped->validate_initialized_slot_headers();
        if (!valid_headers.has_value()) {
            return std::unexpected(valid_headers.error());
        }

        rings.push_back(std::move(*mapped));
    }

    return rings;
}

[[nodiscard]] std::expected<st2110::MtlWorkerSharedMemoryRingMap *, st2110::Error>
find_unique_media_ring(std::vector<st2110::MtlWorkerSharedMemoryRingMap> &rings,
                       const st2110::MtlWorkerMediaKind media_kind) noexcept {
    st2110::MtlWorkerSharedMemoryRingMap *found = nullptr;

    for (auto &ring : rings) {
        if (ring.descriptor().media_kind != media_kind) {
            continue;
        }

        if (found) {
            return std::unexpected(st2110::Error::InvalidValue);
        }

        found = &ring;
    }

    return found;
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
    MtlWorkerEventWriter *event_writer = nullptr;
    MtlWorkerGraphStats stats{};
    std::vector<st2110::MtlWorkerSharedMemoryRingMap> media_rings{};
    std::unique_ptr<MtlVideoRxSession> video{};
    std::unique_ptr<MtlAudioRxSession> audio{};

    Impl(MtlRuntimeContext &runtime_context, MtlReceiveGraphConfig graph_cfg, MtlWorkerEventWriter &writer,
         std::vector<st2110::MtlWorkerSharedMemoryRingMap> imported_media_rings)
        : cfg(std::move(graph_cfg)), runtime(&runtime_context), event_writer(&writer),
          media_rings(std::move(imported_media_rings)) {}
};

std::expected<std::unique_ptr<MtlReceiveGraph>, st2110::Error>
MtlReceiveGraph::create(MtlRuntimeContext &runtime, MtlReceiveGraphConfig cfg, MtlWorkerEventWriter &event_writer,
                        std::span<const int> ancillary_file_descriptors) {
    auto runtime_cfg = resolve_graph_runtime_config(cfg);
    if (!runtime_cfg.has_value()) {
        return std::unexpected(runtime_cfg.error());
    }

    if (runtime.config() != *runtime_cfg) {
        return std::unexpected(st2110::Error::InvalidValue);
    }

    auto imported_rings = import_shared_memory_rings(cfg, ancillary_file_descriptors);
    if (!imported_rings.has_value()) {
        return std::unexpected(imported_rings.error());
    }

    auto impl = std::make_unique<Impl>(runtime, std::move(cfg), event_writer, std::move(*imported_rings));
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
        auto video_ring = find_unique_media_ring(impl_->media_rings, st2110::MtlWorkerMediaKind::Video);
        if (!video_ring.has_value()) {
            return std::unexpected(video_ring.error());
        }

        if (!impl_->event_writer) {
            return std::unexpected(st2110::Error::InvalidBackendState);
        }

        auto video_session = MtlVideoRxSession::create(*impl_->runtime, impl_->cfg.graph_id, *impl_->cfg.video,
                                                       impl_->stats, *impl_->event_writer, *video_ring);
        if (!video_session.has_value()) {
            return std::unexpected(video_session.error());
        }

        staged_video = std::move(*video_session);
    }

    if (impl_->cfg.audio.has_value()) {
        auto audio_ring = find_unique_media_ring(impl_->media_rings, st2110::MtlWorkerMediaKind::Audio);
        if (!audio_ring.has_value()) {
            return std::unexpected(audio_ring.error());
        }

        if (!impl_->event_writer) {
            return std::unexpected(st2110::Error::InvalidBackendState);
        }

        auto audio_session = MtlAudioRxSession::create(*impl_->runtime, impl_->cfg.graph_id, *impl_->cfg.audio,
                                                       impl_->stats, *impl_->event_writer, *audio_ring);
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

MtlWorkerGraphStatsSnapshot MtlReceiveGraph::stats_snapshot() const noexcept {
    auto snapshot = impl_->stats.snapshot();

    if (impl_->video) {
        impl_->video->append_stats_snapshot(snapshot);
    }

    if (impl_->audio) {
        impl_->audio->append_stats_snapshot(snapshot);
    }

    if (impl_->runtime) {
        impl_->runtime->append_stats_snapshot(snapshot);
    }

    return snapshot;
}

bool MtlReceiveGraph::healthy() const noexcept {
    if (!impl_ || !impl_->runtime || !impl_->runtime->handle()) {
        return false;
    }

    if (impl_->cfg.video.has_value() && (!impl_->video || !impl_->video->healthy())) {
        return false;
    }

    if (impl_->cfg.audio.has_value() && (!impl_->audio || !impl_->audio->healthy())) {
        return false;
    }

    return true;
}

st2110::Error MtlReceiveGraph::health_error() const noexcept {
    if (!impl_ || !impl_->runtime || !impl_->runtime->handle()) {
        return st2110::Error::InvalidBackendState;
    }

    if (impl_->cfg.video.has_value() && (!impl_->video || !impl_->video->healthy())) {
        return impl_->video ? impl_->video->health_error() : st2110::Error::InvalidBackendState;
    }

    if (impl_->cfg.audio.has_value() && (!impl_->audio || !impl_->audio->healthy())) {
        return impl_->audio ? impl_->audio->health_error() : st2110::Error::InvalidBackendState;
    }

    return st2110::Error::Ok;
}

std::string MtlReceiveGraph::health_message() const {
    if (!impl_ || !impl_->runtime || !impl_->runtime->handle()) {
        return "MTL receive graph has no live runtime handle";
    }

    if (impl_->cfg.video.has_value() && (!impl_->video || !impl_->video->healthy())) {
        return impl_->video ? impl_->video->health_message() : "MTL receive graph has no video session";
    }

    if (impl_->cfg.audio.has_value() && (!impl_->audio || !impl_->audio->healthy())) {
        return impl_->audio ? impl_->audio->health_message() : "MTL receive graph has no audio session";
    }

    return "MTL receive graph healthy";
}

} // namespace st2110_mtl_rx_worker
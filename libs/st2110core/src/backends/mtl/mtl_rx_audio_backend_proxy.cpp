#include <st2110/backends/mtl/mtl_rx_audio_backend_proxy.hpp>

#include <st2110/foundation/error.hpp>

#include <expected>
#include <utility>

namespace st2110 {

MtlRxAudioBackendProxy::MtlRxAudioBackendProxy(MtlAudioStartConfig cfg,
                                               std::shared_ptr<MtlWorkerGraphClient> graph_client)
    : cfg_(std::move(cfg)), graph_client_(std::move(graph_client)) {}

RxBackendLifecycleResult MtlRxAudioBackendProxy::start(IFrameSink *sink) {
    if (!graph_client_) {
        return std::unexpected(Error::InvalidBackendState);
    }

    if (started_) {
        if (sink_ != sink) {
            return std::unexpected(Error::InvalidBackendState);
        }

        return true;
    }

    auto attached = graph_client_->attach_sink(sink);
    if (!attached.has_value()) {
        return std::unexpected(attached.error());
    }

    sink_ = sink;

    /*
     * This starts the graph-level MTL worker client, not an independent audio
     * worker session. The graph client is expected to contain the optional
     * video/audio configs before start() is called.
     */
    auto started = graph_client_->start();
    if (!started.has_value()) {
        if (!graph_client_->running()) {
            graph_client_->detach_sink_noexcept(sink_);
        }

        sink_ = nullptr;
        return std::unexpected(started.error());
    }

    started_ = true;
    return started;
}

RxBackendLifecycleResult MtlRxAudioBackendProxy::stop() {
    if (!graph_client_) {
        sink_ = nullptr;
        started_ = false;
        return true;
    }

    if (!started_) {
        sink_ = nullptr;
        return true;
    }

    auto *attached_sink = sink_;

    auto stopped = graph_client_->stop();

    if (!graph_client_->running()) {
        graph_client_->detach_sink_noexcept(attached_sink);
    }

    sink_ = nullptr;
    started_ = false;

    return stopped;
}

} // namespace st2110
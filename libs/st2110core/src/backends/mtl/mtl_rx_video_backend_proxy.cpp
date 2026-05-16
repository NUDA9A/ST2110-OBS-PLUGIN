#include <st2110/backends/mtl/mtl_rx_video_backend_proxy.hpp>

#include <st2110/foundation/error.hpp>

#include <expected>
#include <optional>
#include <string>
#include <utility>

namespace st2110 {

MtlRxVideoBackendProxy::MtlRxVideoBackendProxy(MtlVideoStartConfig cfg,
                                               std::shared_ptr<MtlWorkerGraphClient> graph_client)
    : cfg_(std::move(cfg)), graph_client_(std::move(graph_client)) {}

RxBackendLifecycleResult MtlRxVideoBackendProxy::start(IFrameSink *sink) {
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
     * This starts the graph-level MTL worker client, not an independent video
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

RxBackendLifecycleResult MtlRxVideoBackendProxy::stop() {
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

std::optional<MtlWorkerErrorDetail> MtlRxVideoBackendProxy::last_error_detail() const {
    return graph_client_ ? graph_client_->last_error_detail() : std::nullopt;
}

std::string MtlRxVideoBackendProxy::last_error_message() const {
    return graph_client_ ? graph_client_->last_error_message() : std::string{};
}

} // namespace st2110
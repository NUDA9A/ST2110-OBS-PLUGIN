#include "mtl_worker_process_state.hpp"

#include <utility>
#include <variant>

namespace st2110_mtl_rx_worker {
namespace {

[[nodiscard]] st2110::MtlWorkerGraphId current_graph_id(const MtlReceiveGraph *graph) noexcept {
    return graph ? graph->config().graph_id : 0;
}

[[nodiscard]] std::expected<st2110::MtlRuntimeConfig, st2110::Error>
resolve_start_request_runtime_config(const st2110::MtlWorkerStartSessionsRequest &request) {
    if (!request.video.has_value() && !request.audio.has_value()) {
        return std::unexpected(st2110::Error::InvalidValue);
    }

    if (request.video.has_value()) {
        st2110::MtlRuntimeConfig runtime = request.video->runtime;

        if (request.audio.has_value() && request.audio->runtime != runtime) {
            return std::unexpected(st2110::Error::InvalidValue);
        }

        return runtime;
    }

    return request.audio->runtime;
}

} // namespace

st2110::MtlWorkerControlEvent MtlWorkerProcessState::handle(const st2110::MtlWorkerControlRequest &request) {
    return std::visit(
        [this](const auto &typed_request) -> st2110::MtlWorkerControlEvent { return handle(typed_request); }, request);
}

st2110::MtlWorkerControlEvent MtlWorkerProcessState::handle(const st2110::MtlWorkerConfigHandshakeRequest &request) {
    if (shutdown_requested_) {
        return make_error(request.request_id, current_graph_id(graph_.get()), st2110::Error::OperationAborted,
                          "Worker shutdown was already requested");
    }

    if (runtime_) {
        if (runtime_->config() != request.runtime) {
            return make_error(request.request_id, current_graph_id(graph_.get()), st2110::Error::InvalidBackendState,
                              "Worker runtime config is incompatible with handshake request");
        }

        return st2110::MtlWorkerHealthEvent{
            .request_id = request.request_id,
            .healthy = true,
            .message = "MTL worker config handshake accepted",
        };
    }

    auto runtime = MtlRuntimeContext::create(request.runtime);
    if (!runtime.has_value()) {
        return make_error(request.request_id, current_graph_id(graph_.get()), runtime.error(),
                          "Failed to initialize MTL worker runtime");
    }

    runtime_ = std::move(*runtime);

    return st2110::MtlWorkerHealthEvent{
        .request_id = request.request_id,
        .healthy = true,
        .message = "MTL worker runtime initialized",
    };
}

st2110::MtlWorkerControlEvent MtlWorkerProcessState::handle(const st2110::MtlWorkerStartSessionsRequest &request) {
    if (shutdown_requested_) {
        return make_error(request.request_id, request.graph_id, st2110::Error::OperationAborted,
                          "Cannot start sessions after worker shutdown was requested");
    }

    if (!request.video.has_value() && !request.audio.has_value()) {
        return make_error(request.request_id, request.graph_id, st2110::Error::InvalidValue,
                          "StartSessions request contains no video or audio session");
    }

    auto runtime_cfg = resolve_start_request_runtime_config(request);
    if (!runtime_cfg.has_value()) {
        return make_error(request.request_id, request.graph_id, runtime_cfg.error(),
                          "Failed to resolve MTL runtime config from StartSessions request");
    }

    if (runtime_) {
        if (runtime_->config() != *runtime_cfg) {
            return make_error(request.request_id, request.graph_id, st2110::Error::InvalidBackendState,
                              "StartSessions runtime config is incompatible with current worker runtime");
        }
    } else {
        auto runtime = MtlRuntimeContext::create(*runtime_cfg);
        if (!runtime.has_value()) {
            return make_error(request.request_id, request.graph_id, runtime.error(),
                              "Failed to initialize MTL worker runtime");
        }

        runtime_ = std::move(*runtime);
    }

    if (graph_) {
        if (graph_->config().graph_id != request.graph_id) {
            return make_error(request.request_id, request.graph_id, st2110::Error::InvalidBackendState,
                              "Worker already owns a different receive graph");
        }

        auto started = graph_->start_sessions();
        if (!started.has_value()) {
            return make_error(request.request_id, request.graph_id, started.error(),
                              "Failed to restart cached MTL receive sessions");
        }

        return st2110::MtlWorkerStartedEvent{
            .request_id = request.request_id,
            .graph_id = request.graph_id,
        };
    }

    auto graph = MtlReceiveGraph::create(*runtime_, request);
    if (!graph.has_value()) {
        return make_error(request.request_id, request.graph_id, graph.error(), "Failed to create MTL receive graph");
    }

    graph_ = std::move(*graph);

    return st2110::MtlWorkerStartedEvent{
        .request_id = request.request_id,
        .graph_id = request.graph_id,
    };
}

st2110::MtlWorkerControlEvent MtlWorkerProcessState::handle(const st2110::MtlWorkerStopSessionsRequest &request) {
    if (!graph_) {
        return st2110::MtlWorkerStoppedEvent{
            .request_id = request.request_id,
            .graph_id = request.graph_id,
        };
    }

    if (graph_->config().graph_id != request.graph_id) {
        return make_error(request.request_id, request.graph_id, st2110::Error::InvalidBackendState,
                          "StopSessions graph id does not match current worker graph");
    }

    graph_->stop_sessions_noexcept();

    return st2110::MtlWorkerStoppedEvent{
        .request_id = request.request_id,
        .graph_id = request.graph_id,
    };
}

st2110::MtlWorkerControlEvent MtlWorkerProcessState::handle(const st2110::MtlWorkerStatsRequest &request) {
    if (graph_ && graph_->config().graph_id != request.graph_id) {
        return make_error(request.request_id, request.graph_id, st2110::Error::InvalidBackendState,
                          "StatsRequest graph id does not match current worker graph");
    }

    return st2110::MtlWorkerStatsEvent{
        .request_id = request.request_id,
        .graph_id = request.graph_id,
        .video_frames_received = 0,
        .audio_blocks_received = 0,
        .video_frames_dropped = 0,
        .audio_blocks_dropped = 0,
    };
}

st2110::MtlWorkerControlEvent MtlWorkerProcessState::handle(const st2110::MtlWorkerHealthCheckRequest &request) {
    return st2110::MtlWorkerHealthEvent{
        .request_id = request.request_id,
        .healthy = !shutdown_requested_,
        .message = shutdown_requested_ ? "MTL worker shutdown requested" : "MTL worker healthy",
    };
}

st2110::MtlWorkerControlEvent MtlWorkerProcessState::handle(const st2110::MtlWorkerShutdownRequest &request) {
    const st2110::MtlWorkerGraphId graph_id = current_graph_id(graph_.get());

    if (graph_) {
        graph_->stop_sessions_noexcept();
        graph_.reset();
    }

    runtime_.reset();

    shutdown_requested_ = true;

    return st2110::MtlWorkerStoppedEvent{
        .request_id = request.request_id,
        .graph_id = graph_id,
    };
}

bool MtlWorkerProcessState::shutdown_requested() const noexcept { return shutdown_requested_; }

bool MtlWorkerProcessState::sessions_running() const noexcept { return graph_ && graph_->sessions_running(); }

st2110::MtlWorkerErrorEvent MtlWorkerProcessState::make_error(st2110::MtlWorkerRequestId request_id,
                                                              st2110::MtlWorkerGraphId graph_id, st2110::Error error,
                                                              const char *message) const {
    return st2110::MtlWorkerErrorEvent{
        .request_id = request_id,
        .graph_id = graph_id,
        .error = error,
        .message = message ? message : "",
    };
}

} // namespace st2110_mtl_rx_worker
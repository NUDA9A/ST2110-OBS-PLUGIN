#include "mtl_worker_process_state.hpp"

#include "mtl_worker_event_writer.hpp"

#include <span>
#include <type_traits>
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

template <typename Request>
[[nodiscard]] st2110::MtlWorkerGraphId graph_id_from_request(const Request &request) noexcept {
    if constexpr (std::is_same_v<Request, st2110::MtlWorkerStartSessionsRequest> ||
                  std::is_same_v<Request, st2110::MtlWorkerStopSessionsRequest> ||
                  std::is_same_v<Request, st2110::MtlWorkerStatsRequest>) {
        return request.graph_id;
    } else {
        return 0;
    }
}

} // namespace

MtlWorkerProcessState::MtlWorkerProcessState(MtlWorkerEventWriter &event_writer) noexcept
    : event_writer_(&event_writer) {}

st2110::MtlWorkerControlEvent MtlWorkerProcessState::handle(const st2110::MtlWorkerControlRequest &request) {
    return std::visit(
        [this](const auto &typed_request) -> st2110::MtlWorkerControlEvent { return handle(typed_request); }, request);
}

st2110::MtlWorkerControlEvent MtlWorkerProcessState::handle(const st2110::MtlWorkerControlRequest &request,
                                                            std::span<const int> ancillary_file_descriptors) {
    return std::visit(
        [this, ancillary_file_descriptors](const auto &typed_request) -> st2110::MtlWorkerControlEvent {
            using Request = std::decay_t<decltype(typed_request)>;

            if constexpr (std::is_same_v<Request, st2110::MtlWorkerStartSessionsRequest>) {
                return handle(typed_request, ancillary_file_descriptors);
            } else {
                if (!ancillary_file_descriptors.empty()) {
                    return make_error(typed_request.request_id, graph_id_from_request(typed_request),
                                      st2110::Error::InvalidValue,
                                      "Ancillary file descriptors are only accepted with StartSessions requests");
                }

                return handle(typed_request);
            }
        },
        request);
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
    return handle(request, {});
}

st2110::MtlWorkerControlEvent MtlWorkerProcessState::handle(const st2110::MtlWorkerStartSessionsRequest &request,
                                                            std::span<const int> ancillary_file_descriptors) {
    if (shutdown_requested_) {
        return make_error(request.request_id, request.graph_id, st2110::Error::OperationAborted,
                          "Cannot start sessions after worker shutdown was requested");
    }

    if (!request.video.has_value() && !request.audio.has_value()) {
        return make_error(request.request_id, request.graph_id, st2110::Error::InvalidValue,
                          "StartSessions request contains no video or audio session");
    }

    if (request.media_rings.empty() && !ancillary_file_descriptors.empty()) {
        return make_error(request.request_id, request.graph_id, st2110::Error::InvalidValue,
                          "StartSessions received ancillary fds without shared-memory ring descriptors");
    }

    for (const auto &descriptor : request.media_rings) {
        if (descriptor.fd_index >= ancillary_file_descriptors.size()) {
            return make_error(request.request_id, request.graph_id, st2110::Error::InvalidValue,
                              "StartSessions shared-memory ring descriptor references a missing ancillary fd");
        }
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

    if (!event_writer_) {
        return make_error(request.request_id, request.graph_id, st2110::Error::InvalidBackendState,
                          "Worker event writer is not configured");
    }

    auto graph = MtlReceiveGraph::create(*runtime_, request, *event_writer_, ancillary_file_descriptors);
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

    const MtlWorkerGraphStatsSnapshot snapshot = graph_ ? graph_->stats_snapshot() : MtlWorkerGraphStatsSnapshot{};

    return st2110::MtlWorkerStatsEvent{
        .request_id = request.request_id,
        .graph_id = request.graph_id,
        .video_frames_received = snapshot.video_frames_received,
        .audio_blocks_received = snapshot.audio_blocks_received,
        .video_frames_dropped = snapshot.video_frames_dropped,
        .audio_blocks_dropped = snapshot.audio_blocks_dropped,
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
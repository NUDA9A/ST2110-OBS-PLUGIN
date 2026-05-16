#ifndef ST2110_OBS_PLUGIN_MTL_RX_WORKER_PROCESS_STATE_HPP
#define ST2110_OBS_PLUGIN_MTL_RX_WORKER_PROCESS_STATE_HPP

#include "mtl_receive_graph.hpp"

#include <st2110/backends/mtl/mtl_worker_protocol.hpp>
#include <st2110/foundation/error.hpp>

#include <memory>
#include <span>
#include <string>
#include <unordered_map>

namespace st2110_mtl_rx_worker {

class MtlWorkerEventWriter;

/*
 * Worker-process-local typed control state.
 *
 * This class owns worker command semantics and passes the worker event writer
 * into graph/session construction so receive threads can emit asynchronous
 * media-ready events through the single worker IPC write boundary.
 *
 * The worker owns one MTL runtime context and may own multiple receive graphs
 * keyed by MtlWorkerGraphId. Ordinary StopSessions releases the selected graph
 * and its shared-memory mappings, but keeps the worker runtime alive for reuse.
 */
class MtlWorkerProcessState final {
  public:
    explicit MtlWorkerProcessState(MtlWorkerEventWriter &event_writer) noexcept;

    st2110::MtlWorkerControlEvent handle(const st2110::MtlWorkerControlRequest &request);
    st2110::MtlWorkerControlEvent handle(const st2110::MtlWorkerControlRequest &request,
                                         std::span<const int> ancillary_file_descriptors);

    st2110::MtlWorkerControlEvent handle(const st2110::MtlWorkerConfigHandshakeRequest &request);
    st2110::MtlWorkerControlEvent handle(const st2110::MtlWorkerStartSessionsRequest &request);
    st2110::MtlWorkerControlEvent handle(const st2110::MtlWorkerStartSessionsRequest &request,
                                         std::span<const int> ancillary_file_descriptors);
    st2110::MtlWorkerControlEvent handle(const st2110::MtlWorkerStopSessionsRequest &request);
    st2110::MtlWorkerControlEvent handle(const st2110::MtlWorkerStatsRequest &request);
    st2110::MtlWorkerControlEvent handle(const st2110::MtlWorkerHealthCheckRequest &request);
    st2110::MtlWorkerControlEvent handle(const st2110::MtlWorkerShutdownRequest &request);

    [[nodiscard]] bool shutdown_requested() const noexcept;
    [[nodiscard]] bool sessions_running() const noexcept;

  private:
    [[nodiscard]] st2110::MtlWorkerErrorEvent make_error(st2110::MtlWorkerRequestId request_id,
                                                         st2110::MtlWorkerGraphId graph_id, st2110::Error error,
                                                         const char *message) const;

    void mark_worker_unhealthy_noexcept(st2110::Error error, const char *message) noexcept;
    [[nodiscard]] bool refresh_worker_health_noexcept() noexcept;

    bool worker_unhealthy_ = false;
    st2110::Error worker_health_error_ = st2110::Error::Ok;
    std::string worker_health_message_ = "MTL worker healthy";

    MtlWorkerEventWriter *event_writer_ = nullptr;
    std::unique_ptr<MtlRuntimeContext> runtime_{};
    std::unordered_map<st2110::MtlWorkerGraphId, std::unique_ptr<MtlReceiveGraph>> graphs_{};
    bool shutdown_requested_ = false;
};

} // namespace st2110_mtl_rx_worker

#endif // ST2110_OBS_PLUGIN_MTL_RX_WORKER_PROCESS_STATE_HPP
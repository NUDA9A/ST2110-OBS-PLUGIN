#ifndef ST2110_OBS_PLUGIN_MTL_RX_WORKER_PROCESS_STATE_HPP
#define ST2110_OBS_PLUGIN_MTL_RX_WORKER_PROCESS_STATE_HPP

#include "mtl_receive_graph.hpp"

#include <st2110/backends/mtl/mtl_worker_protocol.hpp>
#include <st2110/foundation/error.hpp>

#include <memory>
#include <span>

namespace st2110_mtl_rx_worker {

/*
 * Worker-process-local typed control state.
 *
 * This class does not implement IPC. It only defines the command handling
 * semantics that future IPC framing will call after deserializing a worker
 * control request.
 */
class MtlWorkerProcessState final {
  public:
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

    std::unique_ptr<MtlRuntimeContext> runtime_{};
    std::unique_ptr<MtlReceiveGraph> graph_{};
    bool shutdown_requested_ = false;
};

} // namespace st2110_mtl_rx_worker

#endif // ST2110_OBS_PLUGIN_MTL_RX_WORKER_PROCESS_STATE_HPP
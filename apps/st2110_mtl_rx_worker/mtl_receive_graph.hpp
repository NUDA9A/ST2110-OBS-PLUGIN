#ifndef ST2110_OBS_PLUGIN_MTL_RX_WORKER_RECEIVE_GRAPH_HPP
#define ST2110_OBS_PLUGIN_MTL_RX_WORKER_RECEIVE_GRAPH_HPP

#include "mtl_audio_rx_session.hpp"
#include "mtl_runtime_context.hpp"
#include "mtl_video_rx_session.hpp"

#include <st2110/backends/mtl/mtl_worker_protocol.hpp>
#include <st2110/foundation/error.hpp>

#include <expected>
#include <memory>
#include <optional>

namespace st2110_mtl_rx_worker {

using MtlReceiveGraphConfig = st2110::MtlWorkerStartSessionsRequest;

/*
 * Worker-process-local receive graph.
 * Owns optional video/audio sessions created against a worker-owned
 * MtlRuntimeContext. This object does not own mtl_handle, does not know OBS
 * sinks, and does not perform IPC.
 */
class MtlReceiveGraph final {
  public:
    static std::expected<std::unique_ptr<MtlReceiveGraph>, st2110::Error> create(MtlRuntimeContext &runtime,
                                                                                 MtlReceiveGraphConfig cfg);

    ~MtlReceiveGraph();

    MtlReceiveGraph(const MtlReceiveGraph &) = delete;
    MtlReceiveGraph &operator=(const MtlReceiveGraph &) = delete;

    MtlReceiveGraph(MtlReceiveGraph &&) noexcept = delete;
    MtlReceiveGraph &operator=(MtlReceiveGraph &&) noexcept = delete;

    [[nodiscard]] std::expected<bool, st2110::Error> start_sessions();

    void stop_sessions_noexcept() noexcept;

    void wake_block() noexcept;

    [[nodiscard]] bool sessions_running() const noexcept;

    [[nodiscard]] const MtlReceiveGraphConfig &config() const noexcept;

  private:
    struct Impl;

    explicit MtlReceiveGraph(std::unique_ptr<Impl> impl);

    std::unique_ptr<Impl> impl_;
};

} // namespace st2110_mtl_rx_worker

#endif // ST2110_OBS_PLUGIN_MTL_RX_WORKER_RECEIVE_GRAPH_HPP
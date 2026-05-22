#ifndef ST2110_OBS_PLUGIN_MTL_RX_WORKER_VIDEO_RX_SESSION_HPP
#define ST2110_OBS_PLUGIN_MTL_RX_WORKER_VIDEO_RX_SESSION_HPP

#include "mtl_runtime_context.hpp"
#include "mtl_worker_stats.hpp"

#include <st2110/backends/mtl/mtl_worker_shared_memory_ring.hpp>
#include <st2110/delivery/video/mtl_video_start_config.hpp>
#include <st2110/foundation/error.hpp>

#include <expected>
#include <memory>
#include <string>

namespace st2110_mtl_rx_worker {
class MtlWorkerEventWriter;

class MtlVideoRxSession final {
  public:
    static std::expected<std::unique_ptr<MtlVideoRxSession>, st2110::Error>
    create(MtlRuntimeContext &runtime, st2110::MtlWorkerGraphId graph_id, st2110::MtlVideoStartConfig cfg,
           MtlWorkerGraphStats &stats, MtlWorkerEventWriter &event_writer,
           st2110::MtlWorkerSharedMemoryRingMap *media_ring = nullptr);

    ~MtlVideoRxSession();

    MtlVideoRxSession(const MtlVideoRxSession &) = delete;
    MtlVideoRxSession &operator=(const MtlVideoRxSession &) = delete;

    MtlVideoRxSession(MtlVideoRxSession &&) noexcept = delete;
    MtlVideoRxSession &operator=(MtlVideoRxSession &&) noexcept = delete;

    void wake_block() noexcept;

    [[nodiscard]] bool healthy() const noexcept;
    [[nodiscard]] st2110::Error health_error() const noexcept;
    [[nodiscard]] std::string health_message() const;

    [[nodiscard]] const st2110::MtlVideoStartConfig &config() const noexcept;
    [[nodiscard]] const st2110::MtlWorkerSharedMemoryRingMap *media_ring() const noexcept;

    void append_stats_snapshot(MtlWorkerGraphStatsSnapshot &snapshot) const noexcept;

  private:
    struct Impl;

    explicit MtlVideoRxSession(std::unique_ptr<Impl> impl);

    std::unique_ptr<Impl> impl_;
};

} // namespace st2110_mtl_rx_worker

#endif // ST2110_OBS_PLUGIN_MTL_RX_WORKER_VIDEO_RX_SESSION_HPP
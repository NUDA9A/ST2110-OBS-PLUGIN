#ifndef ST2110_OBS_PLUGIN_MTL_RX_WORKER_STATS_HPP
#define ST2110_OBS_PLUGIN_MTL_RX_WORKER_STATS_HPP

#include <atomic>
#include <cstdint>

namespace st2110_mtl_rx_worker {

struct MtlWorkerGraphStatsSnapshot {
    std::uint64_t video_frames_received = 0;
    std::uint64_t audio_blocks_received = 0;
    std::uint64_t video_frames_dropped = 0;
    std::uint64_t audio_blocks_dropped = 0;
};

/*
 * Worker-process-local graph counters.
 *
 * These counters observe worker-local receive activity only.
 * They do not imply OBS delivery yet and do not model shared-memory slot
 * ownership. Drops remain zero until the shared-memory data plane exists.
 */
class MtlWorkerGraphStats final {
  public:
    void record_video_frame_received() noexcept { video_frames_received_.fetch_add(1, std::memory_order_relaxed); }

    void record_audio_block_received() noexcept { audio_blocks_received_.fetch_add(1, std::memory_order_relaxed); }

    void record_video_frame_dropped() noexcept { video_frames_dropped_.fetch_add(1, std::memory_order_relaxed); }

    void record_audio_block_dropped() noexcept { audio_blocks_dropped_.fetch_add(1, std::memory_order_relaxed); }

    [[nodiscard]] MtlWorkerGraphStatsSnapshot snapshot() const noexcept {
        return MtlWorkerGraphStatsSnapshot{
            .video_frames_received = video_frames_received_.load(std::memory_order_relaxed),
            .audio_blocks_received = audio_blocks_received_.load(std::memory_order_relaxed),
            .video_frames_dropped = video_frames_dropped_.load(std::memory_order_relaxed),
            .audio_blocks_dropped = audio_blocks_dropped_.load(std::memory_order_relaxed),
        };
    }

  private:
    std::atomic_uint64_t video_frames_received_{0};
    std::atomic_uint64_t audio_blocks_received_{0};
    std::atomic_uint64_t video_frames_dropped_{0};
    std::atomic_uint64_t audio_blocks_dropped_{0};
};

} // namespace st2110_mtl_rx_worker

#endif // ST2110_OBS_PLUGIN_MTL_RX_WORKER_STATS_HPP
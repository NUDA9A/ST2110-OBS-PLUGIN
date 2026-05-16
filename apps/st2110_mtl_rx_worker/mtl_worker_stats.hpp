#ifndef ST2110_OBS_PLUGIN_MTL_RX_WORKER_STATS_HPP
#define ST2110_OBS_PLUGIN_MTL_RX_WORKER_STATS_HPP

#include <st2110/backends/mtl/mtl_worker_protocol.hpp>

#include <atomic>
#include <cstdint>

namespace st2110_mtl_rx_worker {

struct MtlWorkerGraphStatsSnapshot {
    std::uint64_t video_frames_received = 0;
    std::uint64_t audio_blocks_received = 0;
    std::uint64_t video_frames_dropped = 0;
    std::uint64_t audio_blocks_dropped = 0;

    std::uint64_t video_frame_packets_total = 0;
    std::uint64_t video_frame_packets_received_primary = 0;
    std::uint64_t video_frame_packets_received_redundant = 0;
    std::uint64_t video_reconstructed_frames = 0;
    std::uint64_t video_corrupted_frames = 0;

    bool video_session_stats_available = false;
    st2110::MtlWorkerRxPortStats video_session_primary{};
    st2110::MtlWorkerRxPortStats video_session_redundant{};
    std::uint64_t video_session_packets_received = 0;
    std::uint64_t video_session_packets_out_of_order = 0;
    std::uint64_t video_session_packets_wrong_ssrc_dropped = 0;
    std::uint64_t video_session_packets_wrong_payload_type_dropped = 0;
    std::uint64_t video_session_bytes_received = 0;
    std::uint64_t video_session_frames_dropped = 0;
    std::uint64_t video_session_frames_packets_missed = 0;
    std::uint64_t video_session_packets_wrong_length_dropped = 0;
    std::uint64_t video_session_slot_get_frame_failures = 0;
    std::uint64_t video_session_stats_query_failures = 0;

    std::uint64_t audio_block_bytes_received = 0;
    std::uint64_t audio_block_packets_total = 0;
    std::uint64_t audio_block_packets_received_primary = 0;
    std::uint64_t audio_block_packets_received_redundant = 0;

    bool audio_session_stats_available = false;
    st2110::MtlWorkerRxPortStats audio_session_primary{};
    st2110::MtlWorkerRxPortStats audio_session_redundant{};
    std::uint64_t audio_session_packets_received = 0;
    std::uint64_t audio_session_packets_out_of_order = 0;
    std::uint64_t audio_session_packets_wrong_ssrc_dropped = 0;
    std::uint64_t audio_session_packets_wrong_payload_type_dropped = 0;
    std::uint64_t audio_session_packets_redundant = 0;
    std::uint64_t audio_session_packets_dropped = 0;
    std::uint64_t audio_session_packets_length_mismatch_dropped = 0;
    std::uint64_t audio_session_slot_get_frame_failures = 0;
    std::uint64_t audio_session_stats_query_failures = 0;

    bool mtl_primary_port_stats_available = false;
    st2110::MtlWorkerDeviceRxPortStats mtl_primary_port{};
    bool mtl_redundant_port_stats_available = false;
    st2110::MtlWorkerDeviceRxPortStats mtl_redundant_port{};
    std::uint64_t mtl_port_stats_query_failures = 0;
};

/*
 * Worker-process-local graph counters.
 *
 * These counters observe worker-local receive activity and MTL-visible
 * metadata only. They do not invent socket/depacketizer/reorder counters for
 * the MTL backend.
 */
class MtlWorkerGraphStats final {
  public:
    void record_video_frame_received() noexcept { video_frames_received_.fetch_add(1, std::memory_order_relaxed); }

    void record_audio_block_received() noexcept { audio_blocks_received_.fetch_add(1, std::memory_order_relaxed); }

    void record_video_frame_dropped() noexcept { video_frames_dropped_.fetch_add(1, std::memory_order_relaxed); }

    void record_audio_block_dropped() noexcept { audio_blocks_dropped_.fetch_add(1, std::memory_order_relaxed); }

    void record_video_frame_packet_metadata(std::uint32_t packets_total, std::uint32_t packets_primary,
                                            std::uint32_t packets_redundant, bool reconstructed,
                                            bool corrupted) noexcept {
        video_frame_packets_total_.fetch_add(packets_total, std::memory_order_relaxed);
        video_frame_packets_received_primary_.fetch_add(packets_primary, std::memory_order_relaxed);
        video_frame_packets_received_redundant_.fetch_add(packets_redundant, std::memory_order_relaxed);

        if (reconstructed) {
            video_reconstructed_frames_.fetch_add(1, std::memory_order_relaxed);
        }

        if (corrupted) {
            video_corrupted_frames_.fetch_add(1, std::memory_order_relaxed);
        }
    }

    void record_audio_block_packet_metadata(std::uint64_t bytes_received, std::uint32_t packets_total,
                                            std::uint32_t packets_primary, std::uint32_t packets_redundant) noexcept {
        audio_block_bytes_received_.fetch_add(bytes_received, std::memory_order_relaxed);
        audio_block_packets_total_.fetch_add(packets_total, std::memory_order_relaxed);
        audio_block_packets_received_primary_.fetch_add(packets_primary, std::memory_order_relaxed);
        audio_block_packets_received_redundant_.fetch_add(packets_redundant, std::memory_order_relaxed);
    }

    [[nodiscard]] MtlWorkerGraphStatsSnapshot snapshot() const noexcept {
        return MtlWorkerGraphStatsSnapshot{
            .video_frames_received = video_frames_received_.load(std::memory_order_relaxed),
            .audio_blocks_received = audio_blocks_received_.load(std::memory_order_relaxed),
            .video_frames_dropped = video_frames_dropped_.load(std::memory_order_relaxed),
            .audio_blocks_dropped = audio_blocks_dropped_.load(std::memory_order_relaxed),

            .video_frame_packets_total = video_frame_packets_total_.load(std::memory_order_relaxed),
            .video_frame_packets_received_primary =
                video_frame_packets_received_primary_.load(std::memory_order_relaxed),
            .video_frame_packets_received_redundant =
                video_frame_packets_received_redundant_.load(std::memory_order_relaxed),
            .video_reconstructed_frames = video_reconstructed_frames_.load(std::memory_order_relaxed),
            .video_corrupted_frames = video_corrupted_frames_.load(std::memory_order_relaxed),

            .audio_block_bytes_received = audio_block_bytes_received_.load(std::memory_order_relaxed),
            .audio_block_packets_total = audio_block_packets_total_.load(std::memory_order_relaxed),
            .audio_block_packets_received_primary =
                audio_block_packets_received_primary_.load(std::memory_order_relaxed),
            .audio_block_packets_received_redundant =
                audio_block_packets_received_redundant_.load(std::memory_order_relaxed),
        };
    }

  private:
    std::atomic_uint64_t video_frames_received_{0};
    std::atomic_uint64_t audio_blocks_received_{0};
    std::atomic_uint64_t video_frames_dropped_{0};
    std::atomic_uint64_t audio_blocks_dropped_{0};

    std::atomic_uint64_t video_frame_packets_total_{0};
    std::atomic_uint64_t video_frame_packets_received_primary_{0};
    std::atomic_uint64_t video_frame_packets_received_redundant_{0};
    std::atomic_uint64_t video_reconstructed_frames_{0};
    std::atomic_uint64_t video_corrupted_frames_{0};

    std::atomic_uint64_t audio_block_bytes_received_{0};
    std::atomic_uint64_t audio_block_packets_total_{0};
    std::atomic_uint64_t audio_block_packets_received_primary_{0};
    std::atomic_uint64_t audio_block_packets_received_redundant_{0};
};

} // namespace st2110_mtl_rx_worker

#endif // ST2110_OBS_PLUGIN_MTL_RX_WORKER_STATS_HPP
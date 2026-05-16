#ifndef ST2110_OBS_PLUGIN_MTL_WORKER_PROTOCOL_HPP
#define ST2110_OBS_PLUGIN_MTL_WORKER_PROTOCOL_HPP

#include <st2110/backends/mtl/mtl_runtime_config.hpp>
#include <st2110/delivery/audio/mtl_audio_start_config.hpp>
#include <st2110/delivery/video/mtl_video_start_config.hpp>
#include <st2110/foundation/error.hpp>
#include <st2110/foundation/timestamp.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace st2110 {

struct MtlWorkerRxPortStats {
    std::uint64_t packets = 0;
    std::uint64_t bytes = 0;
    std::uint64_t frames = 0;
    std::uint64_t incomplete_frames = 0;
    std::uint64_t err_packets = 0;
    std::uint64_t out_of_order_packets = 0;
};

struct MtlWorkerDeviceRxPortStats {
    std::uint64_t rx_packets = 0;
    std::uint64_t rx_bytes = 0;
    std::uint64_t rx_err_packets = 0;
    std::uint64_t rx_hw_dropped_packets = 0;
    std::uint64_t rx_nombuf_packets = 0;
};

struct MtlWorkerSt20RxUserStats {
    st2110::MtlWorkerRxPortStats primary{};
    st2110::MtlWorkerRxPortStats redundant{};

    std::uint64_t stat_pkts_received = 0;
    std::uint64_t stat_pkts_out_of_order = 0;
    std::uint64_t stat_pkts_wrong_ssrc_dropped = 0;
    std::uint64_t stat_pkts_wrong_pt_dropped = 0;

    std::uint64_t stat_bytes_received = 0;
    std::uint64_t stat_slices_received = 0;
    std::uint64_t stat_pkts_idx_dropped = 0;
    std::uint64_t stat_pkts_offset_dropped = 0;
    std::uint64_t stat_frames_dropped = 0;
    std::uint64_t stat_pkts_idx_oo_bitmap = 0;
    std::uint64_t stat_frames_pks_missed = 0;
    std::uint64_t stat_pkts_rtp_ring_full = 0;
    std::uint64_t stat_pkts_no_slot = 0;
    std::uint64_t stat_pkts_redundant_dropped = 0;
    std::uint64_t stat_pkts_wrong_interlace_dropped = 0;
    std::uint64_t stat_pkts_wrong_len_dropped = 0;
    std::uint64_t stat_pkts_enqueue_fallback = 0;
    std::uint64_t stat_pkts_dma = 0;
    std::uint64_t stat_pkts_slice_fail = 0;
    std::uint64_t stat_pkts_slice_merged = 0;
    std::uint64_t stat_pkts_multi_segments_received = 0;
    std::uint64_t stat_pkts_not_bpm = 0;
    std::uint64_t stat_pkts_wrong_payload_hdr_split = 0;
    std::uint64_t stat_mismatch_hdr_split_frame = 0;
    std::uint64_t stat_pkts_copy_hdr_split = 0;
    std::uint64_t stat_vsync_mismatch = 0;
    std::uint64_t stat_slot_get_frame_fail = 0;
    std::uint64_t stat_slot_query_ext_fail = 0;
    std::uint64_t stat_pkts_simulate_loss = 0;
    std::uint64_t stat_pkts_user_meta = 0;
    std::uint64_t stat_pkts_user_meta_err = 0;
    std::uint64_t stat_pkts_retransmit = 0;
    std::uint64_t stat_interlace_first_field = 0;
    std::uint64_t stat_interlace_second_field = 0;
    std::uint64_t stat_st22_boxes = 0;
    std::uint64_t stat_burst_pkts_max = 0;
    std::uint64_t stat_burst_succ_cnt = 0;
    std::uint64_t stat_burst_pkts_sum = 0;
    std::uint64_t incomplete_frames_cnt = 0;
    std::uint64_t stat_pkts_wrong_kmod_dropped = 0;
};

using MtlWorkerRequestId = std::uint64_t;
using MtlWorkerGraphId = std::uint64_t;
using MtlWorkerSlotId = std::uint64_t;
using MtlWorkerSharedMemoryRingId = std::uint64_t;

inline constexpr std::uint32_t mtlWorkerSharedMemoryRingLayoutVersion = 3;
inline constexpr std::size_t defaultMtlWorkerMaxSharedMemoryRingDescriptors = 16;

enum class MtlWorkerMediaKind : std::uint32_t {
    Video = 0,
    Audio = 1,
};

struct MtlWorkerSharedMemoryRingDescriptor {
    MtlWorkerSharedMemoryRingId ring_id = 0;

    MtlWorkerMediaKind media_kind = MtlWorkerMediaKind::Video;

    std::uint32_t fd_index = 0;

    std::uint32_t layout_version = mtlWorkerSharedMemoryRingLayoutVersion;

    std::uint64_t mapped_size_bytes = 0;

    std::uint64_t slot_region_offset_bytes = 0;

    std::uint32_t slot_count = 0;

    std::uint64_t slot_stride_bytes = 0;

    std::uint64_t slot_payload_offset_bytes = 0;

    std::uint64_t slot_payload_capacity_bytes = 0;
};

struct MtlWorkerConfigHandshakeRequest {
    MtlWorkerRequestId request_id = 0;
    MtlRuntimeConfig runtime{};
};

struct MtlWorkerStartSessionsRequest {
    MtlWorkerRequestId request_id = 0;
    MtlWorkerGraphId graph_id = 0;

    std::optional<MtlVideoStartConfig> video{};
    std::optional<MtlAudioStartConfig> audio{};

    std::vector<MtlWorkerSharedMemoryRingDescriptor> media_rings{};
};

struct MtlWorkerStopSessionsRequest {
    MtlWorkerRequestId request_id = 0;
    MtlWorkerGraphId graph_id = 0;
};

struct MtlWorkerStatsRequest {
    MtlWorkerRequestId request_id = 0;
    MtlWorkerGraphId graph_id = 0;
};

struct MtlWorkerHealthCheckRequest {
    MtlWorkerRequestId request_id = 0;
};

struct MtlWorkerShutdownRequest {
    MtlWorkerRequestId request_id = 0;
};

using MtlWorkerControlRequest =
    std::variant<MtlWorkerConfigHandshakeRequest, MtlWorkerStartSessionsRequest, MtlWorkerStopSessionsRequest,
                 MtlWorkerStatsRequest, MtlWorkerHealthCheckRequest, MtlWorkerShutdownRequest>;

struct MtlWorkerStartedEvent {
    MtlWorkerRequestId request_id = 0;
    MtlWorkerGraphId graph_id = 0;
};

struct MtlWorkerStoppedEvent {
    MtlWorkerRequestId request_id = 0;
    MtlWorkerGraphId graph_id = 0;
};

struct MtlWorkerErrorEvent {
    MtlWorkerRequestId request_id = 0;
    MtlWorkerGraphId graph_id = 0;
    Error error = Error::Ok;
    std::string message{};
};

struct MtlWorkerStatsEvent {
    MtlWorkerRequestId request_id = 0;
    MtlWorkerGraphId graph_id = 0;

    std::uint64_t video_frames_received = 0;
    std::uint64_t audio_blocks_received = 0;
    std::uint64_t video_frames_dropped = 0;
    std::uint64_t audio_blocks_dropped = 0;

    std::uint64_t video_frame_packets_total = 0;
    std::uint64_t video_frame_packets_received_primary = 0;
    std::uint64_t video_frame_packets_received_redundant = 0;
    std::uint64_t video_reconstructed_frames = 0;
    std::uint64_t video_corrupted_frames = 0;
    std::uint64_t video_complete_frames = 0;

    bool video_session_stats_available = false;
    MtlWorkerRxPortStats video_session_primary{};
    MtlWorkerRxPortStats video_session_redundant{};
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

    MtlWorkerSt20RxUserStats video_st20_rx{};

    std::uint64_t audio_block_bytes_received = 0;
    std::uint64_t audio_block_packets_total = 0;
    std::uint64_t audio_block_packets_received_primary = 0;
    std::uint64_t audio_block_packets_received_redundant = 0;

    bool audio_session_stats_available = false;
    MtlWorkerRxPortStats audio_session_primary{};
    MtlWorkerRxPortStats audio_session_redundant{};
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
    MtlWorkerDeviceRxPortStats mtl_primary_port{};
    bool mtl_redundant_port_stats_available = false;
    MtlWorkerDeviceRxPortStats mtl_redundant_port{};
    std::uint64_t mtl_port_stats_query_failures = 0;

    std::uint64_t frame_ready_events = 0;
    std::uint64_t audio_block_ready_events = 0;
    std::uint64_t video_frames_delivered = 0;
    std::uint64_t audio_blocks_delivered = 0;
    std::uint64_t released_slots = 0;
    std::uint64_t malformed_ready_events = 0;
    std::uint64_t stale_ready_events = 0;
    std::uint64_t delivery_failures = 0;
    std::uint64_t release_failures = 0;
    std::uint64_t ignored_events = 0;
};

struct MtlWorkerHealthEvent {
    MtlWorkerRequestId request_id = 0;
    bool healthy = false;
    std::string message{};
};

struct MtlWorkerFrameReadyEvent {
    MtlWorkerGraphId graph_id = 0;
    MtlWorkerSharedMemoryRingId ring_id = 0;
    MtlWorkerSlotId slot_id = 0;
    std::uint64_t sequence = 0;
};

struct MtlWorkerAudioBlockReadyEvent {
    MtlWorkerGraphId graph_id = 0;
    MtlWorkerSharedMemoryRingId ring_id = 0;
    MtlWorkerSlotId slot_id = 0;
    std::uint64_t sequence = 0;
};

using MtlWorkerControlEvent =
    std::variant<MtlWorkerStartedEvent, MtlWorkerStoppedEvent, MtlWorkerErrorEvent, MtlWorkerStatsEvent,
                 MtlWorkerHealthEvent, MtlWorkerFrameReadyEvent, MtlWorkerAudioBlockReadyEvent>;

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_MTL_WORKER_PROTOCOL_HPP
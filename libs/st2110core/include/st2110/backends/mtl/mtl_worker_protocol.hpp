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

/*
 * Shared typed protocol model for OBS-process MTL proxy/client code and the
 * MTL worker process.
 *
 * This header intentionally contains no MTL API types and no OS IPC types.
 * Concrete serialization/framing over Unix sockets, pipes, or another IPC
 * mechanism is a separate boundary.
 */

using MtlWorkerRequestId = std::uint64_t;
using MtlWorkerGraphId = std::uint64_t;
using MtlWorkerSlotId = std::uint64_t;
using MtlWorkerSharedMemoryRingId = std::uint64_t;

inline constexpr std::uint32_t mtlWorkerSharedMemoryRingLayoutVersion = 2;
inline constexpr std::size_t defaultMtlWorkerMaxSharedMemoryRingDescriptors = 16;

enum class MtlWorkerMediaKind : std::uint32_t {
    Video = 0,
    Audio = 1,
};

struct MtlWorkerSharedMemoryRingDescriptor {
    /*
     * Stable graph-local ring identifier.
     *
     * Ready events reference this id; raw fd numbers are never serialized into
     * the typed protocol.
     */
    MtlWorkerSharedMemoryRingId ring_id = 0;

    /*
     * Which media path owns this ring.
     */
    MtlWorkerMediaKind media_kind = MtlWorkerMediaKind::Video;

    /*
     * Index into the ancillary fd vector attached to the same StartSessions IPC
     * frame.
     *
     * This is not an OS fd number.
     */
    std::uint32_t fd_index = 0;

    /*
     * Version of the shared-memory slot layout understood by both processes.
     */
    std::uint32_t layout_version = mtlWorkerSharedMemoryRingLayoutVersion;

    /*
     * Total mmap size for the fd.
     */
    std::uint64_t mapped_size_bytes = 0;

    /*
     * Byte offset where the slot region starts inside the mapping.
     */
    std::uint64_t slot_region_offset_bytes = 0;

    /*
     * Number of fixed-stride slots in the slot region.
     */
    std::uint32_t slot_count = 0;

    /*
     * Distance in bytes between consecutive slot starts.
     */
    std::uint64_t slot_stride_bytes = 0;

    /*
     * Byte offset from slot start to media payload.
     */
    std::uint64_t slot_payload_offset_bytes = 0;

    /*
     * Maximum payload bytes available in each slot.
     */
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

    /*
     * Shared-memory media rings available to this graph.
     *
     * Each descriptor references an fd by fd_index into the ancillary fd vector
     * attached to the same StartSessions frame. The descriptor never carries a
     * raw fd value.
     *
     * Empty vector preserves the current control-only behavior.
     */
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

    /*
     * OBS-process-side async/data-plane counters.
     *
     * Worker-side StatsRequest responses naturally leave these as zero. The
     * MtlWorkerGraphClient merges its local async delivery snapshot before
     * returning stats() to callers.
     */
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
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

namespace st2110 {

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

enum class MtlWorkerMediaKind {
    Video,
    Audio,
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
    std::variant<MtlWorkerConfigHandshakeRequest,
                 MtlWorkerStartSessionsRequest,
                 MtlWorkerStopSessionsRequest,
                 MtlWorkerStatsRequest,
                 MtlWorkerHealthCheckRequest,
                 MtlWorkerShutdownRequest>;

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
};

struct MtlWorkerHealthEvent {
    MtlWorkerRequestId request_id = 0;
    bool healthy = false;
    std::string message{};
};

struct MtlWorkerFrameReadyEvent {
    MtlWorkerGraphId graph_id = 0;
    MtlWorkerSlotId slot_id = 0;

    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t rtp_timestamp = 0;
    TimestampNs receive_timestamp_ns = 0;

    std::size_t payload_size = 0;
    bool partial = false;
};

struct MtlWorkerAudioBlockReadyEvent {
    MtlWorkerGraphId graph_id = 0;
    MtlWorkerSlotId slot_id = 0;

    std::uint32_t sample_rate_hz = 0;
    std::uint32_t channels = 0;
    std::uint32_t samples_per_channel = 0;
    std::uint32_t rtp_timestamp = 0;
    TimestampNs receive_timestamp_ns = 0;

    std::size_t payload_size = 0;
    bool partial = false;
};

using MtlWorkerControlEvent =
    std::variant<MtlWorkerStartedEvent,
                 MtlWorkerStoppedEvent,
                 MtlWorkerErrorEvent,
                 MtlWorkerStatsEvent,
                 MtlWorkerHealthEvent,
                 MtlWorkerFrameReadyEvent,
                 MtlWorkerAudioBlockReadyEvent>;

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_MTL_WORKER_PROTOCOL_HPP
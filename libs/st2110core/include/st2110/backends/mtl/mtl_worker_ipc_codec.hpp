#ifndef ST2110_OBS_PLUGIN_MTL_WORKER_IPC_CODEC_HPP
#define ST2110_OBS_PLUGIN_MTL_WORKER_IPC_CODEC_HPP

#include <st2110/backends/mtl/mtl_worker_protocol.hpp>
#include <st2110/foundation/error.hpp>

#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace st2110 {

/*
 * Typed binary codec for MTL worker control IPC.
 *
 * This layer converts typed protocol models to/from byte payloads carried by
 * the length-prefixed framing layer.
 *
 *
 * Current implementation supports:
 * - ConfigHandshakeRequest
 * - StartSessionsRequest
 * - StopSessionsRequest
 * - ShutdownRequest
 * - HealthEvent
 * - ErrorEvent
 * - StartedEvent
 * - StoppedEvent
 */
[[nodiscard]] std::expected<std::vector<std::uint8_t>, Error>
serialize_mtl_worker_control_request(const MtlWorkerControlRequest &request);

[[nodiscard]] std::expected<MtlWorkerControlRequest, Error>
deserialize_mtl_worker_control_request(std::span<const std::uint8_t> payload);

[[nodiscard]] std::expected<std::vector<std::uint8_t>, Error>
serialize_mtl_worker_control_event(const MtlWorkerControlEvent &event);

[[nodiscard]] std::expected<MtlWorkerControlEvent, Error>
deserialize_mtl_worker_control_event(std::span<const std::uint8_t> payload);

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_MTL_WORKER_IPC_CODEC_HPP
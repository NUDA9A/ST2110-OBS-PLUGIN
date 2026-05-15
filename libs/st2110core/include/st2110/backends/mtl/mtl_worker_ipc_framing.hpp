#ifndef ST2110_OBS_PLUGIN_MTL_WORKER_IPC_FRAMING_HPP
#define ST2110_OBS_PLUGIN_MTL_WORKER_IPC_FRAMING_HPP

#include <st2110/foundation/error.hpp>

#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace st2110 {

/*
 * Shared low-level framing for OBS-process MTL worker control IPC.
 *
 * This layer knows only byte frames:
 *
 *   uint32 big-endian payload_size
 *   payload_size bytes
 *
 * Typed MtlWorkerControlRequest/Event serialization is a separate layer.
 */
inline constexpr std::uint32_t defaultMtlWorkerMaxControlFrameBytes = 1024 * 1024;

[[nodiscard]] std::expected<bool, Error> write_mtl_worker_control_frame(int fd, std::span<const std::uint8_t> payload);

[[nodiscard]] std::expected<std::vector<std::uint8_t>, Error>
read_mtl_worker_control_frame(int fd, std::uint32_t max_frame_bytes = defaultMtlWorkerMaxControlFrameBytes);

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_MTL_WORKER_IPC_FRAMING_HPP
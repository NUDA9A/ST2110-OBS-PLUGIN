#ifndef ST2110_OBS_PLUGIN_MTL_WORKER_IPC_FRAMING_HPP
#define ST2110_OBS_PLUGIN_MTL_WORKER_IPC_FRAMING_HPP

#include <st2110/foundation/error.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace st2110 {

/*
 * Shared low-level framing for OBS-process <-> MTL-worker control IPC.
 *
 * Base byte framing:
 *
 *   uint32 big-endian payload_size
 *   payload_size bytes
 *
 * The fd-aware API sends the same byte frame and may attach Unix ancillary
 * file descriptors to the same logical frame with SCM_RIGHTS.
 *
 * This layer knows only byte payloads and owned file descriptors. Typed
 * MtlWorkerControlRequest/Event serialization remains a separate layer.
 */
inline constexpr std::uint32_t defaultMtlWorkerMaxControlFrameBytes = 1024 * 1024;
inline constexpr std::size_t defaultMtlWorkerMaxAncillaryFileDescriptors = 64;

/*
 * Received IPC frame.
 *
 * File descriptors received through SCM_RIGHTS are owned by this object and
 * are closed on destruction unless release_file_descriptors() is called.
 *
 * The payload is ordinary byte data and has no implicit typed meaning at this
 * layer.
 */
class MtlWorkerIpcFrame final {
  public:
    MtlWorkerIpcFrame() = default;
    MtlWorkerIpcFrame(std::vector<std::uint8_t> payload, std::vector<int> file_descriptors);

    ~MtlWorkerIpcFrame();

    MtlWorkerIpcFrame(const MtlWorkerIpcFrame &) = delete;
    MtlWorkerIpcFrame &operator=(const MtlWorkerIpcFrame &) = delete;

    MtlWorkerIpcFrame(MtlWorkerIpcFrame &&other) noexcept;
    MtlWorkerIpcFrame &operator=(MtlWorkerIpcFrame &&other) noexcept;

    [[nodiscard]] const std::vector<std::uint8_t> &payload() const noexcept;
    [[nodiscard]] std::vector<std::uint8_t> &payload() noexcept;

    [[nodiscard]] std::vector<std::uint8_t> release_payload() noexcept;

    [[nodiscard]] const std::vector<int> &file_descriptors() const noexcept;
    [[nodiscard]] bool has_file_descriptors() const noexcept;

    /*
     * Transfers fd ownership to the caller. After this call the frame destructor
     * no longer closes those descriptors.
     */
    [[nodiscard]] std::vector<int> release_file_descriptors() noexcept;

  private:
    std::vector<std::uint8_t> payload_{};
    std::vector<int> file_descriptors_{};
};

/*
 * Payload-only compatibility API.
 *
 * read_mtl_worker_control_frame() rejects frames that carry ancillary file
 * descriptors. Use read_mtl_worker_control_frame_with_fds() when descriptors
 * are expected.
 */
[[nodiscard]] std::expected<bool, Error> write_mtl_worker_control_frame(int fd, std::span<const std::uint8_t> payload);

[[nodiscard]] std::expected<std::vector<std::uint8_t>, Error>
read_mtl_worker_control_frame(int fd, std::uint32_t max_frame_bytes = defaultMtlWorkerMaxControlFrameBytes);

/*
 * Fd-aware API.
 *
 * write_mtl_worker_control_frame_with_fds() borrows the supplied descriptors;
 * ownership remains with the caller.
 *
 * read_mtl_worker_control_frame_with_fds() returns owned descriptors inside
 * MtlWorkerIpcFrame.
 */
[[nodiscard]] std::expected<bool, Error> write_mtl_worker_control_frame_with_fds(int fd,
                                                                                 std::span<const std::uint8_t> payload,
                                                                                 std::span<const int> file_descriptors);

[[nodiscard]] std::expected<MtlWorkerIpcFrame, Error>
read_mtl_worker_control_frame_with_fds(int fd, std::uint32_t max_frame_bytes = defaultMtlWorkerMaxControlFrameBytes,
                                       std::size_t max_file_descriptors = defaultMtlWorkerMaxAncillaryFileDescriptors);

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_MTL_WORKER_IPC_FRAMING_HPP
#ifndef ST2110_OBS_PLUGIN_MTL_WORKER_SHARED_MEMORY_RING_OWNER_HPP
#define ST2110_OBS_PLUGIN_MTL_WORKER_SHARED_MEMORY_RING_OWNER_HPP

#include <st2110/backends/mtl/mtl_worker_shared_memory_ring.hpp>
#include <st2110/foundation/error.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>

namespace st2110 {

/*
 * OBS-process-side owner for a shared-memory media ring.
 *
 * This object owns:
 * - the memfd;
 * - the mmap mapping through MtlWorkerSharedMemoryRingMap.
 *
 * The worker receives only:
 * - a descriptor in MtlWorkerStartSessionsRequest.media_rings;
 * - a borrowed duplicate fd through SCM_RIGHTS.
 *
 * This class contains no MTL API types.
 */
struct MtlWorkerSharedMemoryRingOwnerConfig {
    MtlWorkerSharedMemoryRingId ring_id = 0;
    MtlWorkerMediaKind media_kind = MtlWorkerMediaKind::Video;

    /*
     * Index that the descriptor will use to reference this ring fd inside the
     * ancillary fd vector attached to StartSessions.
     */
    std::uint32_t fd_index = 0;

    std::uint32_t slot_count = 0;
    std::uint64_t slot_payload_capacity_bytes = 0;

    /*
     * Human-readable memfd name. It is diagnostic only and not part of the wire
     * protocol.
     */
    std::string debug_name{};
};

class MtlWorkerSharedMemoryRingOwner final {
  public:
    MtlWorkerSharedMemoryRingOwner() = default;
    ~MtlWorkerSharedMemoryRingOwner();

    MtlWorkerSharedMemoryRingOwner(const MtlWorkerSharedMemoryRingOwner &) = delete;
    MtlWorkerSharedMemoryRingOwner &operator=(const MtlWorkerSharedMemoryRingOwner &) = delete;

    MtlWorkerSharedMemoryRingOwner(MtlWorkerSharedMemoryRingOwner &&other) noexcept;
    MtlWorkerSharedMemoryRingOwner &operator=(MtlWorkerSharedMemoryRingOwner &&other) noexcept;

    [[nodiscard]] static std::expected<MtlWorkerSharedMemoryRingOwner, Error>
    create(const MtlWorkerSharedMemoryRingOwnerConfig &cfg);

    void close_noexcept() noexcept;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] int fd() const noexcept;

    [[nodiscard]] const MtlWorkerSharedMemoryRingDescriptor &descriptor() const noexcept;

    [[nodiscard]] MtlWorkerSharedMemoryRingMap &ring_map() noexcept;
    [[nodiscard]] const MtlWorkerSharedMemoryRingMap &ring_map() const noexcept;

  private:
    int fd_ = -1;
    MtlWorkerSharedMemoryRingMap ring_map_{};

    MtlWorkerSharedMemoryRingOwner(int fd, MtlWorkerSharedMemoryRingMap ring_map) noexcept;
};

[[nodiscard]] std::expected<MtlWorkerSharedMemoryRingDescriptor, Error>
make_mtl_worker_shared_memory_ring_descriptor(const MtlWorkerSharedMemoryRingOwnerConfig &cfg) noexcept;

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_MTL_WORKER_SHARED_MEMORY_RING_OWNER_HPP
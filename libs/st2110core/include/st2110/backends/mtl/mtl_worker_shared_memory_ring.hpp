#ifndef ST2110_OBS_PLUGIN_MTL_WORKER_SHARED_MEMORY_RING_HPP
#define ST2110_OBS_PLUGIN_MTL_WORKER_SHARED_MEMORY_RING_HPP

#include <st2110/backends/mtl/mtl_worker_protocol.hpp>
#include <st2110/foundation/error.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <type_traits>

namespace st2110 {

/*
 * Shared-memory media ring layout.
 *
 * This header contains no MTL API types. It defines the process-shared slot
 * layout and a small RAII mmap wrapper used by both sides of the future MTL
 * media data plane.
 *
 * Ownership model:
 * - fd ownership is outside this object;
 * - this object owns only the mmap mapping;
 * - slot payload ownership is coordinated through slot state transitions.
 */

inline constexpr std::uint32_t mtlWorkerSharedMemorySlotMagic = 0x4D544C53; // 'MTLS'

enum class MtlWorkerSharedMemorySlotState : std::uint32_t {
    Empty = 0,
    Writing = 1,
    Ready = 2,
    Reading = 3,
};

enum class MtlWorkerSharedMemorySlotFlags : std::uint32_t {
    None = 0,
    Partial = 1u << 0,
};

enum class MtlWorkerSharedMemoryBeginReadResult : std::uint32_t {
    Acquired = 0,
    NotReady = 1,
    Stale = 2,
};

inline constexpr std::uint32_t mtlWorkerSharedMemoryMaxPlanes = 4;

struct MtlWorkerSharedMemorySlotMediaMetadata {
    MtlWorkerMediaKind media_kind = MtlWorkerMediaKind::Video;

    /*
     * Video: static_cast<std::uint32_t>(PixelFormat).
     * Audio: static_cast<std::uint32_t>(MtlAudioPcmFormat).
     */
    std::uint32_t media_format = 0;

    std::uint32_t width = 0;
    std::uint32_t height = 0;

    std::uint32_t sample_rate_hz = 0;
    std::uint32_t channels = 0;
    std::uint32_t samples_per_channel = 0;

    std::uint32_t rtp_timestamp = 0;
    TimestampNs receive_timestamp_ns = 0;

    std::uint32_t plane_count = 0;
    std::uint32_t reserved0 = 0;

    std::uint64_t plane_offset_bytes[mtlWorkerSharedMemoryMaxPlanes]{};
    std::uint64_t plane_size_bytes[mtlWorkerSharedMemoryMaxPlanes]{};
    std::uint64_t plane_line_size_bytes[mtlWorkerSharedMemoryMaxPlanes]{};
};

static_assert(std::is_standard_layout_v<MtlWorkerSharedMemorySlotMediaMetadata>);
static_assert(std::is_trivially_copyable_v<MtlWorkerSharedMemorySlotMediaMetadata>);

/*
 * Fixed slot header stored at the beginning of every slot.
 *
 * Ready events carry routing information only. This header carries media
 * payload/layout metadata for the slot payload.
 */
struct alignas(64) MtlWorkerSharedMemorySlotHeader {
    std::uint32_t magic = mtlWorkerSharedMemorySlotMagic;
    std::uint32_t layout_version = mtlWorkerSharedMemoryRingLayoutVersion;

    std::uint32_t state = static_cast<std::uint32_t>(MtlWorkerSharedMemorySlotState::Empty);
    std::uint32_t flags = static_cast<std::uint32_t>(MtlWorkerSharedMemorySlotFlags::None);

    std::uint64_t sequence = 0;
    std::uint64_t payload_size = 0;

    MtlWorkerSharedMemorySlotMediaMetadata media{};
};

static_assert(std::is_standard_layout_v<MtlWorkerSharedMemorySlotHeader>);
static_assert(std::is_trivially_copyable_v<MtlWorkerSharedMemorySlotHeader>);

class MtlWorkerSharedMemoryRingMap final {
  public:
    MtlWorkerSharedMemoryRingMap() = default;
    ~MtlWorkerSharedMemoryRingMap();

    MtlWorkerSharedMemoryRingMap(const MtlWorkerSharedMemoryRingMap &) = delete;
    MtlWorkerSharedMemoryRingMap &operator=(const MtlWorkerSharedMemoryRingMap &) = delete;

    MtlWorkerSharedMemoryRingMap(MtlWorkerSharedMemoryRingMap &&other) noexcept;
    MtlWorkerSharedMemoryRingMap &operator=(MtlWorkerSharedMemoryRingMap &&other) noexcept;

    [[nodiscard]] static std::expected<MtlWorkerSharedMemoryRingMap, Error>
    map_from_descriptor(const MtlWorkerSharedMemoryRingDescriptor &descriptor, int fd);

    void unmap_noexcept() noexcept;

    [[nodiscard]] bool mapped() const noexcept;
    [[nodiscard]] const MtlWorkerSharedMemoryRingDescriptor &descriptor() const noexcept;

    [[nodiscard]] std::byte *data() noexcept;
    [[nodiscard]] const std::byte *data() const noexcept;
    [[nodiscard]] std::size_t size_bytes() const noexcept;

    [[nodiscard]] std::expected<MtlWorkerSharedMemorySlotHeader *, Error>
    slot_header(std::uint32_t slot_index) noexcept;
    [[nodiscard]] std::expected<const MtlWorkerSharedMemorySlotHeader *, Error>
    slot_header(std::uint32_t slot_index) const noexcept;

    [[nodiscard]] std::expected<std::span<std::byte>, Error> slot_payload(std::uint32_t slot_index) noexcept;
    [[nodiscard]] std::expected<std::span<const std::byte>, Error>
    slot_payload(std::uint32_t slot_index) const noexcept;

    [[nodiscard]] std::expected<bool, Error> initialize_slot_headers() noexcept;
    [[nodiscard]] std::expected<bool, Error> validate_initialized_slot_headers() const noexcept;

    [[nodiscard]] std::expected<MtlWorkerSharedMemorySlotState, Error>
    load_slot_state(std::uint32_t slot_index) const noexcept;

    [[nodiscard]] std::expected<bool, Error> begin_write_slot(std::uint32_t slot_index) noexcept;

    [[nodiscard]] std::expected<bool, Error>
    publish_written_slot(std::uint32_t slot_index, std::uint64_t payload_size, std::uint64_t sequence,
                         std::uint32_t flags, const MtlWorkerSharedMemorySlotMediaMetadata &metadata) noexcept;

    [[nodiscard]] std::expected<bool, Error> abort_write_slot(std::uint32_t slot_index) noexcept;

    [[nodiscard]] std::expected<bool, Error> begin_read_slot(std::uint32_t slot_index) noexcept;

    [[nodiscard]] std::expected<MtlWorkerSharedMemoryBeginReadResult, Error>
    begin_read_slot_if_matches(std::uint32_t slot_index, std::uint64_t expected_sequence) noexcept;

    [[nodiscard]] std::expected<bool, Error> release_read_slot(std::uint32_t slot_index) noexcept;

  private:
    MtlWorkerSharedMemoryRingDescriptor descriptor_{};
    std::byte *mapping_ = nullptr;
    std::size_t mapping_size_ = 0;

    MtlWorkerSharedMemoryRingMap(MtlWorkerSharedMemoryRingDescriptor descriptor, std::byte *mapping,
                                 std::size_t mapping_size) noexcept;
};

[[nodiscard]] std::expected<bool, Error> validate_mtl_worker_shared_memory_ring_descriptor_for_mapping(
    const MtlWorkerSharedMemoryRingDescriptor &descriptor) noexcept;

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_MTL_WORKER_SHARED_MEMORY_RING_HPP
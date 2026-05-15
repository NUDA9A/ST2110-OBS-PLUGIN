#include <st2110/backends/mtl/mtl_worker_shared_memory_ring_owner.hpp>

#include <fcntl.h>
#include <linux/memfd.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

namespace st2110 {
namespace {

inline constexpr std::uint64_t mtlWorkerSharedMemoryRingAlignment = alignof(MtlWorkerSharedMemorySlotHeader);

void close_fd_noexcept(int &fd) noexcept {
    if (fd >= 0) {
        (void)::close(fd);
        fd = -1;
    }
}

[[nodiscard]] std::expected<std::uint64_t, Error> align_up_u64(const std::uint64_t value,
                                                               const std::uint64_t alignment) noexcept {
    if (alignment == 0) {
        return std::unexpected(Error::InvalidValue);
    }

    const std::uint64_t remainder = value % alignment;
    if (remainder == 0) {
        return value;
    }

    const std::uint64_t increment = alignment - remainder;
    if (std::numeric_limits<std::uint64_t>::max() - value < increment) {
        return std::unexpected(Error::InvalidValue);
    }

    return value + increment;
}

[[nodiscard]] std::expected<std::uint64_t, Error> add_u64(const std::uint64_t a, const std::uint64_t b) noexcept {
    if (std::numeric_limits<std::uint64_t>::max() - a < b) {
        return std::unexpected(Error::InvalidValue);
    }

    return a + b;
}

[[nodiscard]] std::expected<std::uint64_t, Error> mul_u64(const std::uint64_t a, const std::uint64_t b) noexcept {
    if (a != 0 && b > std::numeric_limits<std::uint64_t>::max() / a) {
        return std::unexpected(Error::InvalidValue);
    }

    return a * b;
}

[[nodiscard]] std::expected<off_t, Error> checked_off_t(const std::uint64_t value) noexcept {
    if (value > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max())) {
        return std::unexpected(Error::InvalidValue);
    }

    return static_cast<off_t>(value);
}

[[nodiscard]] std::expected<int, Error> create_memfd(const std::string &debug_name) noexcept {
    const char *name = debug_name.empty() ? "st2110_mtl_ring" : debug_name.c_str();

    const int fd = static_cast<int>(::syscall(SYS_memfd_create, name, MFD_CLOEXEC));
    if (fd < 0) {
        return std::unexpected(Error::SystemFailure);
    }

    return fd;
}

[[nodiscard]] std::expected<bool, Error> truncate_fd(const int fd, const std::uint64_t size_bytes) noexcept {
    if (fd < 0 || size_bytes == 0) {
        return std::unexpected(Error::InvalidValue);
    }

    auto size = checked_off_t(size_bytes);
    if (!size.has_value()) {
        return std::unexpected(size.error());
    }

    if (::ftruncate(fd, *size) != 0) {
        return std::unexpected(Error::SystemFailure);
    }

    return true;
}

} // namespace

std::expected<MtlWorkerSharedMemoryRingDescriptor, Error>
make_mtl_worker_shared_memory_ring_descriptor(const MtlWorkerSharedMemoryRingOwnerConfig &cfg) noexcept {
    if (cfg.ring_id == 0 || cfg.slot_count == 0 || cfg.slot_payload_capacity_bytes == 0) {
        return std::unexpected(Error::InvalidValue);
    }

    switch (cfg.media_kind) {
    case MtlWorkerMediaKind::Video:
    case MtlWorkerMediaKind::Audio:
        break;
    default:
        return std::unexpected(Error::InvalidValue);
    }

    auto payload_offset = align_up_u64(sizeof(MtlWorkerSharedMemorySlotHeader), mtlWorkerSharedMemoryRingAlignment);
    if (!payload_offset.has_value()) {
        return std::unexpected(payload_offset.error());
    }

    auto payload_end = add_u64(*payload_offset, cfg.slot_payload_capacity_bytes);
    if (!payload_end.has_value()) {
        return std::unexpected(payload_end.error());
    }

    auto slot_stride = align_up_u64(*payload_end, mtlWorkerSharedMemoryRingAlignment);
    if (!slot_stride.has_value()) {
        return std::unexpected(slot_stride.error());
    }

    auto mapped_size = mul_u64(static_cast<std::uint64_t>(cfg.slot_count), *slot_stride);
    if (!mapped_size.has_value()) {
        return std::unexpected(mapped_size.error());
    }

    MtlWorkerSharedMemoryRingDescriptor descriptor{
        .ring_id = cfg.ring_id,
        .media_kind = cfg.media_kind,
        .fd_index = cfg.fd_index,
        .layout_version = mtlWorkerSharedMemoryRingLayoutVersion,
        .mapped_size_bytes = *mapped_size,
        .slot_region_offset_bytes = 0,
        .slot_count = cfg.slot_count,
        .slot_stride_bytes = *slot_stride,
        .slot_payload_offset_bytes = *payload_offset,
        .slot_payload_capacity_bytes = cfg.slot_payload_capacity_bytes,
    };

    auto valid = validate_mtl_worker_shared_memory_ring_descriptor_for_mapping(descriptor);
    if (!valid.has_value()) {
        return std::unexpected(valid.error());
    }

    return descriptor;
}

MtlWorkerSharedMemoryRingOwner::MtlWorkerSharedMemoryRingOwner(int fd, MtlWorkerSharedMemoryRingMap ring_map) noexcept
    : fd_(fd), ring_map_(std::move(ring_map)) {}

MtlWorkerSharedMemoryRingOwner::~MtlWorkerSharedMemoryRingOwner() { close_noexcept(); }

MtlWorkerSharedMemoryRingOwner::MtlWorkerSharedMemoryRingOwner(MtlWorkerSharedMemoryRingOwner &&other) noexcept
    : fd_(std::exchange(other.fd_, -1)), ring_map_(std::exchange(other.ring_map_, MtlWorkerSharedMemoryRingMap{})) {}

MtlWorkerSharedMemoryRingOwner &
MtlWorkerSharedMemoryRingOwner::operator=(MtlWorkerSharedMemoryRingOwner &&other) noexcept {
    if (this == &other) {
        return *this;
    }

    close_noexcept();

    fd_ = std::exchange(other.fd_, -1);
    ring_map_ = std::exchange(other.ring_map_, MtlWorkerSharedMemoryRingMap{});

    return *this;
}

std::expected<MtlWorkerSharedMemoryRingOwner, Error>
MtlWorkerSharedMemoryRingOwner::create(const MtlWorkerSharedMemoryRingOwnerConfig &cfg) {
    auto descriptor = make_mtl_worker_shared_memory_ring_descriptor(cfg);
    if (!descriptor.has_value()) {
        return std::unexpected(descriptor.error());
    }

    auto fd = create_memfd(cfg.debug_name);
    if (!fd.has_value()) {
        return std::unexpected(fd.error());
    }

    int owned_fd = *fd;

    auto truncated = truncate_fd(owned_fd, descriptor->mapped_size_bytes);
    if (!truncated.has_value()) {
        close_fd_noexcept(owned_fd);
        return std::unexpected(truncated.error());
    }

    auto map = MtlWorkerSharedMemoryRingMap::map_from_descriptor(*descriptor, owned_fd);
    if (!map.has_value()) {
        close_fd_noexcept(owned_fd);
        return std::unexpected(map.error());
    }

    auto initialized = map->initialize_slot_headers();
    if (!initialized.has_value()) {
        close_fd_noexcept(owned_fd);
        return std::unexpected(initialized.error());
    }

    return MtlWorkerSharedMemoryRingOwner{owned_fd, std::move(*map)};
}

void MtlWorkerSharedMemoryRingOwner::close_noexcept() noexcept {
    ring_map_.unmap_noexcept();
    close_fd_noexcept(fd_);
}

bool MtlWorkerSharedMemoryRingOwner::valid() const noexcept { return fd_ >= 0 && ring_map_.mapped(); }

int MtlWorkerSharedMemoryRingOwner::fd() const noexcept { return fd_; }

const MtlWorkerSharedMemoryRingDescriptor &MtlWorkerSharedMemoryRingOwner::descriptor() const noexcept {
    return ring_map_.descriptor();
}

MtlWorkerSharedMemoryRingMap &MtlWorkerSharedMemoryRingOwner::ring_map() noexcept { return ring_map_; }

const MtlWorkerSharedMemoryRingMap &MtlWorkerSharedMemoryRingOwner::ring_map() const noexcept { return ring_map_; }

} // namespace st2110
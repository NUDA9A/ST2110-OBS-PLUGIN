#include <st2110/backends/mtl/mtl_worker_shared_memory_ring.hpp>

#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace st2110 {
namespace {

[[nodiscard]] std::expected<std::size_t, Error> checked_size_t(const std::uint64_t value) noexcept {
    if (value > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        return std::unexpected(Error::InvalidValue);
    }

    return static_cast<std::size_t>(value);
}

[[nodiscard]] bool add_overflows_u64(const std::uint64_t a, const std::uint64_t b, std::uint64_t &out) noexcept {
    if (std::numeric_limits<std::uint64_t>::max() - a < b) {
        return true;
    }

    out = a + b;
    return false;
}

[[nodiscard]] bool mul_overflows_u64(const std::uint64_t a, const std::uint64_t b, std::uint64_t &out) noexcept {
    if (a != 0 && b > std::numeric_limits<std::uint64_t>::max() / a) {
        return true;
    }

    out = a * b;
    return false;
}

[[nodiscard]] bool valid_slot_state_value(const std::uint32_t value) noexcept {
    switch (static_cast<MtlWorkerSharedMemorySlotState>(value)) {
    case MtlWorkerSharedMemorySlotState::Empty:
    case MtlWorkerSharedMemorySlotState::Writing:
    case MtlWorkerSharedMemorySlotState::Ready:
    case MtlWorkerSharedMemorySlotState::Reading:
        return true;
    }

    return false;
}

[[nodiscard]] std::expected<bool, Error> validate_fd_size(const int fd,
                                                          const std::uint64_t required_size_bytes) noexcept {
    if (fd < 0) {
        return std::unexpected(Error::InvalidValue);
    }

    struct stat st{};
    if (::fstat(fd, &st) != 0) {
        return std::unexpected(Error::SystemFailure);
    }

    if (st.st_size < 0 || static_cast<std::uint64_t>(st.st_size) < required_size_bytes) {
        return std::unexpected(Error::InvalidValue);
    }

    return true;
}

} // namespace

std::expected<bool, Error> validate_mtl_worker_shared_memory_ring_descriptor_for_mapping(
    const MtlWorkerSharedMemoryRingDescriptor &descriptor) noexcept {
    if (descriptor.ring_id == 0 || descriptor.layout_version != mtlWorkerSharedMemoryRingLayoutVersion ||
        descriptor.mapped_size_bytes == 0 || descriptor.slot_count == 0 || descriptor.slot_stride_bytes == 0 ||
        descriptor.slot_payload_capacity_bytes == 0) {
        return std::unexpected(Error::InvalidValue);
    }

    switch (descriptor.media_kind) {
    case MtlWorkerMediaKind::Video:
    case MtlWorkerMediaKind::Audio:
        break;
    default:
        return std::unexpected(Error::InvalidValue);
    }

    if ((descriptor.slot_region_offset_bytes % alignof(MtlWorkerSharedMemorySlotHeader)) != 0 ||
        (descriptor.slot_stride_bytes % alignof(MtlWorkerSharedMemorySlotHeader)) != 0) {
        return std::unexpected(Error::InvalidValue);
    }

    if (descriptor.slot_payload_offset_bytes < sizeof(MtlWorkerSharedMemorySlotHeader)) {
        return std::unexpected(Error::InvalidValue);
    }

    std::uint64_t payload_end_within_slot = 0;
    if (add_overflows_u64(descriptor.slot_payload_offset_bytes, descriptor.slot_payload_capacity_bytes,
                          payload_end_within_slot)) {
        return std::unexpected(Error::InvalidValue);
    }

    if (payload_end_within_slot > descriptor.slot_stride_bytes) {
        return std::unexpected(Error::InvalidValue);
    }

    std::uint64_t last_slot_relative_offset = 0;
    if (mul_overflows_u64(static_cast<std::uint64_t>(descriptor.slot_count - 1), descriptor.slot_stride_bytes,
                          last_slot_relative_offset)) {
        return std::unexpected(Error::InvalidValue);
    }

    std::uint64_t last_slot_payload_end = 0;
    if (add_overflows_u64(last_slot_relative_offset, payload_end_within_slot, last_slot_payload_end)) {
        return std::unexpected(Error::InvalidValue);
    }

    std::uint64_t mapped_payload_end = 0;
    if (add_overflows_u64(descriptor.slot_region_offset_bytes, last_slot_payload_end, mapped_payload_end)) {
        return std::unexpected(Error::InvalidValue);
    }

    if (mapped_payload_end > descriptor.mapped_size_bytes) {
        return std::unexpected(Error::InvalidValue);
    }

    return checked_size_t(descriptor.mapped_size_bytes).has_value() ? std::expected<bool, Error>{true}
                                                                    : std::unexpected(Error::InvalidValue);
}

MtlWorkerSharedMemoryRingMap::MtlWorkerSharedMemoryRingMap(MtlWorkerSharedMemoryRingDescriptor descriptor,
                                                           std::byte *mapping, const std::size_t mapping_size) noexcept
    : descriptor_(std::move(descriptor)), mapping_(mapping), mapping_size_(mapping_size) {}

MtlWorkerSharedMemoryRingMap::~MtlWorkerSharedMemoryRingMap() { unmap_noexcept(); }

MtlWorkerSharedMemoryRingMap::MtlWorkerSharedMemoryRingMap(MtlWorkerSharedMemoryRingMap &&other) noexcept
    : descriptor_(std::exchange(other.descriptor_, {})), mapping_(std::exchange(other.mapping_, nullptr)),
      mapping_size_(std::exchange(other.mapping_size_, 0)) {}

MtlWorkerSharedMemoryRingMap &MtlWorkerSharedMemoryRingMap::operator=(MtlWorkerSharedMemoryRingMap &&other) noexcept {
    if (this == &other) {
        return *this;
    }

    unmap_noexcept();

    descriptor_ = std::exchange(other.descriptor_, {});
    mapping_ = std::exchange(other.mapping_, nullptr);
    mapping_size_ = std::exchange(other.mapping_size_, 0);

    return *this;
}

std::expected<MtlWorkerSharedMemoryRingMap, Error>
MtlWorkerSharedMemoryRingMap::map_from_descriptor(const MtlWorkerSharedMemoryRingDescriptor &descriptor, const int fd) {
    auto valid_descriptor = validate_mtl_worker_shared_memory_ring_descriptor_for_mapping(descriptor);
    if (!valid_descriptor.has_value()) {
        return std::unexpected(valid_descriptor.error());
    }

    auto mapping_size = checked_size_t(descriptor.mapped_size_bytes);
    if (!mapping_size.has_value()) {
        return std::unexpected(mapping_size.error());
    }

    auto valid_fd = validate_fd_size(fd, descriptor.mapped_size_bytes);
    if (!valid_fd.has_value()) {
        return std::unexpected(valid_fd.error());
    }

    void *mapping = ::mmap(nullptr, *mapping_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapping == MAP_FAILED) {
        return std::unexpected(Error::SystemFailure);
    }

    return MtlWorkerSharedMemoryRingMap{
        descriptor,
        static_cast<std::byte *>(mapping),
        *mapping_size,
    };
}

void MtlWorkerSharedMemoryRingMap::unmap_noexcept() noexcept {
    if (mapping_) {
        (void)::munmap(mapping_, mapping_size_);
        mapping_ = nullptr;
        mapping_size_ = 0;
        descriptor_ = MtlWorkerSharedMemoryRingDescriptor{};
    }
}

bool MtlWorkerSharedMemoryRingMap::mapped() const noexcept { return mapping_ != nullptr; }

const MtlWorkerSharedMemoryRingDescriptor &MtlWorkerSharedMemoryRingMap::descriptor() const noexcept {
    return descriptor_;
}

std::byte *MtlWorkerSharedMemoryRingMap::data() noexcept { return mapping_; }

const std::byte *MtlWorkerSharedMemoryRingMap::data() const noexcept { return mapping_; }

std::size_t MtlWorkerSharedMemoryRingMap::size_bytes() const noexcept { return mapping_size_; }

std::expected<MtlWorkerSharedMemorySlotHeader *, Error>
MtlWorkerSharedMemoryRingMap::slot_header(const std::uint32_t slot_index) noexcept {
    if (!mapping_ || slot_index >= descriptor_.slot_count) {
        return std::unexpected(Error::InvalidValue);
    }

    std::uint64_t slot_offset = 0;
    if (mul_overflows_u64(static_cast<std::uint64_t>(slot_index), descriptor_.slot_stride_bytes, slot_offset)) {
        return std::unexpected(Error::InvalidValue);
    }

    if (add_overflows_u64(descriptor_.slot_region_offset_bytes, slot_offset, slot_offset)) {
        return std::unexpected(Error::InvalidValue);
    }

    auto offset = checked_size_t(slot_offset);
    if (!offset.has_value() || *offset + sizeof(MtlWorkerSharedMemorySlotHeader) > mapping_size_) {
        return std::unexpected(Error::InvalidValue);
    }

    return reinterpret_cast<MtlWorkerSharedMemorySlotHeader *>(mapping_ + *offset);
}

std::expected<const MtlWorkerSharedMemorySlotHeader *, Error>
MtlWorkerSharedMemoryRingMap::slot_header(const std::uint32_t slot_index) const noexcept {
    return const_cast<MtlWorkerSharedMemoryRingMap *>(this)->slot_header(slot_index);
}

std::expected<std::span<std::byte>, Error>
MtlWorkerSharedMemoryRingMap::slot_payload(const std::uint32_t slot_index) noexcept {
    if (!mapping_ || slot_index >= descriptor_.slot_count) {
        return std::unexpected(Error::InvalidValue);
    }

    std::uint64_t slot_offset = 0;
    if (mul_overflows_u64(static_cast<std::uint64_t>(slot_index), descriptor_.slot_stride_bytes, slot_offset)) {
        return std::unexpected(Error::InvalidValue);
    }

    std::uint64_t payload_offset = 0;
    if (add_overflows_u64(descriptor_.slot_region_offset_bytes, slot_offset, payload_offset) ||
        add_overflows_u64(payload_offset, descriptor_.slot_payload_offset_bytes, payload_offset)) {
        return std::unexpected(Error::InvalidValue);
    }

    auto offset = checked_size_t(payload_offset);
    auto capacity = checked_size_t(descriptor_.slot_payload_capacity_bytes);

    if (!offset.has_value() || !capacity.has_value() || *offset + *capacity > mapping_size_) {
        return std::unexpected(Error::InvalidValue);
    }

    return std::span<std::byte>{mapping_ + *offset, *capacity};
}

std::expected<std::span<const std::byte>, Error>
MtlWorkerSharedMemoryRingMap::slot_payload(const std::uint32_t slot_index) const noexcept {
    auto payload = const_cast<MtlWorkerSharedMemoryRingMap *>(this)->slot_payload(slot_index);
    if (!payload.has_value()) {
        return std::unexpected(payload.error());
    }

    return std::span<const std::byte>{payload->data(), payload->size()};
}

std::expected<bool, Error> MtlWorkerSharedMemoryRingMap::initialize_slot_headers() noexcept {
    if (!mapping_) {
        return std::unexpected(Error::InvalidBackendState);
    }

    for (std::uint32_t i = 0; i < descriptor_.slot_count; ++i) {
        auto header = slot_header(i);
        if (!header.has_value()) {
            return std::unexpected(header.error());
        }

        (*header)->magic = mtlWorkerSharedMemorySlotMagic;
        (*header)->layout_version = mtlWorkerSharedMemoryRingLayoutVersion;
        (*header)->flags = static_cast<std::uint32_t>(MtlWorkerSharedMemorySlotFlags::None);
        (*header)->sequence = 0;
        (*header)->payload_size = 0;
        (*header)->reserved0 = 0;
        (*header)->reserved1 = 0;

        std::atomic_ref<std::uint32_t> state_ref((*header)->state);
        state_ref.store(static_cast<std::uint32_t>(MtlWorkerSharedMemorySlotState::Empty), std::memory_order_release);
    }

    return true;
}

std::expected<bool, Error> MtlWorkerSharedMemoryRingMap::validate_initialized_slot_headers() const noexcept {
    if (!mapping_) {
        return std::unexpected(Error::InvalidBackendState);
    }

    for (std::uint32_t i = 0; i < descriptor_.slot_count; ++i) {
        auto header = slot_header(i);
        if (!header.has_value()) {
            return std::unexpected(header.error());
        }

        if ((*header)->magic != mtlWorkerSharedMemorySlotMagic ||
            (*header)->layout_version != mtlWorkerSharedMemoryRingLayoutVersion ||
            !valid_slot_state_value(
                std::atomic_ref<const std::uint32_t>((*header)->state).load(std::memory_order_acquire))) {
            return std::unexpected(Error::InvalidValue);
        }
    }

    return true;
}

std::expected<MtlWorkerSharedMemorySlotState, Error>
MtlWorkerSharedMemoryRingMap::load_slot_state(const std::uint32_t slot_index) const noexcept {
    auto header = slot_header(slot_index);
    if (!header.has_value()) {
        return std::unexpected(header.error());
    }

    const std::uint32_t state = std::atomic_ref<const std::uint32_t>((*header)->state).load(std::memory_order_acquire);

    if (!valid_slot_state_value(state)) {
        return std::unexpected(Error::InvalidValue);
    }

    return static_cast<MtlWorkerSharedMemorySlotState>(state);
}

std::expected<bool, Error> MtlWorkerSharedMemoryRingMap::begin_write_slot(const std::uint32_t slot_index) noexcept {
    auto header = slot_header(slot_index);
    if (!header.has_value()) {
        return std::unexpected(header.error());
    }

    std::uint32_t expected = static_cast<std::uint32_t>(MtlWorkerSharedMemorySlotState::Empty);
    std::atomic_ref<std::uint32_t> state_ref((*header)->state);

    return state_ref.compare_exchange_strong(expected,
                                             static_cast<std::uint32_t>(MtlWorkerSharedMemorySlotState::Writing),
                                             std::memory_order_acq_rel, std::memory_order_acquire);
}

std::expected<bool, Error> MtlWorkerSharedMemoryRingMap::publish_written_slot(const std::uint32_t slot_index,
                                                                              const std::uint64_t payload_size,
                                                                              const std::uint64_t sequence,
                                                                              const std::uint32_t flags) noexcept {
    if (payload_size > descriptor_.slot_payload_capacity_bytes) {
        return std::unexpected(Error::InvalidValue);
    }

    auto header = slot_header(slot_index);
    if (!header.has_value()) {
        return std::unexpected(header.error());
    }

    auto state = load_slot_state(slot_index);
    if (!state.has_value()) {
        return std::unexpected(state.error());
    }

    if (*state != MtlWorkerSharedMemorySlotState::Writing) {
        return std::unexpected(Error::InvalidBackendState);
    }

    (*header)->payload_size = payload_size;
    (*header)->sequence = sequence;
    (*header)->flags = flags;

    std::atomic_ref<std::uint32_t> state_ref((*header)->state);
    state_ref.store(static_cast<std::uint32_t>(MtlWorkerSharedMemorySlotState::Ready), std::memory_order_release);

    return true;
}

std::expected<bool, Error> MtlWorkerSharedMemoryRingMap::abort_write_slot(const std::uint32_t slot_index) noexcept {
    auto header = slot_header(slot_index);
    if (!header.has_value()) {
        return std::unexpected(header.error());
    }

    auto state = load_slot_state(slot_index);
    if (!state.has_value()) {
        return std::unexpected(state.error());
    }

    if (*state != MtlWorkerSharedMemorySlotState::Writing) {
        return false;
    }

    (*header)->payload_size = 0;
    (*header)->flags = static_cast<std::uint32_t>(MtlWorkerSharedMemorySlotFlags::None);

    std::atomic_ref<std::uint32_t> state_ref((*header)->state);
    state_ref.store(static_cast<std::uint32_t>(MtlWorkerSharedMemorySlotState::Empty), std::memory_order_release);

    return true;
}

std::expected<bool, Error> MtlWorkerSharedMemoryRingMap::begin_read_slot(const std::uint32_t slot_index) noexcept {
    auto header = slot_header(slot_index);
    if (!header.has_value()) {
        return std::unexpected(header.error());
    }

    std::uint32_t expected = static_cast<std::uint32_t>(MtlWorkerSharedMemorySlotState::Ready);
    std::atomic_ref<std::uint32_t> state_ref((*header)->state);

    return state_ref.compare_exchange_strong(expected,
                                             static_cast<std::uint32_t>(MtlWorkerSharedMemorySlotState::Reading),
                                             std::memory_order_acq_rel, std::memory_order_acquire);
}

std::expected<MtlWorkerSharedMemoryBeginReadResult, Error>
MtlWorkerSharedMemoryRingMap::begin_read_slot_if_matches(const std::uint32_t slot_index,
                                                         const std::uint64_t expected_sequence,
                                                         const std::uint64_t expected_payload_size) noexcept {
    auto header = slot_header(slot_index);
    if (!header.has_value()) {
        return std::unexpected(header.error());
    }

    std::atomic_ref<std::uint32_t> state_ref((*header)->state);

    const std::uint32_t loaded_state = state_ref.load(std::memory_order_acquire);
    if (!valid_slot_state_value(loaded_state)) {
        return std::unexpected(Error::InvalidValue);
    }

    if (static_cast<MtlWorkerSharedMemorySlotState>(loaded_state) != MtlWorkerSharedMemorySlotState::Ready) {
        return MtlWorkerSharedMemoryBeginReadResult::NotReady;
    }

    if ((*header)->sequence != expected_sequence || (*header)->payload_size != expected_payload_size) {
        return MtlWorkerSharedMemoryBeginReadResult::Stale;
    }

    std::uint32_t expected = static_cast<std::uint32_t>(MtlWorkerSharedMemorySlotState::Ready);
    if (!state_ref.compare_exchange_strong(expected,
                                           static_cast<std::uint32_t>(MtlWorkerSharedMemorySlotState::Reading),
                                           std::memory_order_acq_rel, std::memory_order_acquire)) {
        if (!valid_slot_state_value(expected)) {
            return std::unexpected(Error::InvalidValue);
        }

        return MtlWorkerSharedMemoryBeginReadResult::NotReady;
    }

    /*
     * After the successful Ready -> Reading transition, the worker cannot reuse
     * this slot until OBS releases it. Re-check the fields to guard against
     * corrupted shared memory.
     */
    if ((*header)->sequence != expected_sequence || (*header)->payload_size != expected_payload_size) {
        return std::unexpected(Error::InvalidValue);
    }

    return MtlWorkerSharedMemoryBeginReadResult::Acquired;
}

std::expected<bool, Error> MtlWorkerSharedMemoryRingMap::release_read_slot(const std::uint32_t slot_index) noexcept {
    auto header = slot_header(slot_index);
    if (!header.has_value()) {
        return std::unexpected(header.error());
    }

    auto state = load_slot_state(slot_index);
    if (!state.has_value()) {
        return std::unexpected(state.error());
    }

    if (*state != MtlWorkerSharedMemorySlotState::Reading) {
        return std::unexpected(Error::InvalidBackendState);
    }

    (*header)->payload_size = 0;
    (*header)->flags = static_cast<std::uint32_t>(MtlWorkerSharedMemorySlotFlags::None);

    std::atomic_ref<std::uint32_t> state_ref((*header)->state);
    state_ref.store(static_cast<std::uint32_t>(MtlWorkerSharedMemorySlotState::Empty), std::memory_order_release);

    return true;
}

} // namespace st2110
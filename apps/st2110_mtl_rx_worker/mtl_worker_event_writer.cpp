#include "mtl_worker_event_writer.hpp"

#include <st2110/backends/mtl/mtl_worker_ipc_codec.hpp>
#include <st2110/backends/mtl/mtl_worker_ipc_framing.hpp>

#include <mutex>

namespace st2110_mtl_rx_worker {

MtlWorkerEventWriter::MtlWorkerEventWriter(const int fd) noexcept : fd_(fd) {}

std::expected<bool, st2110::Error> MtlWorkerEventWriter::write_event(const st2110::MtlWorkerControlEvent &event) {
    if (fd_ < 0) {
        return std::unexpected(st2110::Error::InvalidValue);
    }

    auto payload = st2110::serialize_mtl_worker_control_event(event);
    if (!payload.has_value()) {
        return std::unexpected(payload.error());
    }

    std::lock_guard lock(write_mutex_);

    auto wrote = st2110::write_mtl_worker_control_frame(fd_, *payload);
    if (!wrote.has_value()) {
        return std::unexpected(wrote.error());
    }

    return true;
}

int MtlWorkerEventWriter::fd() const noexcept { return fd_; }

} // namespace st2110_mtl_rx_worker
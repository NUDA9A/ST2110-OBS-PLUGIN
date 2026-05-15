#include <st2110/backends/mtl/mtl_worker_manager.hpp>

#include <st2110/backends/mtl/mtl_worker_process_control_channel.hpp>

#include <utility>

namespace st2110 {

struct MtlWorkerManager::Impl {
    std::filesystem::path worker_executable_path = "st2110_mtl_rx_worker";

    std::uint64_t next_worker_id = 1;
    bool has_cached_worker = false;
    WorkerLease cached_worker{};
};

MtlWorkerManager::MtlWorkerManager() : impl_(std::make_unique<Impl>()) {}
MtlWorkerManager::MtlWorkerManager(std::filesystem::path worker_executable_path) : impl_(std::make_unique<Impl>()) {
    impl_->worker_executable_path = std::move(worker_executable_path);
}

MtlWorkerManager::~MtlWorkerManager() { shutdown_all_workers_noexcept(); }

std::expected<MtlWorkerManager::WorkerLease, Error>
MtlWorkerManager::acquire_or_spawn_compatible_worker(const MtlRuntimeConfig &runtime) {
    /*
     * Process spawn and IPC are intentionally not implemented yet.
     *
     * This skeleton models only the compatibility cache:
     * - same runtime config reuses the same logical worker lease;
     * - incompatible runtime config replaces the cached logical worker lease.
     *
     * A future implementation will shutdown the old process before replacing it
     * and will spawn/connect to a real worker process here.
     */
    if (impl_->has_cached_worker && impl_->cached_worker.runtime == runtime) {
        return impl_->cached_worker;
    }

    impl_->cached_worker = WorkerLease{
        .worker_id = impl_->next_worker_id++,
        .runtime = runtime,
        .control_channel = create_mtl_worker_process_control_channel(impl_->worker_executable_path),
    };
    impl_->has_cached_worker = true;

    return impl_->cached_worker;
}

void MtlWorkerManager::shutdown_all_workers_noexcept() noexcept {
    impl_->has_cached_worker = false;
    impl_->cached_worker = WorkerLease{};
}

MtlWorkerManager &default_mtl_worker_manager() {
    static MtlWorkerManager manager;
    return manager;
}

const std::filesystem::path &MtlWorkerManager::worker_executable_path() const noexcept {
    return impl_->worker_executable_path;
}

} // namespace st2110
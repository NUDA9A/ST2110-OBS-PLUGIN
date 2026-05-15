#include <st2110/backends/mtl/mtl_worker_manager.hpp>

#include <st2110/backends/mtl/mtl_worker_process_control_channel.hpp>

#include <atomic>
#include <type_traits>
#include <utility>
#include <variant>

namespace st2110 {

namespace {

[[nodiscard]] MtlWorkerRequestId next_worker_request_id() noexcept {
    static std::atomic<MtlWorkerRequestId> next{1};
    return next.fetch_add(1, std::memory_order_relaxed);
}

[[nodiscard]] std::expected<bool, Error>
interpret_config_handshake_event(const MtlWorkerControlEvent &event, const MtlWorkerRequestId expected_request_id) {
    return std::visit(
        [expected_request_id](const auto &typed_event) -> std::expected<bool, Error> {
            using Event = std::decay_t<decltype(typed_event)>;

            if constexpr (std::is_same_v<Event, MtlWorkerHealthEvent>) {
                if (typed_event.request_id != expected_request_id) {
                    return std::unexpected(Error::InvalidBackendState);
                }

                return typed_event.healthy ? std::expected<bool, Error>{true}
                                           : std::unexpected(Error::InvalidBackendState);
            } else if constexpr (std::is_same_v<Event, MtlWorkerErrorEvent>) {
                if (typed_event.request_id != expected_request_id) {
                    return std::unexpected(Error::InvalidBackendState);
                }

                return std::unexpected(typed_event.error);
            } else {
                return std::unexpected(Error::InvalidBackendState);
            }
        },
        event);
}

void shutdown_worker_lease_noexcept(const MtlWorkerManager::WorkerLease &lease) noexcept {
    if (!lease.control_channel) {
        return;
    }

    try {
        (void)lease.control_channel->transact(MtlWorkerControlRequest{
            MtlWorkerShutdownRequest{
                .request_id = next_worker_request_id(),
            },
        });
    } catch (...) {
        /*
         * shutdown_all_workers_noexcept() must remain noexcept.
         * Process-control implementations may later throw from lower-level IPC
         * wrappers; shutdown must still continue local cleanup.
         */
    }
}

} // namespace

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
    if (impl_->has_cached_worker && impl_->cached_worker.runtime == runtime) {
        return impl_->cached_worker;
    }

    if (impl_->has_cached_worker) {
        /*
         * Current skeleton has a single cached logical worker.
         *
         * Future implementation point:
         * - send ShutdownWorker to the incompatible old worker;
         * - wait/reap process;
         * - then spawn/connect a new compatible worker.
         */
        shutdown_all_workers_noexcept();
    }

    auto control_channel = create_mtl_worker_process_control_channel(impl_->worker_executable_path);
    if (!control_channel) {
        return std::unexpected(Error::InvalidBackendState);
    }

    const MtlWorkerRequestId request_id = next_worker_request_id();

    auto event = control_channel->transact(MtlWorkerControlRequest{
        MtlWorkerConfigHandshakeRequest{
            .request_id = request_id,
            .runtime = runtime,
        },
    });
    if (!event.has_value()) {
        return std::unexpected(event.error());
    }

    auto accepted = interpret_config_handshake_event(*event, request_id);
    if (!accepted.has_value()) {
        return std::unexpected(accepted.error());
    }

    impl_->cached_worker = WorkerLease{
        .worker_id = impl_->next_worker_id++,
        .runtime = runtime,
        .control_channel = std::move(control_channel),
    };
    impl_->has_cached_worker = true;

    return impl_->cached_worker;
}

void MtlWorkerManager::shutdown_all_workers_noexcept() noexcept {
    if (impl_->has_cached_worker) {
        shutdown_worker_lease_noexcept(impl_->cached_worker);
    }

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
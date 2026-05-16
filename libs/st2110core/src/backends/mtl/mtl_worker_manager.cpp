#include <st2110/backends/mtl/mtl_worker_manager.hpp>

#include <st2110/backends/mtl/mtl_worker_process_control_channel.hpp>

#include <atomic>
#include <mutex>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

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
         */
    }
}

[[nodiscard]] bool worker_matches_runtime(const MtlWorkerManager::WorkerLease &lease,
                                          const MtlRuntimeConfig &runtime) noexcept {
    return lease.control_channel && lease.runtime == runtime;
}

} // namespace

struct MtlWorkerManager::Impl {
    std::filesystem::path worker_executable_path = "st2110_mtl_rx_worker";

    std::mutex mutex{};
    std::uint64_t next_worker_id = 1;
    std::vector<WorkerLease> workers{};
};

MtlWorkerManager::MtlWorkerManager() : impl_(std::make_unique<Impl>()) {}
MtlWorkerManager::MtlWorkerManager(std::filesystem::path worker_executable_path) : impl_(std::make_unique<Impl>()) {
    impl_->worker_executable_path = std::move(worker_executable_path);
}

MtlWorkerManager::~MtlWorkerManager() { shutdown_all_workers_noexcept(); }

std::expected<MtlWorkerManager::WorkerLease, Error>
MtlWorkerManager::acquire_or_spawn_compatible_worker(const MtlRuntimeConfig &runtime) {
    std::lock_guard lock(impl_->mutex);

    for (const WorkerLease &worker : impl_->workers) {
        if (worker_matches_runtime(worker, runtime)) {
            return worker;
        }
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

    WorkerLease worker{
        .worker_id = impl_->next_worker_id++,
        .runtime = runtime,
        .control_channel = std::move(control_channel),
    };

    impl_->workers.push_back(worker);
    return worker;
}


void MtlWorkerManager::shutdown_all_workers_noexcept() noexcept {
    std::vector<WorkerLease> workers_to_shutdown{};

    try {
        std::lock_guard lock(impl_->mutex);
        workers_to_shutdown = std::move(impl_->workers);
        impl_->workers.clear();
    } catch (...) {
        return;
    }

    for (const WorkerLease &worker : workers_to_shutdown) {
        shutdown_worker_lease_noexcept(worker);
    }
}

MtlWorkerManager &default_mtl_worker_manager() {
    static MtlWorkerManager manager;
    return manager;
}

const std::filesystem::path &MtlWorkerManager::worker_executable_path() const noexcept {
    return impl_->worker_executable_path;
}

} // namespace st2110
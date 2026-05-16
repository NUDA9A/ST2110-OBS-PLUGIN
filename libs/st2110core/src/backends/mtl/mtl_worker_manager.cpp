#include <st2110/backends/mtl/mtl_worker_manager.hpp>

#include <st2110/backends/mtl/mtl_worker_process_control_channel.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace st2110 {

namespace {

enum class WorkerRecordState {
    Healthy,
    Unhealthy,
    Stopping,
};

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

void force_close_worker_channel_noexcept(const MtlWorkerManager::WorkerLease &lease) noexcept {
    if (!lease.control_channel) {
        return;
    }

    try {
        auto process_channel = std::dynamic_pointer_cast<MtlWorkerProcessControlChannel>(lease.control_channel);
        if (process_channel) {
            process_channel->shutdown_noexcept();
        }
    } catch (...) {
        /*
         * Manager cleanup paths must stay noexcept.
         */
    }
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
         * shutdown/invalidation paths must remain noexcept.
         */
    }

    force_close_worker_channel_noexcept(lease);
}

void shutdown_worker_leases_noexcept(const std::vector<MtlWorkerManager::WorkerLease> &leases) noexcept {
    for (const auto &lease : leases) {
        shutdown_worker_lease_noexcept(lease);
    }
}

} // namespace

struct MtlWorkerManager::Impl {
    struct WorkerRecord {
        WorkerLease lease{};
        WorkerRecordState state = WorkerRecordState::Healthy;
        std::unordered_set<MtlWorkerGraphId> active_graphs{};
    };

    std::filesystem::path worker_executable_path = "st2110_mtl_rx_worker";

    std::mutex mutex{};
    std::uint64_t next_worker_id = 1;
    std::vector<WorkerRecord> workers{};

    [[nodiscard]] static bool worker_matches_runtime(const WorkerRecord &record,
                                                     const MtlRuntimeConfig &runtime) noexcept {
        return record.state == WorkerRecordState::Healthy && record.lease.control_channel &&
               record.lease.control_channel->healthy() && record.lease.runtime == runtime;
    }

    void collect_retirable_workers_no_lock(const MtlRuntimeConfig &requested_runtime,
                                           std::vector<WorkerLease> &out_workers_to_shutdown) {
        auto it = workers.begin();
        while (it != workers.end()) {
            const bool unhealthy = it->state != WorkerRecordState::Healthy || !it->lease.control_channel ||
                                   !it->lease.control_channel->healthy();
            const bool idle_incompatible = it->active_graphs.empty() && it->lease.runtime != requested_runtime;

            if (unhealthy || idle_incompatible) {
                it->state = WorkerRecordState::Stopping;
                out_workers_to_shutdown.push_back(it->lease);
                it = workers.erase(it);
                continue;
            }

            ++it;
        }
    }

    [[nodiscard]] std::optional<WorkerLease> find_compatible_worker_no_lock(const MtlRuntimeConfig &runtime) const {
        for (const WorkerRecord &worker : workers) {
            if (worker_matches_runtime(worker, runtime)) {
                return worker.lease;
            }
        }

        return std::nullopt;
    }
};

MtlWorkerManager::MtlWorkerManager() : impl_(std::make_unique<Impl>()) {}

MtlWorkerManager::MtlWorkerManager(std::filesystem::path worker_executable_path) : impl_(std::make_unique<Impl>()) {
    impl_->worker_executable_path = std::move(worker_executable_path);
}

MtlWorkerManager::~MtlWorkerManager() { shutdown_all_workers_noexcept(); }

std::expected<MtlWorkerManager::WorkerLease, Error>
MtlWorkerManager::acquire_or_spawn_compatible_worker_for_graph(const MtlRuntimeConfig &runtime,
                                                               const MtlWorkerGraphId graph_id) {
    if (graph_id == 0) {
        return std::unexpected(Error::InvalidValue);
    }

    std::vector<WorkerLease> workers_to_shutdown{};
    std::optional<WorkerLease> reusable_worker{};

    {
        std::lock_guard lock(impl_->mutex);

        impl_->collect_retirable_workers_no_lock(runtime, workers_to_shutdown);

        for (auto &worker : impl_->workers) {
            if (!Impl::worker_matches_runtime(worker, runtime)) {
                continue;
            }

            worker.active_graphs.insert(graph_id);
            reusable_worker = worker.lease;
            break;
        }
    }

    shutdown_worker_leases_noexcept(workers_to_shutdown);

    if (reusable_worker.has_value()) {
        return *reusable_worker;
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
        force_close_worker_channel_noexcept(WorkerLease{
            .worker_id = 0,
            .runtime = runtime,
            .control_channel = control_channel,
        });
        return std::unexpected(event.error());
    }

    auto accepted = interpret_config_handshake_event(*event, request_id);
    if (!accepted.has_value()) {
        force_close_worker_channel_noexcept(WorkerLease{
            .worker_id = 0,
            .runtime = runtime,
            .control_channel = control_channel,
        });
        return std::unexpected(accepted.error());
    }

    WorkerLease worker{
        .worker_id = 0,
        .runtime = runtime,
        .control_channel = std::move(control_channel),
    };

    std::optional<WorkerLease> race_reusable_worker{};

    {
        std::lock_guard lock(impl_->mutex);

        /*
         * A compatible worker may have appeared while this function was spawning
         * and handshaking the new process. Prefer reuse and retire the extra
         * process outside the manager mutex.
         */
        for (auto &existing : impl_->workers) {
            if (!Impl::worker_matches_runtime(existing, runtime)) {
                continue;
            }

            existing.active_graphs.insert(graph_id);
            race_reusable_worker = existing.lease;
            break;
        }

        if (!race_reusable_worker.has_value()) {
            worker.worker_id = impl_->next_worker_id++;

            Impl::WorkerRecord record{
                .lease = worker,
                .state = WorkerRecordState::Healthy,
                .active_graphs = {},
            };
            record.active_graphs.insert(graph_id);

            impl_->workers.push_back(std::move(record));
        }
    }

    if (race_reusable_worker.has_value()) {
        shutdown_worker_lease_noexcept(worker);
        return *race_reusable_worker;
    }

    return worker;
}

void MtlWorkerManager::release_graph_noexcept(const std::uint64_t worker_id, const MtlWorkerGraphId graph_id) noexcept {
    if (worker_id == 0 || graph_id == 0) {
        return;
    }

    std::optional<WorkerLease> worker_to_shutdown{};

    try {
        std::lock_guard lock(impl_->mutex);

        auto it = impl_->workers.begin();
        while (it != impl_->workers.end()) {
            if (it->lease.worker_id != worker_id) {
                ++it;
                continue;
            }

            it->active_graphs.erase(graph_id);

            if (!it->active_graphs.empty()) {
                return;
            }

            const bool another_runtime_worker_exists = [&]() {
                for (const auto &other : impl_->workers) {
                    if (other.lease.worker_id == it->lease.worker_id) {
                        continue;
                    }

                    if (other.state == WorkerRecordState::Healthy && other.lease.control_channel &&
                        other.lease.control_channel->healthy() && other.lease.runtime != it->lease.runtime) {
                        return true;
                    }
                }

                return false;
            }();

            if (another_runtime_worker_exists) {
                it->state = WorkerRecordState::Stopping;
                worker_to_shutdown = it->lease;
                impl_->workers.erase(it);
            }

            return;
        }
    } catch (...) {
        return;
    }

    if (worker_to_shutdown.has_value()) {
        shutdown_worker_lease_noexcept(*worker_to_shutdown);
    }
}

void MtlWorkerManager::invalidate_worker_noexcept(const std::uint64_t worker_id) noexcept {
    if (worker_id == 0) {
        return;
    }

    std::optional<WorkerLease> worker_to_shutdown{};

    try {
        std::lock_guard lock(impl_->mutex);

        auto it = impl_->workers.begin();
        while (it != impl_->workers.end()) {
            if (it->lease.worker_id != worker_id) {
                ++it;
                continue;
            }

            it->state = WorkerRecordState::Unhealthy;
            worker_to_shutdown = it->lease;
            impl_->workers.erase(it);
            break;
        }
    } catch (...) {
        return;
    }

    if (worker_to_shutdown.has_value()) {
        shutdown_worker_lease_noexcept(*worker_to_shutdown);
    }
}

void MtlWorkerManager::shutdown_all_workers_noexcept() noexcept {
    std::vector<WorkerLease> workers_to_shutdown{};

    try {
        std::lock_guard lock(impl_->mutex);

        for (auto &worker : impl_->workers) {
            worker.state = WorkerRecordState::Stopping;
            workers_to_shutdown.push_back(worker.lease);
        }

        impl_->workers.clear();
    } catch (...) {
        return;
    }

    shutdown_worker_leases_noexcept(workers_to_shutdown);
}

MtlWorkerManager &default_mtl_worker_manager() {
    static MtlWorkerManager manager;
    return manager;
}

const std::filesystem::path &MtlWorkerManager::worker_executable_path() const noexcept {
    return impl_->worker_executable_path;
}

} // namespace st2110
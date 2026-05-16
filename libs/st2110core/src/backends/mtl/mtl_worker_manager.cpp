#include <st2110/backends/mtl/mtl_worker_manager.hpp>

#include <st2110/backends/mtl/mtl_worker_process_control_channel.hpp>

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#if defined(__linux__)
#include <dlfcn.h>
#include <unistd.h>
#endif

namespace st2110 {

namespace {

inline constexpr const char *mtlWorkerExecutableName = "st2110_mtl_rx_worker";
inline constexpr const char *mtlWorkerExplicitPathEnv = "ST2110_MTL_RX_WORKER";
inline constexpr const char *mtlWorkerPathFallbackEnv = "ST2110_MTL_RX_WORKER_PATH_FALLBACK";

#ifndef ST2110_MTL_RX_WORKER_INSTALL_DIR
#define ST2110_MTL_RX_WORKER_INSTALL_DIR ""
#endif

[[nodiscard]] bool env_flag_enabled(const char *name) noexcept {
    const char *value = std::getenv(name);
    if (!value) {
        return false;
    }

    const std::string text{value};
    return text == "1" || text == "ON" || text == "on" || text == "true" || text == "TRUE";
}

[[nodiscard]] bool executable_regular_file(const std::filesystem::path &path) noexcept {
#if defined(__linux__)
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec) && ::access(path.c_str(), X_OK) == 0;
#else
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
#endif
}

[[nodiscard]] std::optional<std::filesystem::path> current_module_dir() {
#if defined(__linux__)
    Dl_info info{};
    if (::dladdr(reinterpret_cast<const void *>(&default_mtl_worker_manager), &info) == 0 || !info.dli_fname) {
        return std::nullopt;
    }

    std::error_code ec;
    std::filesystem::path module_path = std::filesystem::weakly_canonical(info.dli_fname, ec);
    if (ec) {
        module_path = info.dli_fname;
    }

    return module_path.parent_path();
#else
    return std::nullopt;
#endif
}

[[nodiscard]] std::expected<std::filesystem::path, Error>
resolve_default_mtl_worker_executable_path(std::string &out_message) {
    out_message.clear();

    const char *explicit_path_env = std::getenv(mtlWorkerExplicitPathEnv);
    if (explicit_path_env && explicit_path_env[0] != '\0') {
        std::filesystem::path explicit_path{explicit_path_env};

        if (explicit_path.has_parent_path() && executable_regular_file(explicit_path)) {
            return explicit_path;
        }

        if (!explicit_path.has_parent_path() && env_flag_enabled(mtlWorkerPathFallbackEnv)) {
            return explicit_path;
        }

        out_message = std::string{mtlWorkerExplicitPathEnv} +
                      " points to a missing or non-executable worker binary: " + explicit_path.string();
        return std::unexpected(Error::InvalidBackendState);
    }

    std::vector<std::filesystem::path> checked{};

    if (auto module_dir = current_module_dir()) {
        checked.push_back(*module_dir / mtlWorkerExecutableName);
    }

    const std::string configured_install_dir{ST2110_MTL_RX_WORKER_INSTALL_DIR};
    if (!configured_install_dir.empty()) {
        checked.push_back(std::filesystem::path{configured_install_dir} / mtlWorkerExecutableName);
    }

    for (const auto &candidate : checked) {
        if (executable_regular_file(candidate)) {
            return candidate;
        }
    }

    if (env_flag_enabled(mtlWorkerPathFallbackEnv)) {
        /*
         * Development-only fallback. Product installs must resolve an absolute
         * worker path next to the plugin or through ST2110_MTL_RX_WORKER.
         */
        return std::filesystem::path{mtlWorkerExecutableName};
    }

    out_message = "MTL RX worker binary was not found. Expected st2110_mtl_rx_worker next to st2110_obs.so";

    if (!configured_install_dir.empty()) {
        out_message += " or under configured worker install dir: ";
        out_message += configured_install_dir;
    }

    if (!checked.empty()) {
        out_message += ". Checked:";
        for (const auto &candidate : checked) {
            out_message += " ";
            out_message += candidate.string();
        }
    }

    out_message +=
        ". Install the worker with the OBS plugin, or set ST2110_MTL_RX_WORKER to the absolute worker path. "
        "PATH lookup is disabled for product builds; set ST2110_MTL_RX_WORKER_PATH_FALLBACK=1 only for development.";

    return std::unexpected(Error::InvalidBackendState);
}

enum class WorkerRecordState {
    Healthy,
    Unhealthy,
    Stopping,
};

[[nodiscard]] MtlWorkerRequestId next_worker_request_id() noexcept {
    static std::atomic<MtlWorkerRequestId> next{1};
    return next.fetch_add(1, std::memory_order_relaxed);
}

[[nodiscard]] std::expected<bool, Error> interpret_config_handshake_event(const MtlWorkerControlEvent &event,
                                                                          const MtlWorkerRequestId expected_request_id,
                                                                          std::string *detail) {
    return std::visit(
        [expected_request_id, detail](const auto &typed_event) -> std::expected<bool, Error> {
            using Event = std::decay_t<decltype(typed_event)>;

            if constexpr (std::is_same_v<Event, MtlWorkerHealthEvent>) {
                if (typed_event.request_id != expected_request_id) {
                    if (detail) {
                        *detail = "Worker returned HealthEvent for a different request_id";
                    }
                    return std::unexpected(Error::InvalidBackendState);
                }

                if (!typed_event.healthy) {
                    if (detail) {
                        *detail = typed_event.message;
                    }
                    return std::unexpected(Error::InvalidBackendState);
                }

                return true;
            } else if constexpr (std::is_same_v<Event, MtlWorkerErrorEvent>) {
                if (typed_event.request_id != expected_request_id) {
                    if (detail) {
                        *detail = "Worker returned ErrorEvent for a different request_id";
                    }
                    return std::unexpected(Error::InvalidBackendState);
                }

                if (detail) {
                    *detail = typed_event.message;
                }

                return std::unexpected(typed_event.error);
            } else {
                if (detail) {
                    *detail = "Worker returned an unexpected event type during ConfigHandshake";
                }
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

    std::filesystem::path worker_executable_path{};
    bool worker_executable_path_explicit = false;
    std::string last_error_message{};

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

    void set_last_error_message_no_lock(std::string message) { last_error_message = std::move(message); }

    [[nodiscard]] bool ensure_worker_executable_path_no_lock() {
        if (worker_executable_path_explicit && !worker_executable_path.empty()) {
            return true;
        }

        if (!worker_executable_path.empty()) {
            return true;
        }

        std::string message{};
        auto resolved = resolve_default_mtl_worker_executable_path(message);
        if (!resolved.has_value()) {
            set_last_error_message_no_lock(std::move(message));
            worker_executable_path.clear();
            return false;
        }

        worker_executable_path = std::move(*resolved);
        last_error_message.clear();
        return true;
    }
};

MtlWorkerManager::MtlWorkerManager() : impl_(std::make_unique<Impl>()) {
    std::lock_guard lock(impl_->mutex);
    (void)impl_->ensure_worker_executable_path_no_lock();
}

MtlWorkerManager::MtlWorkerManager(std::filesystem::path worker_executable_path) : impl_(std::make_unique<Impl>()) {
    std::lock_guard lock(impl_->mutex);
    impl_->worker_executable_path = std::move(worker_executable_path);
    impl_->worker_executable_path_explicit = true;
}

MtlWorkerManager::~MtlWorkerManager() { shutdown_all_workers_noexcept(); }

std::expected<MtlWorkerManager::WorkerLease, Error>
MtlWorkerManager::acquire_or_spawn_compatible_worker_for_graph(const MtlRuntimeConfig &runtime,
                                                               const MtlWorkerGraphId graph_id) {
    if (graph_id == 0) {
        return std::unexpected(Error::InvalidValue);
    }

    std::filesystem::path worker_executable_path{};

    {
        std::lock_guard lock(impl_->mutex);

        if (!impl_->ensure_worker_executable_path_no_lock()) {
            return std::unexpected(Error::InvalidBackendState);
        }

        worker_executable_path = impl_->worker_executable_path;
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

    auto control_channel = create_mtl_worker_process_control_channel(worker_executable_path);
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
        {
            std::lock_guard lock(impl_->mutex);
            impl_->set_last_error_message_no_lock(
                "MTL RX worker failed to start or exited before ConfigHandshake. Worker path: " +
                worker_executable_path.string() +
                ". Verify that st2110_mtl_rx_worker is installed next to the OBS plugin and that MTL/DPDK runtime "
                "libraries are visible to the dynamic linker. Run ldconfig after installing MTL. " +
                std::string{"Underlying error: "} + to_string(event.error()));
        }

        force_close_worker_channel_noexcept(WorkerLease{
            .worker_id = 0,
            .runtime = runtime,
            .control_channel = control_channel,
        });
        return std::unexpected(event.error());
    }

    std::string handshake_detail{};
    auto accepted = interpret_config_handshake_event(*event, request_id, &handshake_detail);
    if (!accepted.has_value()) {
        {
            std::lock_guard lock(impl_->mutex);
            impl_->set_last_error_message_no_lock(
                "MTL RX worker ConfigHandshake failed. Worker path: " + worker_executable_path.string() +
                (handshake_detail.empty() ? "" : ". Detail: " + handshake_detail) +
                ". Error: " + to_string(accepted.error()));
        }

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

    {
        std::lock_guard lock(impl_->mutex);
        impl_->last_error_message.clear();
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

std::string MtlWorkerManager::last_error_message() const {
    try {
        std::lock_guard lock(impl_->mutex);
        return impl_->last_error_message;
    } catch (...) {
        return {};
    }
}

} // namespace st2110
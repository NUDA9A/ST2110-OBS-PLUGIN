#ifndef ST2110_OBS_PLUGIN_MTL_WORKER_MANAGER_HPP
#define ST2110_OBS_PLUGIN_MTL_WORKER_MANAGER_HPP

#include <st2110/backends/mtl/mtl_runtime_config.hpp>
#include <st2110/backends/mtl/mtl_worker_control_channel.hpp>
#include <st2110/foundation/error.hpp>

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>

namespace st2110 {

/*
 * OBS-process-side MTL worker manager.
 *
 * This class must not include MTL runtime headers and must not call MTL APIs.
 * It is responsible for supervising/acquiring compatible MTL worker processes.
 *
 * Workers are cached by derived MtlRuntimeConfig. Multiple receive graphs with
 * the same compatible runtime config reuse the same worker process/control
 * channel; incompatible runtime configs get separate workers.
 *
 * The manager also tracks graph ownership. A healthy idle compatible worker may
 * remain cached for future reuse. A healthy idle incompatible worker is retired
 * when a new incompatible runtime is acquired or when it becomes idle while
 * another runtime worker exists.
 */
class MtlWorkerManager final {
  public:
    struct WorkerLease {
        std::uint64_t worker_id = 0;
        MtlRuntimeConfig runtime{};
        std::shared_ptr<IMtlWorkerControlChannel> control_channel{};
    };

    MtlWorkerManager();
    ~MtlWorkerManager();
    explicit MtlWorkerManager(std::filesystem::path worker_executable_path);

    MtlWorkerManager(const MtlWorkerManager &) = delete;
    MtlWorkerManager &operator=(const MtlWorkerManager &) = delete;

    MtlWorkerManager(MtlWorkerManager &&) noexcept = delete;
    MtlWorkerManager &operator=(MtlWorkerManager &&) noexcept = delete;

    [[nodiscard]] std::expected<WorkerLease, Error>
    acquire_or_spawn_compatible_worker_for_graph(const MtlRuntimeConfig &runtime, MtlWorkerGraphId graph_id);

    void release_graph_noexcept(std::uint64_t worker_id, MtlWorkerGraphId graph_id) noexcept;

    void invalidate_worker_noexcept(std::uint64_t worker_id) noexcept;

    void shutdown_all_workers_noexcept() noexcept;
    [[nodiscard]] const std::filesystem::path &worker_executable_path() const noexcept;
    [[nodiscard]] std::string last_error_message() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

[[nodiscard]] MtlWorkerManager &default_mtl_worker_manager();

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_MTL_WORKER_MANAGER_HPP
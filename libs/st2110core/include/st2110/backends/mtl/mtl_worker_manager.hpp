#ifndef ST2110_OBS_PLUGIN_MTL_WORKER_MANAGER_HPP
#define ST2110_OBS_PLUGIN_MTL_WORKER_MANAGER_HPP

#include <st2110/backends/mtl/mtl_runtime_config.hpp>
#include <st2110/backends/mtl/mtl_worker_control_channel.hpp>
#include <st2110/foundation/error.hpp>

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>

namespace st2110 {

/*
 * OBS-process-side MTL worker manager.
 *
 * This class must not include MTL runtime headers and must not call MTL APIs.
 * It is responsible for supervising/acquiring a compatible MTL worker process.
 *
 * Current skeleton does not spawn a process yet. It only fixes the OBS-side
 * ownership/API boundary for future process supervision and IPC.
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

    [[nodiscard]] std::expected<WorkerLease, Error> acquire_or_spawn_compatible_worker(const MtlRuntimeConfig &runtime);

    void shutdown_all_workers_noexcept() noexcept;
    [[nodiscard]] const std::filesystem::path &worker_executable_path() const noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

[[nodiscard]] MtlWorkerManager &default_mtl_worker_manager();

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_MTL_WORKER_MANAGER_HPP
#ifndef ST2110_OBS_PLUGIN_MTL_WORKER_PROCESS_CONTROL_CHANNEL_HPP
#define ST2110_OBS_PLUGIN_MTL_WORKER_PROCESS_CONTROL_CHANNEL_HPP

#include <st2110/backends/mtl/mtl_worker_control_channel.hpp>
#include <st2110/backends/mtl/mtl_worker_protocol.hpp>
#include <st2110/foundation/error.hpp>

#include <expected>
#include <filesystem>
#include <memory>
#include <span>

namespace st2110 {

/*
 * OBS-process-side control channel backed by an MTL worker process.
 *
 * Current implementation starts the worker process, exchanges length-prefixed
 * typed control frames over a Unix socketpair, and keeps MTL API ownership out
 * of the OBS process.
 *
 * Shared-memory media transport is still a separate future boundary.
 */
class MtlWorkerProcessControlChannel final : public IMtlWorkerControlChannel {
  public:
    explicit MtlWorkerProcessControlChannel(std::filesystem::path worker_executable_path);
    ~MtlWorkerProcessControlChannel() override;

    MtlWorkerProcessControlChannel(const MtlWorkerProcessControlChannel &) = delete;
    MtlWorkerProcessControlChannel &operator=(const MtlWorkerProcessControlChannel &) = delete;

    MtlWorkerProcessControlChannel(MtlWorkerProcessControlChannel &&) noexcept = delete;
    MtlWorkerProcessControlChannel &operator=(MtlWorkerProcessControlChannel &&) noexcept = delete;

    [[nodiscard]] std::expected<MtlWorkerControlEventEnvelope, Error>
    transact_with_fds(const MtlWorkerControlRequest &request, std::span<const int> file_descriptors) override;

    void shutdown_noexcept() noexcept;

    [[nodiscard]] const std::filesystem::path &worker_executable_path() const noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

[[nodiscard]] std::shared_ptr<IMtlWorkerControlChannel>
create_mtl_worker_process_control_channel(std::filesystem::path worker_executable_path);

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_MTL_WORKER_PROCESS_CONTROL_CHANNEL_HPP
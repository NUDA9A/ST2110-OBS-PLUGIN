#include <st2110/backends/mtl/mtl_worker_process_control_channel.hpp>

#include <utility>

namespace st2110 {

struct MtlWorkerProcessControlChannel::Impl {
    std::filesystem::path worker_executable_path{};

    explicit Impl(std::filesystem::path path) : worker_executable_path(std::move(path)) {}
};

MtlWorkerProcessControlChannel::MtlWorkerProcessControlChannel(std::filesystem::path worker_executable_path)
    : impl_(std::make_unique<Impl>(std::move(worker_executable_path))) {}

MtlWorkerProcessControlChannel::~MtlWorkerProcessControlChannel() { shutdown_noexcept(); }

std::expected<MtlWorkerControlEvent, Error>
MtlWorkerProcessControlChannel::transact(const MtlWorkerControlRequest &request) {
    (void)request;

    /*
     * Future implementation point:
     *
     * 1. spawn/connect worker process if needed;
     * 2. serialize request into IPC frame;
     * 3. wait for matching worker response/event;
     * 4. deserialize to MtlWorkerControlEvent.
     *
     * Returning Unsupported is intentional until real process supervision and
     * IPC framing exist.
     */
    return std::unexpected(Error::Unsupported);
}

void MtlWorkerProcessControlChannel::shutdown_noexcept() noexcept {
    /*
     * Future implementation point:
     *
     * Send ShutdownWorker if connected, close IPC handles, terminate/reap
     * process if required.
     */
}

const std::filesystem::path &MtlWorkerProcessControlChannel::worker_executable_path() const noexcept {
    return impl_->worker_executable_path;
}

std::shared_ptr<IMtlWorkerControlChannel>
create_mtl_worker_process_control_channel(std::filesystem::path worker_executable_path) {
    return std::make_shared<MtlWorkerProcessControlChannel>(std::move(worker_executable_path));
}

} // namespace st2110
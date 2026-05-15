#ifndef ST2110_OBS_PLUGIN_MTL_WORKER_CONTROL_CHANNEL_HPP
#define ST2110_OBS_PLUGIN_MTL_WORKER_CONTROL_CHANNEL_HPP

#include <st2110/backends/mtl/mtl_worker_protocol.hpp>
#include <st2110/foundation/error.hpp>

#include <expected>
#include <memory>

namespace st2110 {

/*
 * OBS-process-side control channel to an MTL worker process.
 *
 * This interface contains no MTL API types and owns no MTL runtime state.
 * Concrete implementations may use Unix domain sockets, pipes, or another IPC
 * mechanism.
 */
class IMtlWorkerControlChannel {
public:
    virtual ~IMtlWorkerControlChannel() = default;

    IMtlWorkerControlChannel(const IMtlWorkerControlChannel &) = delete;
    IMtlWorkerControlChannel &operator=(const IMtlWorkerControlChannel &) = delete;

    IMtlWorkerControlChannel(IMtlWorkerControlChannel &&) noexcept = delete;
    IMtlWorkerControlChannel &operator=(IMtlWorkerControlChannel &&) noexcept = delete;

    [[nodiscard]] virtual std::expected<MtlWorkerControlEvent, Error>
    transact(const MtlWorkerControlRequest &request) = 0;

protected:
    IMtlWorkerControlChannel() = default;
};

/*
 * Temporary skeleton channel.
 *
 * It lets OBS-side code route through the correct control-channel boundary
 * without pretending that real IPC exists.
 */
[[nodiscard]] std::shared_ptr<IMtlWorkerControlChannel> create_unsupported_mtl_worker_control_channel();

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_MTL_WORKER_CONTROL_CHANNEL_HPP
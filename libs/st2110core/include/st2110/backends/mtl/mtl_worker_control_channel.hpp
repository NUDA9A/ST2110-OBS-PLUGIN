#ifndef ST2110_OBS_PLUGIN_MTL_WORKER_CONTROL_CHANNEL_HPP
#define ST2110_OBS_PLUGIN_MTL_WORKER_CONTROL_CHANNEL_HPP

#include <st2110/backends/mtl/mtl_worker_ipc_framing.hpp>
#include <st2110/backends/mtl/mtl_worker_protocol.hpp>
#include <st2110/foundation/error.hpp>

#include <expected>
#include <functional>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace st2110 {

/*
 * Typed worker response envelope.
 *
 * The typed event is deserialized from the byte payload.
 * Any descriptors attached to the same IPC frame remain owned by ipc_frame.
 *
 * Call release_file_descriptors() when ownership must move to a higher-level
 * component, for example a future shared-memory ring importer.
 */
struct MtlWorkerControlEventEnvelope {
    MtlWorkerControlEvent event{};
    MtlWorkerIpcFrame ipc_frame{};

    MtlWorkerControlEventEnvelope() = default;

    MtlWorkerControlEventEnvelope(MtlWorkerControlEvent response_event, MtlWorkerIpcFrame transport_frame)
        : event(std::move(response_event)), ipc_frame(std::move(transport_frame)) {}

    [[nodiscard]] const std::vector<int> &file_descriptors() const noexcept { return ipc_frame.file_descriptors(); }

    [[nodiscard]] bool has_file_descriptors() const noexcept { return ipc_frame.has_file_descriptors(); }

    [[nodiscard]] std::vector<int> release_file_descriptors() noexcept { return ipc_frame.release_file_descriptors(); }
};

/*
 * Callback for asynchronous worker events that are not responses to a transact()
 * request.
 *
 * Current async event types:
 * - MtlWorkerFrameReadyEvent;
 * - MtlWorkerAudioBlockReadyEvent.
 *
 * The envelope is move-only because it owns any received ancillary descriptors.
 */
using MtlWorkerAsyncEventHandler = std::function<void(MtlWorkerControlEventEnvelope)>;

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

    /*
     * Payload-only compatibility API.
     *
     * This rejects responses that carry descriptors. Use transact_with_fds()
     * when descriptors are part of the expected response contract.
     */
    [[nodiscard]] virtual std::expected<MtlWorkerControlEvent, Error> transact(const MtlWorkerControlRequest &request);

    /*
     * Fd-aware typed transaction API.
     *
     * file_descriptors are borrowed by this call. Ownership remains with the
     * caller. Any response descriptors are returned owned by the response
     * envelope.
     */
    [[nodiscard]] virtual std::expected<MtlWorkerControlEventEnvelope, Error>
    transact_with_fds(const MtlWorkerControlRequest &request, std::span<const int> file_descriptors) = 0;

    /*
     * Registers graph-scoped async event routing.
     *
     * The handler is called from the control-channel reader thread. It must not
     * block for long-running media processing.
     */
    [[nodiscard]] virtual std::expected<bool, Error>
    register_async_event_handler(MtlWorkerGraphId graph_id, MtlWorkerAsyncEventHandler handler) = 0;

    virtual void unregister_async_event_handler_noexcept(MtlWorkerGraphId graph_id) noexcept = 0;

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
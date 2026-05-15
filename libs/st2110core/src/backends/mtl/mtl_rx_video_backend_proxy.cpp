#include <st2110/backends/mtl/mtl_rx_video_backend_proxy.hpp>

#include <st2110/foundation/error.hpp>

#include <expected>
#include <utility>

namespace st2110 {

MtlRxVideoBackendProxy::MtlRxVideoBackendProxy(MtlVideoStartConfig cfg) : cfg_(std::move(cfg)) {}

RxBackendLifecycleResult MtlRxVideoBackendProxy::start(IFrameSink *sink) {
    (void)sink;

    /*
     * Worker IPC/shared-memory transport is not implemented yet.
     *
     * Returning Unsupported is intentional for the skeleton: this proxy already
     * defines the OBS-process boundary, but it must not silently pretend to run
     * before it can communicate with the MTL worker process.
     */
    return std::unexpected(Error::Unsupported);
}

RxBackendLifecycleResult MtlRxVideoBackendProxy::stop() {
    return true;
}

} // namespace st2110
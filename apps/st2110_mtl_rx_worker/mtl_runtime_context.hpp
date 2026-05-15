#ifndef ST2110_OBS_PLUGIN_MTL_RX_WORKER_RUNTIME_CONTEXT_HPP
#define ST2110_OBS_PLUGIN_MTL_RX_WORKER_RUNTIME_CONTEXT_HPP

#include <st2110/backends/mtl/mtl_runtime_config.hpp>
#include <st2110/foundation/error.hpp>

#include <mtl/mtl_api.h>

#include <expected>
#include <memory>

namespace st2110_mtl_rx_worker {

/*
 * Worker-process-local MTL runtime context.
 *
 * This object is allowed to own the real MTL runtime because it lives inside
 * the MTL worker process, not inside the OBS process.
 *
 * Public OBS/plugin/source code must not include this header.
 */
class MtlRuntimeContext final {
public:
    static std::expected<std::unique_ptr<MtlRuntimeContext>, st2110::Error>
    create(st2110::MtlRuntimeConfig cfg);

    ~MtlRuntimeContext();

    MtlRuntimeContext(const MtlRuntimeContext &) = delete;
    MtlRuntimeContext &operator=(const MtlRuntimeContext &) = delete;

    MtlRuntimeContext(MtlRuntimeContext &&) noexcept = delete;
    MtlRuntimeContext &operator=(MtlRuntimeContext &&) noexcept = delete;

    [[nodiscard]] const st2110::MtlRuntimeConfig &config() const noexcept;

    /*
     * Worker-private native MTL handle accessor.
     *
     * This must remain inside the worker process boundary. OBS-process code
     * must not include this header or observe this handle.
     */
    [[nodiscard]] mtl_handle handle() const noexcept;

private:
    struct Impl;

    explicit MtlRuntimeContext(std::unique_ptr<Impl> impl);

    std::unique_ptr<Impl> impl_;
};

} // namespace st2110_mtl_rx_worker

#endif // ST2110_OBS_PLUGIN_MTL_RX_WORKER_RUNTIME_CONTEXT_HPP
#ifndef ST2110_OBS_PLUGIN_MTL_RX_VIDEO_BACKEND_HPP
#define ST2110_OBS_PLUGIN_MTL_RX_VIDEO_BACKEND_HPP

#include <st2110/contracts/backend/backend.hpp>
#include <st2110/delivery/video/mtl_video_start_config.hpp>

#include <memory>

namespace st2110 {

class MtlRxVideoBackend final : public IRxBackend {
public:
    explicit MtlRxVideoBackend(MtlVideoStartConfig cfg);
    ~MtlRxVideoBackend() override;

    MtlRxVideoBackend(const MtlRxVideoBackend &) = delete;
    MtlRxVideoBackend &operator=(const MtlRxVideoBackend &) = delete;

    MtlRxVideoBackend(MtlRxVideoBackend &&) noexcept = delete;
    MtlRxVideoBackend &operator=(MtlRxVideoBackend &&) noexcept = delete;

    RxBackendLifecycleResult start(IFrameSink *sink) override;
    RxBackendLifecycleResult stop() override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_MTL_RX_VIDEO_BACKEND_HPP
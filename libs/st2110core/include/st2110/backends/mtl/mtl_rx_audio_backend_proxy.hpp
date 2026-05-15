#ifndef ST2110_OBS_PLUGIN_MTL_RX_AUDIO_BACKEND_PROXY_HPP
#define ST2110_OBS_PLUGIN_MTL_RX_AUDIO_BACKEND_PROXY_HPP

#include <st2110/contracts/backend/backend.hpp>
#include <st2110/delivery/audio/mtl_audio_start_config.hpp>

namespace st2110 {

/*
 * OBS-process MTL audio backend proxy.
 *
 * This class must not own real MTL runtime state and must not call MTL APIs.
 * The future implementation will send audio session commands to an MTL worker
 * process and deliver worker-exported audio blocks to the OBS-process sink.
 */
class MtlRxAudioBackendProxy final : public IRxBackend {
public:
    explicit MtlRxAudioBackendProxy(MtlAudioStartConfig cfg);
    ~MtlRxAudioBackendProxy() override = default;

    MtlRxAudioBackendProxy(const MtlRxAudioBackendProxy &) = delete;
    MtlRxAudioBackendProxy &operator=(const MtlRxAudioBackendProxy &) = delete;

    MtlRxAudioBackendProxy(MtlRxAudioBackendProxy &&) noexcept = delete;
    MtlRxAudioBackendProxy &operator=(MtlRxAudioBackendProxy &&) noexcept = delete;

    RxBackendLifecycleResult start(IFrameSink *sink) override;
    RxBackendLifecycleResult stop() override;

private:
    MtlAudioStartConfig cfg_{};
};

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_MTL_RX_AUDIO_BACKEND_PROXY_HPP
#ifndef ST2110_OBS_PLUGIN_MTL_RX_AUDIO_BACKEND_HPP
#define ST2110_OBS_PLUGIN_MTL_RX_AUDIO_BACKEND_HPP

#include <st2110/contracts/backend/backend.hpp>
#include <st2110/delivery/audio/mtl_audio_start_config.hpp>

#include <memory>

namespace st2110 {

class MtlRxAudioBackend final : public IRxBackend {
public:
    explicit MtlRxAudioBackend(MtlAudioStartConfig cfg);
    ~MtlRxAudioBackend() override;

    MtlRxAudioBackend(const MtlRxAudioBackend &) = delete;
    MtlRxAudioBackend &operator=(const MtlRxAudioBackend &) = delete;

    MtlRxAudioBackend(MtlRxAudioBackend &&) noexcept = delete;
    MtlRxAudioBackend &operator=(MtlRxAudioBackend &&) noexcept = delete;

    RxBackendLifecycleResult start(IFrameSink *sink) override;
    RxBackendLifecycleResult stop() override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_MTL_RX_AUDIO_BACKEND_HPP
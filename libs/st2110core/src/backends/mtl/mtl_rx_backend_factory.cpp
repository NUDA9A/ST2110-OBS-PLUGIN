#include "st2110/backends/mtl/mtl_rx_backend_factory.hpp"
#include "st2110/backends/mtl/mtl_rx_video_backend.hpp"

#include <memory>

namespace st2110 {
namespace {
constexpr bool kMtlVideoRuntimeAvailable = false;
constexpr bool kMtlAudioRuntimeAvailable = false;
} // namespace

RxBackendDescriptor MtlRxAudioBackendFactory::descriptor() const {
    return RxBackendDescriptor{.kind = RxBackendKind::Mtl,
                               .name = "mtl",
                               .capabilities = RxBackendCapabilities{.video_rx = false, .audio_rx = true},
                               .available = kMtlAudioRuntimeAvailable};
}

std::unique_ptr<IRxBackend> MtlRxAudioBackendFactory::create_backend() const {
    return nullptr;
}

RxBackendDescriptor MtlRxVideoBackendFactory::descriptor() const {
    return RxBackendDescriptor{.kind = RxBackendKind::Mtl,
                               .name = "mtl",
                               .capabilities = RxBackendCapabilities{.video_rx = true, .audio_rx = false},
                               .available = kMtlVideoRuntimeAvailable};
}

std::unique_ptr<IRxBackend> MtlRxVideoBackendFactory::create_backend() const {
    if (!kMtlVideoRuntimeAvailable) {
        return nullptr;
    }

    return std::make_unique<MtlRxVideoBackend>();
}
} // namespace st2110
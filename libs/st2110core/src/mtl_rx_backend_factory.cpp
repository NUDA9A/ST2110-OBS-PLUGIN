#include "st2110/mtl_rx_backend_factory.hpp"
#include "st2110/mtl_rx_video_backend.hpp"

namespace st2110 {
RxBackendDescriptor MtlRxAudioBackendFactory::descriptor() const {
    return RxBackendDescriptor{.kind = RxBackendKind::Mtl,
                               .name = "mtl",
                               .capabilities = RxBackendCapabilities{.video_rx = false, .audio_rx = true},
                               .available = false};
}

std::unique_ptr<IRxBackend> MtlRxAudioBackendFactory::create_backend() const { return nullptr; }

RxBackendDescriptor MtlRxVideoBackendFactory::descriptor() const {
    return RxBackendDescriptor{.kind = RxBackendKind::Mtl,
                               .name = "mtl",
                               .capabilities = RxBackendCapabilities{.video_rx = true, .audio_rx = false},
                               .available = false};
}

std::unique_ptr<IRxBackend> MtlRxVideoBackendFactory::create_backend() const {
    return std::unique_ptr<IRxBackend>(new MtlRxVideoBackend());
}
} // namespace st2110
#include "st2110/backend_factory.hpp"
#include "st2110/socket_rx_video_backend.hpp"
#include "st2110/socket_rx_audio_backend.hpp"

#if defined(ST2110_HAS_MTL_BACKEND) && ST2110_HAS_MTL_BACKEND
#include "st2110/mtl_rx_backend_factory.hpp"
#endif

#include <array>

namespace st2110 {
namespace {
SocketRxVideoBackendFactory socket_rx_video_factory{};
SocketRxAudioBackendFactory socket_rx_audio_factory{};

#if defined(ST2110_HAS_MTL_BACKEND) && ST2110_HAS_MTL_BACKEND
MtlRxVideoBackendFactory mtl_rx_video_factory{};
MtlRxAudioBackendFactory mtl_rx_audio_factory{};

std::array<IRxBackendFactory *, 4> builtin_rx_backend_factories{
    &socket_rx_video_factory,
    &socket_rx_audio_factory,
    &mtl_rx_video_factory,
    &mtl_rx_audio_factory,
};
#else
std::array<IRxBackendFactory *, 2> builtin_rx_backend_factories{
    &socket_rx_video_factory,
    &socket_rx_audio_factory,
};
#endif
} // namespace

[[nodiscard]] bool rx_backend_kind_built(RxBackendKind kind) noexcept {
    switch (kind) {
    case RxBackendKind::Socket:
        return true;
    case RxBackendKind::Mtl:
#if defined(ST2110_HAS_MTL_BACKEND) && ST2110_HAS_MTL_BACKEND
        return true;
#endif
    default:
        return false;
    }
}

[[nodiscard]] std::span<IRxBackendFactory *const> default_rx_backend_factories() noexcept {
    return {builtin_rx_backend_factories.data(), builtin_rx_backend_factories.size()};
}
} // namespace st2110
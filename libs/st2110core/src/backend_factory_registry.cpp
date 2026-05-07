#include "st2110/backends/socket/socket_rx_audio_backend.hpp"
#include "st2110/contracts/backend/backend_factory.hpp"
#include "st2110/backends/socket/socket_rx_video_backend.hpp"
#include "st2110/video_backend_support.hpp"

#if defined(ST2110_HAS_MTL_BACKEND) && ST2110_HAS_MTL_BACKEND
#include "st2110/backends/mtl/mtl_rx_backend_factory.hpp"
#include "st2110/backends/mtl/mtl_rx_video_backend.hpp"
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

Error validate_selected_rx_video_backend_support_matrix(RxBackendKind backend_kind,
                                                        const CommonVideoBackendSupportMatrix &matrix) noexcept {
    if (const Error err = validate_common_video_backend_support_matrix(matrix); err != Error::Ok) {
        return err;
    }

    switch (backend_kind) {
    case RxBackendKind::Socket:
        return validate_socket_rx_video_backend_support_matrix_implementation_support(
            matrix, default_socket_rx_video_support_policy());

    case RxBackendKind::Mtl:
#if defined(ST2110_HAS_MTL_BACKEND) && ST2110_HAS_MTL_BACKEND
        return validate_mtl_rx_video_backend_support_matrix_implementation_support(
            matrix, default_mtl_rx_video_support_policy());
#else
        return Error::Unsupported;
#endif

    default:
        return Error::InvalidValue;
    }
}

Error validate_rx_video_backend_selection_support(const RxBackendSelection &selection, const RxVideoConfig &cfg) {
    if (const Error err = validate_rx_backend_selection(selection); err != Error::Ok) {
        return err;
    }

    if (selection.media_kind != RxMediaKind::Video) {
        return Error::InvalidValue;
    }

    auto matrix = common_video_backend_support_matrix_from_rx_video_config(cfg);
    if (!matrix.has_value()) {
        return matrix.error();
    }

    return validate_selected_rx_video_backend_support_matrix(selection.backend_kind, *matrix);
}

std::expected<IRxBackendFactory *, Error> select_rx_video_backend_factory(std::span<IRxBackendFactory *const> factories,
                                                                          const RxBackendSelection &selection,
                                                                          const RxVideoConfig &cfg) {
    if (const Error err = validate_rx_video_backend_selection_support(selection, cfg); err != Error::Ok) {
        return std::unexpected(err);
    }

    return select_rx_backend_factory(factories, selection);
}

std::expected<std::unique_ptr<IRxBackend>, Error> create_rx_video_backend(std::span<IRxBackendFactory *const> factories,
                                                                          const RxBackendSelection &selection,
                                                                          const RxVideoConfig &cfg) {
    auto selected_factory = select_rx_video_backend_factory(factories, selection, cfg);
    if (!selected_factory.has_value()) {
        return std::unexpected(selected_factory.error());
    }

    std::unique_ptr<IRxBackend> backend = (*selected_factory)->create_backend();
    if (!backend) {
        return std::unexpected(Error::InvalidValue);
    }

    return backend;
}

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
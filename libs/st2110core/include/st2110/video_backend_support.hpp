#ifndef ST2110_OBS_PLUGIN_VIDEO_BACKEND_SUPPORT_HPP
#define ST2110_OBS_PLUGIN_VIDEO_BACKEND_SUPPORT_HPP

#include "error.hpp"
#include "pixel_format.hpp"
#include "rx_config.hpp"
#include "video_receive_capability.hpp"

#include <expected>

namespace st2110 {

/*
 * Shared common-video support model consumed by backend-local implementation
 * support policies.
 *
 * This model is intentionally backend-agnostic:
 * - it carries the current project pixel/storage axis;
 * - it carries the structurally recognized common video receive capability;
 * - it must not encode Socket- or MTL-specific implementation limits.
 */
struct CommonVideoBackendSupportMatrix {
    PixelFormat project_pixel_format = PixelFormat::UYVY;
    VideoReceiveCapability receive_capability{};
};

/*
 * Structural validation for the common backend-support matrix only.
 *
 * This helper validates:
 * - that the project pixel/storage axis is a known project value;
 * - that the common receive capability is structurally valid.
 *
 * It must not apply:
 * - project-delivery support limits;
 * - Socket implementation limits;
 * - MTL session/projection limits.
 */
[[nodiscard]] inline Error
validate_common_video_backend_support_matrix(const CommonVideoBackendSupportMatrix &matrix) noexcept {
    if (auto project_handoff = video_frame_handoff_format_from_project_pixel_format(matrix.project_pixel_format);
        !project_handoff.has_value()) {
        return project_handoff.error();
    }

    if (Error err = validate_video_receive_capability_structure(matrix.receive_capability); err != Error::Ok) {
        return err;
    }

    return Error::Ok;
}

/*
 * Structural/common validation entry point for backend support evaluation.
 *
 * This helper must remain equivalent to validate_rx_video_config_structure(...)
 * and must not apply:
 * - project-delivery support;
 * - Socket implementation-support policy;
 * - MTL session/projection support policy.
 */
[[nodiscard]] inline Error validate_rx_video_config_for_common_video_backend_support(const RxVideoConfig &cfg) {
    return validate_rx_video_config_structure(cfg);
}

/*
 * Resolves the shared common-video backend-support matrix from RxVideoConfig.
 *
 * Resolution order:
 * 1) structural/common RxVideoConfig validation;
 * 2) effective VideoReceiveCapability resolution;
 * 3) common matrix validation.
 *
 * This helper is the shared pipeline that later backend-local support policies
 * must consume.
 *
 * It must not apply backend-specific narrowing.
 */
[[nodiscard]] inline std::expected<CommonVideoBackendSupportMatrix, Error>
common_video_backend_support_matrix_from_rx_video_config(const RxVideoConfig &cfg) {
    if (Error err = validate_rx_video_config_for_common_video_backend_support(cfg); err != Error::Ok) {
        return std::unexpected(err);
    }

    auto capability = rx_video_config_effective_receive_capability(cfg);
    if (!capability.has_value()) {
        return std::unexpected(capability.error());
    }

    CommonVideoBackendSupportMatrix matrix{
        .project_pixel_format = cfg.format,
        .receive_capability = *capability,
    };

    if (Error err = validate_common_video_backend_support_matrix(matrix); err != Error::Ok) {
        return std::unexpected(err);
    }

    return matrix;
}

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_VIDEO_BACKEND_SUPPORT_HPP
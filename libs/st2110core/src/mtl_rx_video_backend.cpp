#include "st2110/mtl_rx_video_backend.hpp"

#include <utility>

namespace st2110 {
MtlRxVideoBackend::~MtlRxVideoBackend() { (void)stop(); }

const char *MtlRxVideoBackend::backend_name() const { return "mtl"; }

MtlRxVideoSupportPolicy default_mtl_rx_video_support_policy() {
    return MtlRxVideoSupportPolicy{
        .project_delivery = default_video_project_delivery_support_policy(),
        .require_mtl_session_packing_mode_support = true,
        .require_progressive_scan_mode = false,
        .require_single_stream_topology = true,
        .require_90khz_rtp_clock = true,
        .require_project_handoff_format_support = true,
    };
}

Error validate_mtl_rx_video_packing_mode_support(VideoPackingMode mode) noexcept {
    return validate_runtime_video_packing_mode_support(mode);
}

Error validate_mtl_rx_video_receive_capability_session_support(const VideoReceiveCapability &capability,
                                                               const MtlRxVideoSupportPolicy &support) noexcept {
    if (Error err = validate_video_receive_capability_structure(capability); err != Error::Ok) {
        return err;
    }

    if (support.require_mtl_session_packing_mode_support) {
        if (Error err = validate_mtl_rx_video_packing_mode_support(capability.packing_mode); err != Error::Ok) {
            return err;
        }
    }

    if (support.require_progressive_scan_mode && capability.scan_mode != VideoScanMode::Progressive) {
        return Error::Unsupported;
    }

    if (support.require_single_stream_topology &&
        (capability.topology.kind != VideoReceiveTopologyKind::SingleStream || capability.topology.stream_count != 1)) {
        return Error::Unsupported;
    }

    if (support.require_90khz_rtp_clock && capability.rtp_clock.rtp_clock_rate != 90000) {
        return Error::Unsupported;
    }

    return Error::Ok;
}

Error validate_mtl_rx_video_receive_capability_project_projection_support(
    const VideoReceiveCapability &capability, const MtlRxVideoSupportPolicy &support) noexcept {
    if (Error err = validate_video_receive_capability_structure(capability); err != Error::Ok) {
        return err;
    }

    if (support.require_project_handoff_format_support) {
        if (Error err = validate_video_frame_handoff_format_matches_media_description(capability.handoff_format,
                                                                                      capability.media);
            err != Error::Ok) {
            return err;
        }

        if (Error err = validate_project_video_frame_handoff_format_support(capability.handoff_format);
            err != Error::Ok) {
            return err;
        }
    }

    return Error::Ok;
}

Error validate_mtl_rx_video_config_support(const RxVideoConfig &cfg, const MtlRxVideoSupportPolicy &support) {
    if (Error err = validate_rx_video_config_against_project_delivery_support(cfg, support.project_delivery);
        err != Error::Ok) {
        return err;
    }

    auto capability = rx_video_config_effective_receive_capability(cfg);
    if (!capability.has_value()) {
        return capability.error();
    }

    if (Error err = validate_mtl_rx_video_receive_capability_session_support(*capability, support); err != Error::Ok) {
        return err;
    }

    return validate_mtl_rx_video_receive_capability_project_projection_support(*capability, support);
}

RxBackendLifecycleResult MtlRxVideoBackend::start_video(const RxVideoConfig &cfg, IVideoFrameSink &sink) {
    (void)sink;

    std::lock_guard lock(mutex_);

    if (const Error err = validate_start_state_locked(); err != Error::Ok) {
        return std::unexpected(err);
    }

    auto projected = project_video_start_config(cfg);
    if (!projected.has_value()) {
        return std::unexpected(projected.error());
    }

    /*
     * Task 131 currently establishes the explicit MTL support/projection split:
     * - common structural validity;
     * - MTL session support;
     * - MTL project/start projection support;
     * - current VideoFrameView delivery support.
     *
     * Actual MTL device/session creation and runtime start/stop behavior
     * remain for later implementation steps.
     */
    return std::unexpected(Error::Unsupported);
}

RxBackendLifecycleResult MtlRxVideoBackend::stop() {
    std::lock_guard lock(mutex_);

    if (backend_is_stopped(state_) && video_sink_ == nullptr && device_ == nullptr && session_ == nullptr) {
        return state_;
    }

    reset_runtime_locked();
    return state_;
}

RxBackendState MtlRxVideoBackend::state() const {
    std::lock_guard lock(mutex_);
    return state_;
}

RxBackendCapabilities MtlRxVideoBackend::capabilities() const {
    return RxBackendCapabilities{
        .video_rx = true,
        .audio_rx = false,
    };
}

BackendStats MtlRxVideoBackend::stats() const {
    std::lock_guard lock(mutex_);
    return stats_;
}

Error MtlRxVideoBackend::validate_start_state_locked() const noexcept {
    if (!backend_is_stopped(state_)) {
        return Error::InvalidBackendState;
    }

    if (video_sink_ != nullptr) {
        return Error::InvalidBackendState;
    }

    if (device_ != nullptr) {
        return Error::InvalidBackendState;
    }

    if (session_ != nullptr) {
        return Error::InvalidBackendState;
    }

    return Error::Ok;
}

Error MtlRxVideoBackend::validate_video_frame_view_delivery_support(const RxVideoConfig &cfg,
                                                                    const VideoReceiveCapability &capability) noexcept {
    if (Error err = validate_project_video_frame_storage_compatibility(capability, cfg.format); err != Error::Ok) {
        return err;
    }

    switch (cfg.format) {
    case PixelFormat::UYVY:
        break;
    default:
        return Error::InvalidValue;
    }

    switch (capability.scan_mode) {
    case VideoScanMode::Progressive:
        break;
    case VideoScanMode::Interlaced:
    case VideoScanMode::PsF:
        return Error::Unsupported;
    default:
        return Error::InvalidValue;
    }

    switch (capability.packing_mode) {
    case VideoPackingMode::Gpm:
        return Error::Ok;
    case VideoPackingMode::Bpm:
    case VideoPackingMode::GpmSingleLine:
        return Error::Unsupported;
    default:
        return Error::InvalidValue;
    }
}

Error MtlRxVideoBackend::validate_projected_video_start_support(const RxVideoConfig &cfg) {
    if (const Error err = validate_mtl_rx_video_config_support(cfg, default_mtl_rx_video_support_policy());
        err != Error::Ok) {
        return err;
    }

    return Error::Ok;
}

std::expected<bool, Error> MtlRxVideoBackend::scan_mode_maps_to_mtl_interlaced(VideoScanMode scan_mode) noexcept {
    switch (scan_mode) {
    case VideoScanMode::Progressive:
        return false;
    case VideoScanMode::Interlaced:
        return true;
    case VideoScanMode::PsF:
        return false;
    default:
        return std::unexpected(Error::InvalidValue);
    }
}

std::expected<std::uint16_t, Error>
MtlRxVideoBackend::session_port_count_from_receive_topology(const VideoReceiveTopology &topology) noexcept {
    if (Error err = validate_video_receive_topology(topology); err != Error::Ok) {
        return std::unexpected(err);
    }

    switch (topology.kind) {
    case VideoReceiveTopologyKind::SingleStream:
        return static_cast<std::uint16_t>(1);

    case VideoReceiveTopologyKind::RedundantStreams:
        return std::unexpected(Error::Unsupported);

    default:
        return std::unexpected(Error::InvalidValue);
    }
}

std::expected<MtlRxVideoBackend::ProjectedVideoStartConfig, Error>
MtlRxVideoBackend::project_video_start_config(const RxVideoConfig &cfg) {
    if (const Error err = validate_projected_video_start_support(cfg); err != Error::Ok) {
        return std::unexpected(err);
    }

    auto capability = rx_video_config_effective_receive_capability(cfg);
    if (!capability.has_value()) {
        return std::unexpected(capability.error());
    }

    auto project_format = project_pixel_format_from_video_frame_handoff_format(capability->handoff_format);
    if (!project_format.has_value()) {
        return std::unexpected(project_format.error());
    }

    auto mtl_interlaced = scan_mode_maps_to_mtl_interlaced(capability->scan_mode);
    if (!mtl_interlaced.has_value()) {
        return std::unexpected(mtl_interlaced.error());
    }

    auto session_port_count = session_port_count_from_receive_topology(capability->topology);
    if (!session_port_count.has_value()) {
        return std::unexpected(session_port_count.error());
    }

    ProjectedVideoStartConfig projected{};
    projected.receive_capability = *capability;
    projected.width = capability->media.width;
    projected.height = capability->media.height;
    projected.fps_num = capability->media.fps_num;
    projected.fps_den = capability->media.fps_den;
    projected.payload_type = cfg.payload_type;
    projected.local_ip = cfg.local_ip;
    projected.dest_ip = cfg.dest_ip;
    projected.pixel_format = *project_format;
    projected.handoff_format = capability->handoff_format;
    projected.transport_format = capability->transport_format;
    projected.scan_mode = capability->scan_mode;
    projected.packing_mode = capability->packing_mode;
    projected.mtl_interlaced = *mtl_interlaced;
    projected.session_port_count = *session_port_count;
    projected.frame_buffer_count = kDefaultFrameBufferCount;

    return projected;
}

void MtlRxVideoBackend::reset_runtime_locked() noexcept {
    /*
     * Release order is intentional:
     * session state must go away before device state.
     */
    session_.reset();
    device_.reset();

    video_sink_ = nullptr;
    state_ = {};
    stats_ = {};
}

} // namespace st2110
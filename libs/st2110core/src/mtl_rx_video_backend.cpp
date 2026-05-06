#include "st2110/mtl_rx_video_backend.hpp"

#include <utility>

namespace st2110 {
MtlRxVideoBackend::~MtlRxVideoBackend() { (void)stop(); }

const char *MtlRxVideoBackend::backend_name() const { return "mtl"; }

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
     * Task 131 / step 2 adds the explicit support/projection boundary only.
     * Actual MTL device/session creation and start/stop runtime behavior
     * remain in later steps.
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

Error MtlRxVideoBackend::validate_video_frame_view_compatibility(PixelFormat pixel_format, VideoScanMode scan_mode,
                                                                 VideoPackingMode packing_mode) noexcept {
    switch (scan_mode) {
    case VideoScanMode::Progressive:
        break;
    case VideoScanMode::Interlaced:
    case VideoScanMode::PsF:
        return Error::Unsupported;
    }

    if (scan_mode != VideoScanMode::Progressive) {
        return Error::InvalidValue;
    }

    switch (packing_mode) {
    case VideoPackingMode::Gpm:
        break;
    case VideoPackingMode::Bpm:
        return Error::Unsupported;
    }

    if (packing_mode != VideoPackingMode::Gpm) {
        return Error::InvalidValue;
    }

    switch (pixel_format) {
    case PixelFormat::UYVY:
        return Error::Ok;
    }

    return Error::InvalidValue;
}

Error MtlRxVideoBackend::validate_mtl_st20p_mvp_compatibility(const RxVideoConfig &cfg) noexcept {
    if (const Error err = validate_rx_video_config(cfg); err != Error::Ok) {
        return err;
    }

    return validate_video_frame_view_compatibility(cfg.format, cfg.scan_mode, cfg.packing_mode);
}

bool MtlRxVideoBackend::scan_mode_maps_to_mtl_interlaced(VideoScanMode scan_mode) noexcept {
    switch (scan_mode) {
    case VideoScanMode::Progressive:
        return false;
    case VideoScanMode::Interlaced:
        return true;
    case VideoScanMode::PsF:
        return false;
    }

    return false;
}

std::expected<MtlRxVideoBackend::ProjectedVideoStartConfig, Error>
MtlRxVideoBackend::project_video_start_config(const RxVideoConfig &cfg) {
    if (const Error err = validate_mtl_st20p_mvp_compatibility(cfg); err != Error::Ok) {
        return std::unexpected(err);
    }

    ProjectedVideoStartConfig projected{};
    projected.width = cfg.width;
    projected.height = cfg.height;
    projected.fps_num = cfg.fps_num;
    projected.fps_den = cfg.fps_den;
    projected.payload_type = cfg.payload_type;
    projected.local_ip = cfg.local_ip;
    projected.dest_ip = cfg.dest_ip;
    projected.pixel_format = cfg.format;
    projected.scan_mode = cfg.scan_mode;
    projected.packing_mode = cfg.packing_mode;
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
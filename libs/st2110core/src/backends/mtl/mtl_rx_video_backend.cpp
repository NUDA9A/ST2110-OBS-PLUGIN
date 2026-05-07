#include "st2110/backends/mtl/mtl_rx_video_backend.hpp"

#include <mtl/st_pipeline_api.h>

#include <utility>

namespace st2110 {
MtlRxVideoBackend::~MtlRxVideoBackend() { (void)stop(); }

const char *MtlRxVideoBackend::backend_name() const { return "mtl"; }

MtlRxVideoSupportPolicy default_mtl_rx_video_support_policy() {
    return MtlRxVideoSupportPolicy{
        .require_mtl_session_packing_mode_support = true,
        .require_progressive_scan_mode = false,
        .require_single_stream_topology = true,
        .require_90khz_rtp_clock = true,
        .require_mtl_project_handoff_format_support = true,
    };
}

namespace {

std::expected<st20_fmt, Error>
mtl_st20_transport_fmt_from_video_transport_format(VideoTransportPayloadFormat format) noexcept {
    if (const Error err = validate_video_transport_payload_format(format); err != Error::Ok) {
        return std::unexpected(err);
    }

    switch (format) {
    case VideoTransportPayloadFormat::Rfc4175Ycbcr422_8Bit:
        return ST20_FMT_YUV_422_8BIT;
    case VideoTransportPayloadFormat::Rfc4175Ycbcr422_10Bit:
        return ST20_FMT_YUV_422_10BIT;
    case VideoTransportPayloadFormat::Rfc4175Ycbcr422_12Bit:
        return ST20_FMT_YUV_422_12BIT;
    case VideoTransportPayloadFormat::Rfc4175Ycbcr422_16Bit:
        return ST20_FMT_YUV_422_16BIT;
    case VideoTransportPayloadFormat::Rfc4175Ycbcr420_8Bit:
        return ST20_FMT_YUV_420_8BIT;
    case VideoTransportPayloadFormat::Rfc4175Ycbcr420_10Bit:
        return ST20_FMT_YUV_420_10BIT;
    case VideoTransportPayloadFormat::Rfc4175Ycbcr420_12Bit:
        return ST20_FMT_YUV_420_12BIT;
    case VideoTransportPayloadFormat::Rfc4175Ycbcr420_16Bit:
        return ST20_FMT_YUV_420_16BIT;
    case VideoTransportPayloadFormat::Rfc4175Rgb_8Bit:
        return ST20_FMT_RGB_8BIT;
    case VideoTransportPayloadFormat::Rfc4175Rgb_10Bit:
        return ST20_FMT_RGB_10BIT;
    case VideoTransportPayloadFormat::Rfc4175Rgb_12Bit:
        return ST20_FMT_RGB_12BIT;
    case VideoTransportPayloadFormat::Rfc4175Rgb_16Bit:
        return ST20_FMT_RGB_16BIT;
    case VideoTransportPayloadFormat::Rfc4175Ycbcr444_8Bit:
        return ST20_FMT_YUV_444_8BIT;
    case VideoTransportPayloadFormat::Rfc4175Ycbcr444_10Bit:
        return ST20_FMT_YUV_444_10BIT;
    case VideoTransportPayloadFormat::Rfc4175Ycbcr444_12Bit:
        return ST20_FMT_YUV_444_12BIT;
    case VideoTransportPayloadFormat::Rfc4175Ycbcr444_16Bit:
        return ST20_FMT_YUV_444_16BIT;
    case VideoTransportPayloadFormat::CustomYcbcr422Planar10Le:
        return ST20_FMT_YUV_422_PLANAR10LE;
    case VideoTransportPayloadFormat::CustomV210:
        return ST20_FMT_V210;
    case VideoTransportPayloadFormat::Rfc4175:
        return std::unexpected(Error::Unsupported);
    default:
        return std::unexpected(Error::InvalidValue);
    }
}

std::expected<st_frame_fmt, Error> mtl_st_frame_fmt_from_video_handoff_format(VideoFrameHandoffFormat format) noexcept {
    if (const Error err = validate_video_frame_handoff_format(format); err != Error::Ok) {
        return std::unexpected(err);
    }

    switch (format) {
    case VideoFrameHandoffFormat::Uyvy:
        return ST_FRAME_FMT_UYVY;
    case VideoFrameHandoffFormat::Yuv422Planar8:
        return ST_FRAME_FMT_YUV422PLANAR8;
    case VideoFrameHandoffFormat::Yuv422Planar10Le:
        return ST_FRAME_FMT_YUV422PLANAR10LE;
    case VideoFrameHandoffFormat::Yuv422Planar12Le:
        return ST_FRAME_FMT_YUV422PLANAR12LE;
    case VideoFrameHandoffFormat::Yuv422Planar16Le:
        return ST_FRAME_FMT_YUV422PLANAR16LE;
    case VideoFrameHandoffFormat::V210:
        return ST_FRAME_FMT_V210;
    case VideoFrameHandoffFormat::Y210:
        return ST_FRAME_FMT_Y210;
    case VideoFrameHandoffFormat::Yuv422Rfc4175Pgroup2Be10:
        return ST_FRAME_FMT_YUV422RFC4175PG2BE10;
    case VideoFrameHandoffFormat::Yuv422Rfc4175Pgroup2Be12:
        return ST_FRAME_FMT_YUV422RFC4175PG2BE12;
    case VideoFrameHandoffFormat::Yuv444Planar10Le:
        return ST_FRAME_FMT_YUV444PLANAR10LE;
    case VideoFrameHandoffFormat::Yuv444Planar12Le:
        return ST_FRAME_FMT_YUV444PLANAR12LE;
    case VideoFrameHandoffFormat::Yuv444Rfc4175Pgroup4Be10:
        return ST_FRAME_FMT_YUV444RFC4175PG4BE10;
    case VideoFrameHandoffFormat::Yuv444Rfc4175Pgroup2Be12:
        return ST_FRAME_FMT_YUV444RFC4175PG2BE12;
    case VideoFrameHandoffFormat::Yuv420Planar8:
        return ST_FRAME_FMT_YUV420PLANAR8;
    case VideoFrameHandoffFormat::Argb:
        return ST_FRAME_FMT_ARGB;
    case VideoFrameHandoffFormat::Bgra:
        return ST_FRAME_FMT_BGRA;
    case VideoFrameHandoffFormat::Rgb8:
        return ST_FRAME_FMT_RGB8;
    case VideoFrameHandoffFormat::GbrPlanar10Le:
        return ST_FRAME_FMT_GBRPLANAR10LE;
    case VideoFrameHandoffFormat::GbrPlanar12Le:
        return ST_FRAME_FMT_GBRPLANAR12LE;
    case VideoFrameHandoffFormat::RgbRfc4175Pgroup4Be10:
        return ST_FRAME_FMT_RGBRFC4175PG4BE10;
    case VideoFrameHandoffFormat::RgbRfc4175Pgroup2Be12:
        return ST_FRAME_FMT_RGBRFC4175PG2BE12;
    default:
        return std::unexpected(Error::InvalidValue);
    }
}

std::expected<st_fps, Error> mtl_st_fps_from_video_rate(std::uint32_t fps_num, std::uint32_t fps_den,
                                                        VideoScanMode scan_mode) noexcept {
    if (const Error err = config_validation::validate_frame_rate(fps_num, fps_den); err != Error::Ok) {
        return std::unexpected(err);
    }

    if (const Error err = config_validation::validate_video_scan_mode(scan_mode); err != Error::Ok) {
        return std::unexpected(err);
    }

    double frame_rate = static_cast<double>(fps_num) / static_cast<double>(fps_den);

    switch (scan_mode) {
    case VideoScanMode::Progressive:
    case VideoScanMode::PsF:
        break;
    case VideoScanMode::Interlaced:
        frame_rate *= 2.0;
        break;
    default:
        return std::unexpected(Error::InvalidValue);
    }

    const st_fps fps = st_frame_rate_to_st_fps(frame_rate);
    if (fps == ST_FPS_MAX) {
        return std::unexpected(Error::Unsupported);
    }

    return fps;
}

} // namespace

Error validate_mtl_rx_video_session_packing_mode_implementation_support(VideoPackingMode mode) noexcept {
    return validate_video_packing_mode(mode);
}

Error validate_mtl_rx_video_receive_capability_session_implementation_support(
    const VideoReceiveCapability &capability, const MtlRxVideoSupportPolicy &support) noexcept {
    if (Error err = validate_video_receive_capability_structure(capability); err != Error::Ok) {
        return err;
    }

    if (support.require_mtl_session_packing_mode_support) {
        if (Error err = validate_mtl_rx_video_session_packing_mode_implementation_support(capability.packing_mode);
            err != Error::Ok) {
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

Error validate_mtl_rx_video_backend_support_matrix_project_projection_implementation_support(
    const CommonVideoBackendSupportMatrix &matrix, const MtlRxVideoSupportPolicy &support) noexcept {
    if (Error err = validate_common_video_backend_support_matrix(matrix); err != Error::Ok) {
        return err;
    }

    if (support.require_mtl_project_handoff_format_support) {
        if (Error err = validate_project_video_frame_handoff_format_matches_pixel_format(
                matrix.receive_capability.handoff_format, matrix.project_pixel_format);
            err != Error::Ok) {
            return err;
        }

        if (Error err = validate_project_video_frame_handoff_format_support(matrix.receive_capability.handoff_format);
            err != Error::Ok) {
            return err;
        }
    }

    return Error::Ok;
}

Error validate_mtl_rx_video_backend_support_matrix_implementation_support(
    const CommonVideoBackendSupportMatrix &matrix, const MtlRxVideoSupportPolicy &support) noexcept {
    if (Error err = validate_common_video_backend_support_matrix(matrix); err != Error::Ok) {
        return err;
    }

    if (Error err =
            validate_mtl_rx_video_receive_capability_session_implementation_support(matrix.receive_capability, support);
        err != Error::Ok) {
        return err;
    }

    return validate_mtl_rx_video_backend_support_matrix_project_projection_implementation_support(matrix, support);
}

Error validate_mtl_rx_video_config_support(const RxVideoConfig &cfg, const MtlRxVideoSupportPolicy &support) {
    auto matrix = common_video_backend_support_matrix_from_rx_video_config(cfg);
    if (!matrix.has_value()) {
        return matrix.error();
    }

    return validate_mtl_rx_video_backend_support_matrix_implementation_support(*matrix, support);
}

RxBackendLifecycleResult MtlRxVideoBackend::start_video(const RxVideoConfig &cfg, IVideoFrameSink &sink) {
    (void)sink;

    std::lock_guard lock(mutex_);

    if (const Error err = validate_start_state_locked(); err != Error::Ok) {
        return std::unexpected(err);
    }

    auto projected = project_mtl_video_session_config(cfg);
    if (!projected.has_value()) {
        return std::unexpected(projected.error());
    }

    st20p_rx_ops ops{};

    ops.port.num_port = static_cast<std::uint8_t>(projected->common.session_port_count);
    ops.port.udp_port[MTL_SESSION_PORT_P] = projected->session_runtime.primary_udp_port;
    ops.port.payload_type = projected->common.payload_type;

    ops.width = projected->common.width;
    ops.height = projected->common.height;
    ops.interlaced = projected->mtl_interlaced;
    ops.framebuff_cnt = projected->session_runtime.frame_buffer_count;
    ops.device = ST_PLUGIN_DEVICE_AUTO;

    auto transport_fmt = mtl_st20_transport_fmt_from_video_transport_format(projected->common.transport_format);
    if (!transport_fmt.has_value()) {
        return std::unexpected(transport_fmt.error());
    }
    ops.transport_fmt = *transport_fmt;

    auto output_fmt = mtl_st_frame_fmt_from_video_handoff_format(projected->common.handoff_format);
    if (!output_fmt.has_value()) {
        return std::unexpected(output_fmt.error());
    }
    ops.output_fmt = *output_fmt;

    auto fps =
        mtl_st_fps_from_video_rate(projected->common.fps_num, projected->common.fps_den, projected->common.scan_mode);
    if (!fps.has_value()) {
        return std::unexpected(fps.error());
    }
    ops.fps = *fps;

    ops.flags = 0;
    if (projected->session_runtime.enable_block_get) {
        ops.flags |= ST20P_RX_FLAG_BLOCK_GET;
    }
    if (projected->session_runtime.receive_incomplete_frame) {
        ops.flags |= ST20P_RX_FLAG_RECEIVE_INCOMPLETE_FRAME;
    }
    if (projected->session_runtime.enable_rtcp) {
        ops.flags |= ST20P_RX_FLAG_ENABLE_RTCP;
    }
    if (projected->session_runtime.enable_timing_parser_stat) {
        ops.flags |= ST20P_RX_FLAG_TIMING_PARSER_STAT;
    }
    if (projected->session_runtime.enable_timing_parser_meta) {
        ops.flags |= ST20P_RX_FLAG_TIMING_PARSER_META;
    }

    (void)ops;

    /*
     * Task 131D step 2 now projects the common video/session state into
     * concrete ST20P RX ops fields through .cpp-local MTL mapping helpers.
     *
     * Actual MTL device/session creation and runtime start/stop still remain
     * for the next step(s).
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
    if (Error err = validate_video_receive_capability_structure(capability); err != Error::Ok) {
        return err;
    }

    if (auto project_handoff = video_frame_handoff_format_from_project_pixel_format(cfg.format);
        !project_handoff.has_value()) {
        return project_handoff.error();
    }

    if (Error err =
            validate_project_video_frame_handoff_format_matches_pixel_format(capability.handoff_format, cfg.format);
        err != Error::Ok) {
        return err;
    }

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

Error MtlRxVideoBackend::validate_projected_common_video_support(const RxVideoConfig &cfg) {
    return validate_mtl_rx_video_config_support(cfg, default_mtl_rx_video_support_policy());
}

std::expected<bool, Error>
MtlRxVideoBackend::scan_mode_maps_to_mtl_interlaced(VideoScanMode scan_mode) noexcept {
    if (const Error err = config_validation::validate_video_scan_mode(scan_mode); err != Error::Ok) {
        return std::unexpected(err);
    }

    switch (scan_mode) {
    case VideoScanMode::Progressive:
    case VideoScanMode::PsF:
        return false;
    case VideoScanMode::Interlaced:
        return true;
    default:
        return std::unexpected(Error::InvalidValue);
    }
}

std::expected<std::uint16_t, Error>
MtlRxVideoBackend::session_port_count_from_receive_topology(const VideoReceiveTopology &topology) noexcept {
    if (const Error err = validate_video_receive_topology(topology); err != Error::Ok) {
        return std::unexpected(err);
    }

    switch (topology.kind) {
    case VideoReceiveTopologyKind::SingleStream:
        return 1;

    case VideoReceiveTopologyKind::RedundantStreams:
        if (topology.stream_count == 2) {
            return 2;
        }
        return std::unexpected(Error::Unsupported);

    default:
        return std::unexpected(Error::InvalidValue);
    }
}

std::expected<MtlRxVideoBackend::ProjectedCommonVideoConfig, Error>
MtlRxVideoBackend::project_common_video_config(const RxVideoConfig &cfg) {
    if (const Error err = validate_projected_common_video_support(cfg); err != Error::Ok) {
        return std::unexpected(err);
    }

    auto matrix = common_video_backend_support_matrix_from_rx_video_config(cfg);
    if (!matrix.has_value()) {
        return std::unexpected(matrix.error());
    }

    const auto &capability = matrix->receive_capability;

    auto session_port_count = session_port_count_from_receive_topology(capability.topology);
    if (!session_port_count.has_value()) {
        return std::unexpected(session_port_count.error());
    }

    ProjectedCommonVideoConfig projected{};
    projected.receive_capability = capability;
    projected.payload_type = cfg.payload_type;
    projected.local_ip = cfg.local_ip;
    projected.dest_ip = cfg.dest_ip;
    projected.project_pixel_format = matrix->project_pixel_format;
    projected.handoff_format = capability.handoff_format;
    projected.transport_format = capability.transport_format;
    projected.scan_mode = capability.scan_mode;
    projected.packing_mode = capability.packing_mode;
    projected.width = capability.media.width;
    projected.height = capability.media.height;
    projected.fps_num = capability.media.fps_num;
    projected.fps_den = capability.media.fps_den;
    projected.session_port_count = *session_port_count;

    return projected;
}

MtlRxVideoBackend::MtlDeviceRuntimeConfig MtlRxVideoBackend::default_mtl_device_runtime_config() noexcept {
    return MtlDeviceRuntimeConfig{
        .primary_port = MtlDevicePortRuntimeConfig{},
        .redundant_port = std::nullopt,
        .auto_start_stop = true,
        .enable_hw_timestamp = false,
        .socket_id = -1,
        .lcores = {},
    };
}

std::expected<MtlRxVideoBackend::MtlSessionRuntimeConfig, Error>
MtlRxVideoBackend::default_mtl_session_runtime_config(const RxVideoConfig &cfg,
                                                      const ProjectedCommonVideoConfig &common) {
    if (const Error err = validate_projected_common_video_support(cfg); err != Error::Ok) {
        return std::unexpected(err);
    }

    if (cfg.udp_port == 0) {
        return std::unexpected(Error::InvalidValue);
    }

    switch (common.session_port_count) {
    case 1:
        break;
    case 2:
        return std::unexpected(Error::Unsupported);
    default:
        return std::unexpected(Error::InvalidValue);
    }

    return MtlSessionRuntimeConfig{
        .primary_udp_port = cfg.udp_port,
        .redundant_udp_port = std::nullopt,
        .frame_buffer_count = kDefaultFrameBufferCount,
        .enable_block_get = true,
        .receive_incomplete_frame = false,
        .enable_rtcp = false,
        .enable_timing_parser_stat = false,
        .enable_timing_parser_meta = false,
    };
}

std::expected<MtlRxVideoBackend::ProjectedMtlVideoSessionConfig, Error>
MtlRxVideoBackend::project_mtl_video_session_config(const ProjectedCommonVideoConfig &common,
                                                    const MtlDeviceRuntimeConfig &device_runtime,
                                                    const MtlSessionRuntimeConfig &session_runtime) {
    CommonVideoBackendSupportMatrix matrix{
        .project_pixel_format = common.project_pixel_format,
        .receive_capability = common.receive_capability,
    };

    if (const Error err = validate_mtl_rx_video_backend_support_matrix_implementation_support(
            matrix, default_mtl_rx_video_support_policy());
        err != Error::Ok) {
        return std::unexpected(err);
    }

    if (common.width != common.receive_capability.media.width) {
        return std::unexpected(Error::InvalidValue);
    }

    if (common.height != common.receive_capability.media.height) {
        return std::unexpected(Error::InvalidValue);
    }

    if (common.fps_num != common.receive_capability.media.fps_num) {
        return std::unexpected(Error::InvalidValue);
    }

    if (common.fps_den != common.receive_capability.media.fps_den) {
        return std::unexpected(Error::InvalidValue);
    }

    if (common.handoff_format != common.receive_capability.handoff_format) {
        return std::unexpected(Error::InvalidValue);
    }

    if (common.transport_format != common.receive_capability.transport_format) {
        return std::unexpected(Error::InvalidValue);
    }

    if (common.scan_mode != common.receive_capability.scan_mode) {
        return std::unexpected(Error::InvalidValue);
    }

    if (common.packing_mode != common.receive_capability.packing_mode) {
        return std::unexpected(Error::InvalidValue);
    }

    if (!config_validation::is_dynamic_rtp_payload_type(common.payload_type)) {
        return std::unexpected(Error::InvalidValue);
    }

    if (common.session_port_count == 0) {
        return std::unexpected(Error::InvalidValue);
    }

    if (session_runtime.primary_udp_port == 0) {
        return std::unexpected(Error::InvalidValue);
    }

    if (session_runtime.frame_buffer_count == 0) {
        return std::unexpected(Error::InvalidValue);
    }

    switch (common.session_port_count) {
    case 1:
        if (device_runtime.redundant_port.has_value() || session_runtime.redundant_udp_port.has_value()) {
            return std::unexpected(Error::InvalidValue);
        }
        break;
    case 2:
        if (!device_runtime.redundant_port.has_value() || !session_runtime.redundant_udp_port.has_value()) {
            return std::unexpected(Error::InvalidValue);
        }
        break;
    default:
        return std::unexpected(Error::InvalidValue);
    }

    auto mtl_interlaced = scan_mode_maps_to_mtl_interlaced(common.scan_mode);
    if (!mtl_interlaced.has_value()) {
        return std::unexpected(mtl_interlaced.error());
    }

    ProjectedMtlVideoSessionConfig projected{
        .common = common,
        .device_runtime = device_runtime,
        .session_runtime = session_runtime,
        .mtl_interlaced = *mtl_interlaced,
    };

    if (const Error err = validate_projected_mtl_video_session_config(projected); err != Error::Ok) {
        return std::unexpected(err);
    }

    return projected;
}

std::expected<MtlRxVideoBackend::ProjectedMtlVideoSessionConfig, Error>
MtlRxVideoBackend::project_mtl_video_session_config(const RxVideoConfig &cfg) {
    auto common = project_common_video_config(cfg);
    if (!common.has_value()) {
        return std::unexpected(common.error());
    }

    auto session_runtime = default_mtl_session_runtime_config(cfg, *common);
    if (!session_runtime.has_value()) {
        return std::unexpected(session_runtime.error());
    }

    auto device_runtime = default_mtl_device_runtime_config();
    return project_mtl_video_session_config(*common, device_runtime, *session_runtime);
}

Error MtlRxVideoBackend::validate_projected_mtl_video_session_config(
    const ProjectedMtlVideoSessionConfig &cfg) noexcept {
    CommonVideoBackendSupportMatrix matrix{
        .project_pixel_format = cfg.common.project_pixel_format,
        .receive_capability = cfg.common.receive_capability,
    };

    if (Error err = validate_mtl_rx_video_backend_support_matrix_implementation_support(
            matrix, default_mtl_rx_video_support_policy());
        err != Error::Ok) {
        return err;
    }

    if (cfg.common.width != cfg.common.receive_capability.media.width) {
        return Error::InvalidValue;
    }

    if (cfg.common.height != cfg.common.receive_capability.media.height) {
        return Error::InvalidValue;
    }

    if (cfg.common.fps_num != cfg.common.receive_capability.media.fps_num) {
        return Error::InvalidValue;
    }

    if (cfg.common.fps_den != cfg.common.receive_capability.media.fps_den) {
        return Error::InvalidValue;
    }

    if (cfg.common.handoff_format != cfg.common.receive_capability.handoff_format) {
        return Error::InvalidValue;
    }

    if (cfg.common.transport_format != cfg.common.receive_capability.transport_format) {
        return Error::InvalidValue;
    }

    if (cfg.common.scan_mode != cfg.common.receive_capability.scan_mode) {
        return Error::InvalidValue;
    }

    if (cfg.common.packing_mode != cfg.common.receive_capability.packing_mode) {
        return Error::InvalidValue;
    }

    if (!config_validation::is_dynamic_rtp_payload_type(cfg.common.payload_type)) {
        return Error::InvalidValue;
    }

    if (cfg.common.session_port_count == 0) {
        return Error::InvalidValue;
    }

    if (cfg.session_runtime.primary_udp_port == 0) {
        return Error::InvalidValue;
    }

    if (cfg.session_runtime.frame_buffer_count == 0) {
        return Error::InvalidValue;
    }

    switch (cfg.common.scan_mode) {
    case VideoScanMode::Progressive:
        if (cfg.mtl_interlaced) {
            return Error::InvalidValue;
        }
        break;
    case VideoScanMode::Interlaced:
        if (!cfg.mtl_interlaced) {
            return Error::InvalidValue;
        }
        break;
    case VideoScanMode::PsF:
        if (cfg.mtl_interlaced) {
            return Error::InvalidValue;
        }
        break;
    default:
        return Error::InvalidValue;
    }

    switch (cfg.common.session_port_count) {
    case 1:
        if (cfg.device_runtime.redundant_port.has_value() || cfg.session_runtime.redundant_udp_port.has_value()) {
            return Error::InvalidValue;
        }
        break;
    case 2:
        if (!cfg.device_runtime.redundant_port.has_value() || !cfg.session_runtime.redundant_udp_port.has_value()) {
            return Error::InvalidValue;
        }
        break;
    default:
        return Error::InvalidValue;
    }

    return Error::Ok;
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
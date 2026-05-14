#include <st2110/backends/mtl/mtl_rx_video_backend.hpp>

#include <st2110/delivery/video/pixel_format.hpp>
#include <st2110/delivery/video/video_frame.hpp>
#include <st2110/foundation/error.hpp>
#include <st2110/foundation/timestamp.hpp>

#include <mtl/mtl_api.h>
#include <mtl/st20_api.h>
#include <mtl/st_pipeline_api.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <expected>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

#ifndef ST2110_MTL_DEV_KERNEL_SOCKET
#define ST2110_MTL_DEV_KERNEL_SOCKET 0
#endif

namespace st2110 {
namespace {
[[nodiscard]] constexpr mtl_pmd_type default_mtl_pmd() noexcept {
#if ST2110_MTL_DEV_KERNEL_SOCKET
    return MTL_PMD_KERNEL_SOCKET;
#else
    return MTL_PMD_DPDK_USER;
#endif
}

[[nodiscard]] std::expected<st_fps, Error> map_mtl_video_frame_rate(const MtlVideoFrameRate fps) noexcept {
    switch (fps) {
    case MtlVideoFrameRate::P23_98:
        return ST_FPS_P23_98;
    case MtlVideoFrameRate::P24:
        return ST_FPS_P24;
    case MtlVideoFrameRate::P25:
        return ST_FPS_P25;
    case MtlVideoFrameRate::P29_97:
        return ST_FPS_P29_97;
    case MtlVideoFrameRate::P30:
        return ST_FPS_P30;
    case MtlVideoFrameRate::P50:
        return ST_FPS_P50;
    case MtlVideoFrameRate::P59_94:
        return ST_FPS_P59_94;
    case MtlVideoFrameRate::P60:
        return ST_FPS_P60;
    case MtlVideoFrameRate::P100:
        return ST_FPS_P100;
    case MtlVideoFrameRate::P119_88:
        return ST_FPS_P119_88;
    case MtlVideoFrameRate::P120:
        return ST_FPS_P120;
    }

    return std::unexpected(Error::Unsupported);
}

[[nodiscard]] std::expected<st20_fmt, Error>
map_mtl_video_transport_format(const MtlVideoTransportFormat fmt) noexcept {
    switch (fmt) {
    case MtlVideoTransportFormat::Yuv422_8Bit:
        return ST20_FMT_YUV_422_8BIT;
    case MtlVideoTransportFormat::Yuv422_10Bit:
        return ST20_FMT_YUV_422_10BIT;
    case MtlVideoTransportFormat::Yuv422_12Bit:
        return ST20_FMT_YUV_422_12BIT;
    case MtlVideoTransportFormat::Yuv422_16Bit:
        return ST20_FMT_YUV_422_16BIT;
    case MtlVideoTransportFormat::Yuv420_8Bit:
        return ST20_FMT_YUV_420_8BIT;
    case MtlVideoTransportFormat::Yuv420_10Bit:
        return ST20_FMT_YUV_420_10BIT;
    case MtlVideoTransportFormat::Yuv420_12Bit:
        return ST20_FMT_YUV_420_12BIT;
    case MtlVideoTransportFormat::Yuv420_16Bit:
        return ST20_FMT_YUV_420_16BIT;
    case MtlVideoTransportFormat::Yuv444_8Bit:
        return ST20_FMT_YUV_444_8BIT;
    case MtlVideoTransportFormat::Yuv444_10Bit:
        return ST20_FMT_YUV_444_10BIT;
    case MtlVideoTransportFormat::Yuv444_12Bit:
        return ST20_FMT_YUV_444_12BIT;
    case MtlVideoTransportFormat::Yuv444_16Bit:
        return ST20_FMT_YUV_444_16BIT;
    case MtlVideoTransportFormat::Rgb8:
        return ST20_FMT_RGB_8BIT;
    case MtlVideoTransportFormat::Rgb10:
        return ST20_FMT_RGB_10BIT;
    case MtlVideoTransportFormat::Rgb12:
        return ST20_FMT_RGB_12BIT;
    case MtlVideoTransportFormat::Rgb16:
        return ST20_FMT_RGB_16BIT;
    }

    return std::unexpected(Error::Unsupported);
}

[[nodiscard]] std::expected<st_frame_fmt, Error> map_mtl_video_output_format(const PixelFormat fmt) noexcept {
    switch (fmt) {
    case PixelFormat::UYVY:
        return ST_FRAME_FMT_UYVY;
    case PixelFormat::RGB8:
        return ST_FRAME_FMT_RGB8;

    case PixelFormat::YUV422RFC4175PG2BE10:
        return ST_FRAME_FMT_YUV422RFC4175PG2BE10;
    case PixelFormat::YUV422RFC4175PG2BE12:
        return ST_FRAME_FMT_YUV422RFC4175PG2BE12;
    case PixelFormat::YUV444RFC4175PG4BE10:
        return ST_FRAME_FMT_YUV444RFC4175PG4BE10;
    case PixelFormat::YUV444RFC4175PG2BE12:
        return ST_FRAME_FMT_YUV444RFC4175PG2BE12;
    case PixelFormat::RGBRFC4175PG4BE10:
        return ST_FRAME_FMT_RGBRFC4175PG4BE10;
    case PixelFormat::RGBRFC4175PG2BE12:
        return ST_FRAME_FMT_RGBRFC4175PG2BE12;

    case PixelFormat::YUV422PLANAR8:
        return ST_FRAME_FMT_YUV422PLANAR8;
    case PixelFormat::YUV422PLANAR10LE:
        return ST_FRAME_FMT_YUV422PLANAR10LE;
    case PixelFormat::YUV422PLANAR12LE:
        return ST_FRAME_FMT_YUV422PLANAR12LE;
    case PixelFormat::YUV422PLANAR16LE:
        return ST_FRAME_FMT_YUV422PLANAR16LE;
    case PixelFormat::YUV444PLANAR10LE:
        return ST_FRAME_FMT_YUV444PLANAR10LE;
    case PixelFormat::YUV444PLANAR12LE:
        return ST_FRAME_FMT_YUV444PLANAR12LE;
    case PixelFormat::YUV420PLANAR8:
        return ST_FRAME_FMT_YUV420PLANAR8;

    case PixelFormat::BGRA:
        return ST_FRAME_FMT_BGRA;
    case PixelFormat::ARGB:
        return ST_FRAME_FMT_ARGB;
    case PixelFormat::V210:
        return ST_FRAME_FMT_V210;
    case PixelFormat::Y210:
        return ST_FRAME_FMT_Y210;
    }

    return std::unexpected(Error::Unsupported);
}

[[nodiscard]] std::expected<mtl_init_params, Error> make_mtl_init_params(const MtlRuntimeConfig &cfg) noexcept {
    mtl_init_params params{};

    params.num_ports = cfg.redundant_port.has_value() ? 2 : 1;

    const auto fill_port = [](struct mtl_init_params &params, const enum mtl_port port_index,
                              const MtlRuntimePortConfig &port_cfg) {
        std::snprintf(params.port[port_index], MTL_PORT_MAX_LEN, "%s", port_cfg.port_name.c_str());

        params.pmd[port_index] = default_mtl_pmd();
        params.net_proto[port_index] = MTL_PROTO_STATIC;

        /*
         * RX video backend needs one RX queue per initialized MTL device port.
         * TX queues are not needed for RX-only video backend.
         */
        params.tx_queues_cnt[port_index] = 0;
        params.rx_queues_cnt[port_index] = 1;

        std::memcpy(params.sip_addr[port_index], port_cfg.sip_addr.data(), port_cfg.sip_addr.size());
    };

    fill_port(params, MTL_PORT_P, cfg.primary_port);

    if (cfg.redundant_port.has_value()) {
        fill_port(params, MTL_PORT_R, *cfg.redundant_port);
    }

    params.flags |= MTL_FLAG_DEV_AUTO_START_STOP;

    return params;
}

[[nodiscard]] bool mtl_interlaced_flag(const VideoScanMode scan_mode) noexcept {
    switch (scan_mode) {
    case VideoScanMode::Progressive:
        return false;
    case VideoScanMode::Interlaced:
    case VideoScanMode::PsF:
        return true;
    }

    return false;
}

void fill_st20p_session_port(st20p_rx_ops &ops, const mtl_session_port session_port,
                             const MtlRuntimePortConfig &runtime_port,
                             const MtlVideoSessionPortConfig &session_port_cfg) {
    std::memcpy(ops.port.ip_addr[session_port], session_port_cfg.ip_addr.data(), session_port_cfg.ip_addr.size());

    if (session_port_cfg.source_ip.has_value()) {
        std::memcpy(ops.port.mcast_sip_addr[session_port], session_port_cfg.source_ip->data(),
                    session_port_cfg.source_ip->size());
    }

    std::snprintf(ops.port.port[session_port], MTL_PORT_MAX_LEN, "%s", runtime_port.port_name.c_str());

    ops.port.udp_port[session_port] = session_port_cfg.udp_port;
}

[[nodiscard]] std::expected<st20p_rx_ops, Error> make_st20p_rx_ops(const MtlVideoStartConfig &cfg) noexcept {
    auto fps = map_mtl_video_frame_rate(cfg.fps);
    if (!fps.has_value()) {
        return std::unexpected(fps.error());
    }

    auto transport_fmt = map_mtl_video_transport_format(cfg.transport_format);
    if (!transport_fmt.has_value()) {
        return std::unexpected(transport_fmt.error());
    }

    auto output_fmt = map_mtl_video_output_format(cfg.output_format);
    if (!output_fmt.has_value()) {
        return std::unexpected(output_fmt.error());
    }

    st20p_rx_ops ops{};
    ops.name = "st2110_mtl_video_rx";
    ops.priv = nullptr;

    ops.port.num_port = cfg.redundant.has_value() ? 2 : 1;

    fill_st20p_session_port(ops, MTL_SESSION_PORT_P, cfg.runtime.primary_port, cfg.primary);

    if (cfg.redundant.has_value()) {
        fill_st20p_session_port(ops, MTL_SESSION_PORT_R, *cfg.runtime.redundant_port, *cfg.redundant);
    }

    ops.port.payload_type = cfg.expected_payload_type;

    ops.width = cfg.width;
    ops.height = cfg.height;
    ops.fps = *fps;
    ops.interlaced = mtl_interlaced_flag(cfg.scan_mode);

    ops.transport_fmt = *transport_fmt;
    ops.output_fmt = *output_fmt;

    ops.device = ST_PLUGIN_DEVICE_AUTO;
    ops.framebuff_cnt = cfg.frame_buffer_count;

    ops.flags = ST20P_RX_FLAG_BLOCK_GET;

    return ops;
}

[[nodiscard]] TimestampNs mtl_receive_timestamp_ns(const st_frame &frame) noexcept {
    return static_cast<TimestampNs>(frame.receive_timestamp);
}

} // namespace

class MtlRxVideoBackend::Impl {
  public:
    explicit Impl(MtlVideoStartConfig cfg) : cfg_(std::move(cfg)) {}

    ~Impl() { (void)stop(); }

    [[nodiscard]] RxBackendLifecycleResult start(IFrameSink *sink) {
        auto params = make_mtl_init_params(cfg_.runtime);
        if (!params.has_value()) {
            return std::unexpected(params.error());
        }

        mtl_handle staged_mt = mtl_init(&*params);
        if (!staged_mt) {
            return std::unexpected(Error::SystemFailure);
        }

        auto ops = make_st20p_rx_ops(cfg_);
        if (!ops.has_value()) {
            mtl_uninit(staged_mt);
            return std::unexpected(ops.error());
        }

        ops->priv = this;

        st20p_rx_handle staged_rx = st20p_rx_create(staged_mt, &*ops);
        if (!staged_rx) {
            mtl_uninit(staged_mt);
            return std::unexpected(Error::SystemFailure);
        }

        auto expected_output_format = map_mtl_video_output_format(cfg_.output_format);
        if (!expected_output_format.has_value()) {
            st20p_rx_free(staged_rx);
            mtl_uninit(staged_mt);
            return std::unexpected(expected_output_format.error());
        }

        {
            std::lock_guard lock(mutex_);

            sink_ = sink;
            mt_ = staged_mt;
            rx_ = staged_rx;
            expected_mtl_output_format_ = *expected_output_format;
            expected_project_format_ = cfg_.output_format;
            stop_requested_.store(false);
        }

        receive_thread_ = std::jthread([this](std::stop_token stop_token) { receive_loop(stop_token); });

        return true;
    }

    [[nodiscard]] RxBackendLifecycleResult stop() noexcept {
        st20p_rx_handle rx_to_wake = nullptr;
        std::jthread thread_to_join{};

        {
            std::lock_guard lock(mutex_);

            stop_requested_.store(true);

            rx_to_wake = rx_;
            thread_to_join = std::move(receive_thread_);
        }

        if (rx_to_wake) {
            st20p_rx_wake_block(rx_to_wake);
        }

        thread_to_join = {};

        {
            std::lock_guard lock(mutex_);

            if (rx_) {
                st20p_rx_free(rx_);
                rx_ = nullptr;
            }

            if (mt_) {
                mtl_uninit(mt_);
                mt_ = nullptr;
            }

            sink_ = nullptr;
        }

        return true;
    }

  private:
    void receive_loop(std::stop_token stop_token) noexcept {
        while (!stop_token.stop_requested() && !stop_requested_.load()) {
            st_frame *frame = st20p_rx_get_frame(rx_);
            if (!frame) {
                continue;
            }

            handle_frame(*frame);

            st20p_rx_put_frame(rx_, frame);
        }
    }

    void handle_frame(const st_frame &mtl_frame) {
        IFrameSink *sink = nullptr;
        PixelFormat project_format{};
        st_frame_fmt expected_mtl_format{};

        {
            std::lock_guard lock(mutex_);

            sink = sink_;
            project_format = expected_project_format_;
            expected_mtl_format = expected_mtl_output_format_;
        }

        if (!st_is_frame_complete(mtl_frame.status)) {
            return;
        }

        if (mtl_frame.width != cfg_.width || mtl_frame.height != cfg_.height) {
            return;
        }

        if (mtl_frame.fmt != expected_mtl_format) {
            return;
        }

        auto copied_frame = copy_frame(mtl_frame, project_format);
        if (!copied_frame.has_value()) {
            return;
        }

        sink->on_video_frame(std::move(*copied_frame), FrameTimingMetadata{
                                                           .rtp_timestamp = mtl_frame.rtp_timestamp,
                                                           .receive_timestamp_ns = mtl_receive_timestamp_ns(mtl_frame),
                                                       });
    }

    [[nodiscard]] std::expected<VideoFrame, Error> copy_frame(const st_frame &mtl_frame,
                                                              const PixelFormat project_format) const {
        VideoFrame out(mtl_frame.width, mtl_frame.height, project_format);

        for (std::size_t plane = 0; plane < out.plane_count(); ++plane) {
            if (!mtl_frame.addr[plane]) {
                return std::unexpected(Error::InvalidValue);
            }

            const std::size_t active_row_bytes = out.active_row_bytes(plane);
            const std::size_t rows = out.plane_height_rows(plane);

            if (mtl_frame.linesize[plane] < active_row_bytes) {
                return std::unexpected(Error::BufferTooSmall);
            }

            const auto *src = static_cast<const std::uint8_t *>(mtl_frame.addr[plane]);

            for (std::size_t row = 0; row < rows; ++row) {
                std::memcpy(out.row_data(static_cast<std::uint32_t>(row), plane), src + row * mtl_frame.linesize[plane],
                            active_row_bytes);
            }
        }

        return out;
    }

    MtlVideoStartConfig cfg_{};

    mutable std::mutex mutex_{};

    IFrameSink *sink_ = nullptr;

    mtl_handle mt_ = nullptr;
    st20p_rx_handle rx_ = nullptr;

    std::jthread receive_thread_{};
    std::atomic_bool stop_requested_{true};

    st_frame_fmt expected_mtl_output_format_{};
    PixelFormat expected_project_format_ = PixelFormat::UYVY;
};

MtlRxVideoBackend::MtlRxVideoBackend(MtlVideoStartConfig cfg) : impl_(std::make_unique<Impl>(std::move(cfg))) {}

MtlRxVideoBackend::~MtlRxVideoBackend() = default;

RxBackendLifecycleResult MtlRxVideoBackend::start(IFrameSink *sink) { return impl_->start(sink); }

RxBackendLifecycleResult MtlRxVideoBackend::stop() { return impl_->stop(); }

} // namespace st2110
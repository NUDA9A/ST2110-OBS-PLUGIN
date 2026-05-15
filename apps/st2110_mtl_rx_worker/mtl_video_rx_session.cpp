#include "mtl_video_rx_session.hpp"
#include "mtl_worker_event_writer.hpp"

#include <mtl/st20_api.h>
#include <mtl/st_pipeline_api.h>

#include <cstdio>
#include <cstring>
#include <stop_token>
#include <thread>
#include <utility>

namespace st2110_mtl_rx_worker {
namespace {

[[nodiscard]] std::expected<st_fps, st2110::Error>
map_mtl_video_frame_rate(const st2110::MtlVideoFrameRate fps) noexcept {
    switch (fps) {
    case st2110::MtlVideoFrameRate::P23_98:
        return ST_FPS_P23_98;
    case st2110::MtlVideoFrameRate::P24:
        return ST_FPS_P24;
    case st2110::MtlVideoFrameRate::P25:
        return ST_FPS_P25;
    case st2110::MtlVideoFrameRate::P29_97:
        return ST_FPS_P29_97;
    case st2110::MtlVideoFrameRate::P30:
        return ST_FPS_P30;
    case st2110::MtlVideoFrameRate::P50:
        return ST_FPS_P50;
    case st2110::MtlVideoFrameRate::P59_94:
        return ST_FPS_P59_94;
    case st2110::MtlVideoFrameRate::P60:
        return ST_FPS_P60;
    case st2110::MtlVideoFrameRate::P100:
        return ST_FPS_P100;
    case st2110::MtlVideoFrameRate::P119_88:
        return ST_FPS_P119_88;
    case st2110::MtlVideoFrameRate::P120:
        return ST_FPS_P120;
    }

    return std::unexpected(st2110::Error::Unsupported);
}

[[nodiscard]] std::expected<st20_fmt, st2110::Error>
map_mtl_video_transport_format(const st2110::MtlVideoTransportFormat fmt) noexcept {
    switch (fmt) {
    case st2110::MtlVideoTransportFormat::Yuv422_8Bit:
        return ST20_FMT_YUV_422_8BIT;
    case st2110::MtlVideoTransportFormat::Yuv422_10Bit:
        return ST20_FMT_YUV_422_10BIT;
    case st2110::MtlVideoTransportFormat::Yuv422_12Bit:
        return ST20_FMT_YUV_422_12BIT;
    case st2110::MtlVideoTransportFormat::Yuv422_16Bit:
        return ST20_FMT_YUV_422_16BIT;
    case st2110::MtlVideoTransportFormat::Yuv420_8Bit:
        return ST20_FMT_YUV_420_8BIT;
    case st2110::MtlVideoTransportFormat::Yuv420_10Bit:
        return ST20_FMT_YUV_420_10BIT;
    case st2110::MtlVideoTransportFormat::Yuv420_12Bit:
        return ST20_FMT_YUV_420_12BIT;
    case st2110::MtlVideoTransportFormat::Yuv420_16Bit:
        return ST20_FMT_YUV_420_16BIT;
    case st2110::MtlVideoTransportFormat::Yuv444_8Bit:
        return ST20_FMT_YUV_444_8BIT;
    case st2110::MtlVideoTransportFormat::Yuv444_10Bit:
        return ST20_FMT_YUV_444_10BIT;
    case st2110::MtlVideoTransportFormat::Yuv444_12Bit:
        return ST20_FMT_YUV_444_12BIT;
    case st2110::MtlVideoTransportFormat::Yuv444_16Bit:
        return ST20_FMT_YUV_444_16BIT;
    case st2110::MtlVideoTransportFormat::Rgb8:
        return ST20_FMT_RGB_8BIT;
    case st2110::MtlVideoTransportFormat::Rgb10:
        return ST20_FMT_RGB_10BIT;
    case st2110::MtlVideoTransportFormat::Rgb12:
        return ST20_FMT_RGB_12BIT;
    case st2110::MtlVideoTransportFormat::Rgb16:
        return ST20_FMT_RGB_16BIT;
    }

    return std::unexpected(st2110::Error::Unsupported);
}

[[nodiscard]] std::expected<st_frame_fmt, st2110::Error>
map_mtl_video_output_format(const st2110::PixelFormat fmt) noexcept {
    switch (fmt) {
    case st2110::PixelFormat::UYVY:
        return ST_FRAME_FMT_UYVY;
    case st2110::PixelFormat::RGB8:
        return ST_FRAME_FMT_RGB8;

    case st2110::PixelFormat::YUV422RFC4175PG2BE10:
        return ST_FRAME_FMT_YUV422RFC4175PG2BE10;
    case st2110::PixelFormat::YUV422RFC4175PG2BE12:
        return ST_FRAME_FMT_YUV422RFC4175PG2BE12;
    case st2110::PixelFormat::YUV444RFC4175PG4BE10:
        return ST_FRAME_FMT_YUV444RFC4175PG4BE10;
    case st2110::PixelFormat::YUV444RFC4175PG2BE12:
        return ST_FRAME_FMT_YUV444RFC4175PG2BE12;
    case st2110::PixelFormat::RGBRFC4175PG4BE10:
        return ST_FRAME_FMT_RGBRFC4175PG4BE10;
    case st2110::PixelFormat::RGBRFC4175PG2BE12:
        return ST_FRAME_FMT_RGBRFC4175PG2BE12;

    case st2110::PixelFormat::YUV422PLANAR8:
        return ST_FRAME_FMT_YUV422PLANAR8;
    case st2110::PixelFormat::YUV422PLANAR10LE:
        return ST_FRAME_FMT_YUV422PLANAR10LE;
    case st2110::PixelFormat::YUV422PLANAR12LE:
        return ST_FRAME_FMT_YUV422PLANAR12LE;
    case st2110::PixelFormat::YUV422PLANAR16LE:
        return ST_FRAME_FMT_YUV422PLANAR16LE;
    case st2110::PixelFormat::YUV444PLANAR10LE:
        return ST_FRAME_FMT_YUV444PLANAR10LE;
    case st2110::PixelFormat::YUV444PLANAR12LE:
        return ST_FRAME_FMT_YUV444PLANAR12LE;
    case st2110::PixelFormat::YUV420PLANAR8:
        return ST_FRAME_FMT_YUV420PLANAR8;

    case st2110::PixelFormat::BGRA:
        return ST_FRAME_FMT_BGRA;
    case st2110::PixelFormat::ARGB:
        return ST_FRAME_FMT_ARGB;
    case st2110::PixelFormat::V210:
        return ST_FRAME_FMT_V210;
    case st2110::PixelFormat::Y210:
        return ST_FRAME_FMT_Y210;
    }

    return std::unexpected(st2110::Error::Unsupported);
}

[[nodiscard]] bool mtl_interlaced_flag(const st2110::VideoScanMode scan_mode) noexcept {
    switch (scan_mode) {
    case st2110::VideoScanMode::Progressive:
        return false;
    case st2110::VideoScanMode::Interlaced:
    case st2110::VideoScanMode::PsF:
        return true;
    }

    return false;
}

[[nodiscard]] std::expected<bool, st2110::Error>
validate_video_media_ring(const st2110::MtlWorkerSharedMemoryRingMap *ring) noexcept {
    if (!ring) {
        return true;
    }

    if (!ring->mapped()) {
        return std::unexpected(st2110::Error::InvalidBackendState);
    }

    if (ring->descriptor().media_kind != st2110::MtlWorkerMediaKind::Video) {
        return std::unexpected(st2110::Error::InvalidValue);
    }

    auto valid_headers = ring->validate_initialized_slot_headers();
    if (!valid_headers.has_value()) {
        return std::unexpected(valid_headers.error());
    }

    return true;
}

void fill_st20p_session_port(st20p_rx_ops &ops, const mtl_session_port session_port,
                             const st2110::MtlRuntimePortConfig &runtime_port,
                             const st2110::MtlVideoSessionPortConfig &session_port_cfg) {
    std::memcpy(ops.port.ip_addr[session_port], session_port_cfg.ip_addr.data(), session_port_cfg.ip_addr.size());

    if (session_port_cfg.source_ip.has_value()) {
        std::memcpy(ops.port.mcast_sip_addr[session_port], session_port_cfg.source_ip->data(),
                    session_port_cfg.source_ip->size());
    }

    std::snprintf(ops.port.port[session_port], MTL_PORT_MAX_LEN, "%s", runtime_port.port_name.c_str());

    ops.port.udp_port[session_port] = session_port_cfg.udp_port;
}

[[nodiscard]] std::expected<st20p_rx_ops, st2110::Error>
make_st20p_rx_ops(const st2110::MtlVideoStartConfig &cfg) noexcept {
    if (cfg.redundant.has_value() && !cfg.runtime.redundant_port.has_value()) {
        return std::unexpected(st2110::Error::InvalidValue);
    }

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
    ops.name = "st2110_mtl_worker_video_rx";
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

    /*
     * Required for the future worker receive thread:
     * st20p_rx_get_frame() blocks until a frame is ready, and stop wakes it.
     */
    ops.flags = ST20P_RX_FLAG_BLOCK_GET;

    return ops;
}

} // namespace

struct MtlVideoRxSession::Impl {
    st2110::MtlVideoStartConfig cfg{};
    st20p_rx_handle rx = nullptr;

    MtlWorkerGraphStats *stats = nullptr;
    MtlWorkerEventWriter *event_writer = nullptr;
    st2110::MtlWorkerSharedMemoryRingMap *media_ring = nullptr;

    std::jthread receive_thread{};

    explicit Impl(st2110::MtlVideoStartConfig session_cfg, st20p_rx_handle session_handle,
              MtlWorkerGraphStats &graph_stats, MtlWorkerEventWriter &writer,
              st2110::MtlWorkerSharedMemoryRingMap *bound_media_ring)
    : cfg(std::move(session_cfg)),
      rx(session_handle),
      stats(&graph_stats),
      event_writer(&writer),
      media_ring(bound_media_ring) {}

    ~Impl() {
        stop_thread_noexcept();

        if (rx) {
            st20p_rx_free(rx);
            rx = nullptr;
        }
    }

    [[nodiscard]] std::expected<bool, st2110::Error> start_thread() {
        if (!rx) {
            return std::unexpected(st2110::Error::InvalidBackendState);
        }

        try {
            receive_thread = std::jthread([this](std::stop_token stop_token) { receive_loop_noexcept(stop_token); });
        } catch (...) {
            st20p_rx_wake_block(rx);
            return std::unexpected(st2110::Error::SystemFailure);
        }

        return true;
    }

    void stop_thread_noexcept() noexcept {
        if (!receive_thread.joinable()) {
            return;
        }

        receive_thread.request_stop();

        if (rx) {
            st20p_rx_wake_block(rx);
        }

        try {
            receive_thread.join();
        } catch (...) {
            /*
             * Destructor path must stay noexcept.
             * In normal execution join() should not throw after joinable() check.
             */
        }
    }

    void receive_loop_noexcept(std::stop_token stop_token) noexcept {
        while (!stop_token.stop_requested()) {
            st_frame *frame = st20p_rx_get_frame(rx);
            if (!frame) {
                continue;
            }

            if (stats) {
                stats->record_video_frame_received();
            }

            /*
             * Future media data-plane boundary:
             *
             * - map MTL st_frame metadata;
             * - copy/export payload into shared-memory video ring slot;
             * - send MtlWorkerFrameReadyEvent through control/event IPC;
             * - wait for/recover slot ownership according to the future shared
             *   memory protocol.
             *
             * For now, the worker only validates the blocking get/put lifetime
             * and returns the frame immediately to MTL.
             */
            st20p_rx_put_frame(rx, frame);
        }
    }
};

std::expected<std::unique_ptr<MtlVideoRxSession>, st2110::Error>
MtlVideoRxSession::create(MtlRuntimeContext &runtime, st2110::MtlVideoStartConfig cfg, MtlWorkerGraphStats &stats,
                          MtlWorkerEventWriter &event_writer,
                          st2110::MtlWorkerSharedMemoryRingMap *media_ring) {
    if (!runtime.handle()) {
        return std::unexpected(st2110::Error::InvalidBackendState);
    }

    auto valid_ring = validate_video_media_ring(media_ring);
    if (!valid_ring.has_value()) {
        return std::unexpected(valid_ring.error());
    }

    auto ops = make_st20p_rx_ops(cfg);
    if (!ops.has_value()) {
        return std::unexpected(ops.error());
    }

    st20p_rx_handle rx = st20p_rx_create(runtime.handle(), &*ops);
    if (!rx) {
        return std::unexpected(st2110::Error::SystemFailure);
    }

    auto impl = std::make_unique<Impl>(std::move(cfg), rx, stats, event_writer, media_ring);
    auto started = impl->start_thread();
    if (!started.has_value()) {
        return std::unexpected(started.error());
    }

    return std::unique_ptr<MtlVideoRxSession>(new MtlVideoRxSession(std::move(impl)));
}

MtlVideoRxSession::MtlVideoRxSession(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

MtlVideoRxSession::~MtlVideoRxSession() = default;

void MtlVideoRxSession::wake_block() noexcept {
    if (impl_->rx) {
        st20p_rx_wake_block(impl_->rx);
    }
}

const st2110::MtlVideoStartConfig &MtlVideoRxSession::config() const noexcept { return impl_->cfg; }

const st2110::MtlWorkerSharedMemoryRingMap *MtlVideoRxSession::media_ring() const noexcept {
    return impl_->media_ring;
}

} // namespace st2110_mtl_rx_worker
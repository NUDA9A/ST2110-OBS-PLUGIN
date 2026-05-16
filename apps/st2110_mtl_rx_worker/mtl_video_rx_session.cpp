#include "mtl_video_rx_session.hpp"
#include "mtl_worker_event_writer.hpp"
#include "mtl_worker_health.hpp"

#include <mtl/st20_api.h>
#include <mtl/st_pipeline_api.h>
#include <st2110/delivery/video/video_frame.hpp>

#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <span>
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

[[nodiscard]] std::expected<std::uint32_t, st2110::Error> make_video_field_flags(const st2110::MtlVideoStartConfig &cfg,
                                                                                 const st_frame &frame) noexcept {
    const bool expected_interlaced = cfg.scan_mode != st2110::VideoScanMode::Progressive;

    if (frame.interlaced != expected_interlaced) {
        return std::unexpected(st2110::Error::InvalidValue);
    }

    if (!frame.interlaced && frame.second_field) {
        return std::unexpected(st2110::Error::InvalidValue);
    }

    std::uint32_t flags = 0;

    if (frame.interlaced) {
        flags |= static_cast<std::uint32_t>(st2110::MtlWorkerVideoFieldFlags::Interlaced);
    }

    if (frame.second_field) {
        flags |= static_cast<std::uint32_t>(st2110::MtlWorkerVideoFieldFlags::SecondField);
    }

    return flags;
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

[[nodiscard]] std::expected<bool, st2110::Error> acquire_video_ring_slot(st2110::MtlWorkerSharedMemoryRingMap &ring,
                                                                         std::uint32_t &next_slot_index,
                                                                         std::uint32_t &out_slot_index) noexcept {
    const auto &descriptor = ring.descriptor();
    if (descriptor.slot_count == 0) {
        return std::unexpected(st2110::Error::InvalidBackendState);
    }

    for (std::uint32_t attempt = 0; attempt < descriptor.slot_count; ++attempt) {
        const std::uint32_t candidate =
            static_cast<std::uint32_t>((static_cast<std::uint64_t>(next_slot_index) + attempt) % descriptor.slot_count);

        auto began = ring.begin_write_slot(candidate);
        if (!began.has_value()) {
            return std::unexpected(began.error());
        }

        if (*began) {
            out_slot_index = candidate;
            next_slot_index =
                static_cast<std::uint32_t>((static_cast<std::uint64_t>(candidate) + 1) % descriptor.slot_count);
            return true;
        }
    }

    return false;
}

struct VideoPayloadCopyResult {
    std::uint64_t payload_size = 0;
    st2110::MtlWorkerSharedMemorySlotMediaMetadata metadata{};
};

[[nodiscard]] bool add_overflows_u64(const std::uint64_t a, const std::uint64_t b, std::uint64_t &out) noexcept {
    if (std::numeric_limits<std::uint64_t>::max() - a < b) {
        return true;
    }

    out = a + b;
    return false;
}

[[nodiscard]] bool mul_overflows_u64(const std::uint64_t a, const std::uint64_t b, std::uint64_t &out) noexcept {
    if (a != 0 && b > std::numeric_limits<std::uint64_t>::max() / a) {
        return true;
    }

    out = a * b;
    return false;
}

[[nodiscard]] std::expected<VideoPayloadCopyResult, st2110::Error>
copy_video_frame_planes_to_payload(std::span<std::byte> payload, const st2110::MtlVideoStartConfig &cfg,
                                   st_frame *frame) {
    if (!frame) {
        return std::unexpected(st2110::Error::InvalidValue);
    }

    if (frame->width != cfg.width || frame->height != cfg.height) {
        return std::unexpected(st2110::Error::InvalidValue);
    }

    auto expected_output_format = map_mtl_video_output_format(cfg.output_format);
    if (!expected_output_format.has_value()) {
        return std::unexpected(expected_output_format.error());
    }

    if (frame->fmt != *expected_output_format) {
        return std::unexpected(st2110::Error::InvalidValue);
    }

    auto video_field_flags = make_video_field_flags(cfg, *frame);
    if (!video_field_flags.has_value()) {
        return std::unexpected(video_field_flags.error());
    }

    st2110::VideoFrame layout{frame->width, frame->height, cfg.output_format};

    if (layout.plane_count() == 0 || layout.plane_count() > st2110::mtlWorkerSharedMemoryMaxPlanes) {
        return std::unexpected(st2110::Error::InvalidValue);
    }

    st2110::MtlWorkerSharedMemorySlotMediaMetadata metadata{
        .media_kind = st2110::MtlWorkerMediaKind::Video,
        .media_format = static_cast<std::uint32_t>(cfg.output_format),
        .width = frame->width,
        .height = frame->height,
        .sample_rate_hz = 0,
        .channels = 0,
        .samples_per_channel = 0,
        .rtp_timestamp = frame->rtp_timestamp,
        .receive_timestamp_ns = static_cast<st2110::TimestampNs>(frame->receive_timestamp),
        .plane_count = static_cast<std::uint32_t>(layout.plane_count()),
        .reserved0 = 0,
        .video_scan_mode = static_cast<std::uint32_t>(cfg.scan_mode),
        .video_field_flags = *video_field_flags,
        .plane_offset_bytes = {0, 0, 0, 0},
        .plane_size_bytes = {0, 0, 0, 0},
        .plane_line_size_bytes = {0, 0, 0, 0},
    };

    std::uint64_t payload_offset = 0;

    for (std::size_t plane = 0; plane < layout.plane_count(); ++plane) {
        if (!frame->addr[plane]) {
            return std::unexpected(st2110::Error::InvalidValue);
        }

        const auto active_row_bytes = static_cast<std::uint64_t>(layout.active_row_bytes(plane));
        const auto height_rows = static_cast<std::uint64_t>(layout.plane_height_rows(plane));
        const auto source_stride = static_cast<std::uint64_t>(frame->linesize[plane]);

        if (active_row_bytes == 0 || height_rows == 0 || source_stride < active_row_bytes) {
            return std::unexpected(st2110::Error::InvalidValue);
        }

        std::uint64_t plane_size = 0;
        if (mul_overflows_u64(active_row_bytes, height_rows, plane_size)) {
            return std::unexpected(st2110::Error::InvalidValue);
        }

        std::uint64_t next_payload_offset = 0;
        if (add_overflows_u64(payload_offset, plane_size, next_payload_offset) ||
            next_payload_offset > static_cast<std::uint64_t>(payload.size())) {
            return std::unexpected(st2110::Error::InvalidValue);
        }

        const auto *src = static_cast<const std::byte *>(frame->addr[plane]);
        std::byte *dst = payload.data() + static_cast<std::size_t>(payload_offset);

        for (std::uint64_t row = 0; row < height_rows; ++row) {
            std::memcpy(dst + static_cast<std::size_t>(row * active_row_bytes),
                        src + static_cast<std::size_t>(row * source_stride),
                        static_cast<std::size_t>(active_row_bytes));
        }

        metadata.plane_offset_bytes[plane] = payload_offset;
        metadata.plane_size_bytes[plane] = plane_size;

        /*
         * Shared-memory payload is tightly packed per plane. This is the slot
         * payload line size, not necessarily MTL's source linesize.
         */
        metadata.plane_line_size_bytes[plane] = active_row_bytes;

        payload_offset = next_payload_offset;
    }

    return VideoPayloadCopyResult{
        .payload_size = payload_offset,
        .metadata = metadata,
    };
}

[[nodiscard]] std::expected<bool, st2110::Error>
export_video_frame_to_ring(st2110::MtlWorkerSharedMemoryRingMap *ring, MtlWorkerEventWriter *event_writer,
                           const st2110::MtlWorkerGraphId graph_id, const st2110::MtlVideoStartConfig &cfg,
                           st_frame *frame, std::uint32_t &next_slot_index, std::uint64_t &next_sequence) noexcept {
    if (!ring || !event_writer || !frame) {
        return false;
    }

    if (!ring->mapped()) {
        return false;
    }

    std::uint32_t slot_index = 0;
    auto acquired = acquire_video_ring_slot(*ring, next_slot_index, slot_index);
    if (!acquired.has_value()) {
        return std::unexpected(acquired.error());
    }

    if (!*acquired) {
        return false;
    }

    auto payload = ring->slot_payload(slot_index);
    if (!payload.has_value()) {
        (void)ring->abort_write_slot(slot_index);
        return std::unexpected(payload.error());
    }

    std::expected<VideoPayloadCopyResult, st2110::Error> copied;

    try {
        copied = copy_video_frame_planes_to_payload(*payload, cfg, frame);
    } catch (...) {
        (void)ring->abort_write_slot(slot_index);
        return std::unexpected(st2110::Error::InvalidValue);
    }
    if (!copied.has_value()) {
        (void)ring->abort_write_slot(slot_index);
        return std::unexpected(copied.error());
    }

    if (copied->payload_size == 0 || copied->payload_size > ring->descriptor().slot_payload_capacity_bytes) {
        (void)ring->abort_write_slot(slot_index);
        return false;
    }

    const std::uint64_t sequence = next_sequence++;
    auto published = ring->publish_written_slot(
        slot_index, copied->payload_size, sequence,
        static_cast<std::uint32_t>(st2110::MtlWorkerSharedMemorySlotFlags::None), copied->metadata);
    if (!published.has_value()) {
        (void)ring->abort_write_slot(slot_index);
        return std::unexpected(published.error());
    }

    st2110::MtlWorkerFrameReadyEvent event{
        .graph_id = graph_id,
        .ring_id = ring->descriptor().ring_id,
        .slot_id = slot_index,
        .sequence = sequence,
    };

    auto wrote = event_writer->write_event(st2110::MtlWorkerControlEvent{event});
    if (!wrote.has_value()) {
        return std::unexpected(wrote.error());
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

int on_st20p_rx_event(void *priv, const enum st_event event, void *args) {
    (void)args;

    auto *health = static_cast<MtlWorkerHealthState *>(priv);
    if (!health) {
        return 0;
    }

    if (event == ST_EVENT_FATAL_ERROR) {
        health->mark_unhealthy(st2110::Error::InvalidBackendState, "MTL ST20P RX session reported fatal event");
    }

    return 0;
}

[[nodiscard]] std::expected<st20p_rx_ops, st2110::Error> make_st20p_rx_ops(const st2110::MtlVideoStartConfig &cfg,
                                                                           MtlWorkerHealthState *health) noexcept {
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
    ops.priv = health;
    ops.notify_event = on_st20p_rx_event;

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

void copy_rx_port_stats(st2110::MtlWorkerRxPortStats &dst, const st_rx_port_stats &src) noexcept {
    dst.packets = src.packets;
    dst.bytes = src.bytes;
    dst.frames = src.frames;
    dst.incomplete_frames = src.incomplete_frames;
    dst.err_packets = src.err_packets;
    dst.out_of_order_packets = src.out_of_order_packets;
}

void record_video_frame_mtl_metadata(MtlWorkerGraphStats &stats, const st_frame &frame) noexcept {
    stats.record_video_frame_packet_metadata(
        frame.pkts_total, frame.pkts_recv[MTL_SESSION_PORT_P], frame.pkts_recv[MTL_SESSION_PORT_R],
        frame.status == ST_FRAME_STATUS_RECONSTRUCTED, frame.status == ST_FRAME_STATUS_CORRUPTED);
}

} // namespace

struct MtlVideoRxSession::Impl {
    st2110::MtlVideoStartConfig cfg{};
    st20p_rx_handle rx = nullptr;

    MtlWorkerGraphStats *stats = nullptr;
    MtlWorkerEventWriter *event_writer = nullptr;
    st2110::MtlWorkerSharedMemoryRingMap *media_ring = nullptr;
    st2110::MtlWorkerGraphId graph_id = 0;
    std::uint32_t next_slot_index = 0;
    std::uint64_t next_sequence = 1;
    std::shared_ptr<MtlWorkerHealthState> health{};
    std::atomic_bool receive_loop_active{false};

    std::jthread receive_thread{};

    explicit Impl(st2110::MtlWorkerGraphId graph, st2110::MtlVideoStartConfig session_cfg,
                  st20p_rx_handle session_handle, MtlWorkerGraphStats &graph_stats, MtlWorkerEventWriter &writer,
                  st2110::MtlWorkerSharedMemoryRingMap *bound_media_ring,
                  std::shared_ptr<MtlWorkerHealthState> session_health)
        : cfg(std::move(session_cfg)), graph_id(graph), rx(session_handle), stats(&graph_stats), event_writer(&writer),
          media_ring(bound_media_ring), health(std::move(session_health)) {}

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

        receive_loop_active.store(true, std::memory_order_release);

        try {
            receive_thread = std::jthread([this](std::stop_token stop_token) { receive_loop_noexcept(stop_token); });
        } catch (...) {
            receive_loop_active.store(false, std::memory_order_release);

            if (health) {
                health->mark_unhealthy(st2110::Error::SystemFailure, "Failed to start MTL video receive thread");
            }

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
        try {
            while (!stop_token.stop_requested()) {
                st_frame *frame = st20p_rx_get_frame(rx);
                if (!frame) {
                    continue;
                }

                if (stats) {
                    stats->record_video_frame_received();
                    record_video_frame_mtl_metadata(*stats, *frame);
                }

                auto exported = export_video_frame_to_ring(media_ring, event_writer, graph_id, cfg, frame,
                                                           next_slot_index, next_sequence);
                if (!exported.has_value()) {
                    if (stats) {
                        stats->record_video_frame_dropped();
                    }

                    if (health) {
                        health->mark_unhealthy(exported.error(), "MTL video shared-memory/IPC export failed");
                    }

                    (void)st20p_rx_put_frame(rx, frame);
                    break;
                }

                if (!*exported && stats) {
                    stats->record_video_frame_dropped();
                }

                if (st20p_rx_put_frame(rx, frame) < 0) {
                    if (health) {
                        health->mark_unhealthy(st2110::Error::SystemFailure, "MTL video st20p_rx_put_frame failed");
                    }

                    break;
                }
            }
        } catch (...) {
            if (health) {
                health->mark_unhealthy(st2110::Error::SystemFailure, "MTL video receive loop threw");
            }
        }

        receive_loop_active.store(false, std::memory_order_release);
    }
};

std::expected<std::unique_ptr<MtlVideoRxSession>, st2110::Error>
MtlVideoRxSession::create(MtlRuntimeContext &runtime, st2110::MtlWorkerGraphId graph_id,
                          st2110::MtlVideoStartConfig cfg, MtlWorkerGraphStats &stats,
                          MtlWorkerEventWriter &event_writer, st2110::MtlWorkerSharedMemoryRingMap *media_ring) {
    if (!runtime.handle()) {
        return std::unexpected(st2110::Error::InvalidBackendState);
    }

    auto valid_ring = validate_video_media_ring(media_ring);
    if (!valid_ring.has_value()) {
        return std::unexpected(valid_ring.error());
    }

    auto health = std::make_shared<MtlWorkerHealthState>();

    auto ops = make_st20p_rx_ops(cfg, health.get());
    if (!ops.has_value()) {
        return std::unexpected(ops.error());
    }

    st20p_rx_handle rx = st20p_rx_create(runtime.handle(), &*ops);
    if (!rx) {
        return std::unexpected(st2110::Error::SystemFailure);
    }

    auto impl =
        std::make_unique<Impl>(graph_id, std::move(cfg), rx, stats, event_writer, media_ring, std::move(health));
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

const st2110::MtlWorkerSharedMemoryRingMap *MtlVideoRxSession::media_ring() const noexcept { return impl_->media_ring; }

void MtlVideoRxSession::append_stats_snapshot(MtlWorkerGraphStatsSnapshot &snapshot) const noexcept {
    if (!impl_->rx) {
        return;
    }

    st20_rx_user_stats session_stats{};
    if (st20p_rx_get_session_stats(impl_->rx, &session_stats) < 0) {
        ++snapshot.video_session_stats_query_failures;
        return;
    }

    snapshot.video_session_stats_available = true;

    copy_rx_port_stats(snapshot.video_session_primary, session_stats.common.port[MTL_SESSION_PORT_P]);

    if (impl_->cfg.redundant.has_value()) {
        copy_rx_port_stats(snapshot.video_session_redundant, session_stats.common.port[MTL_SESSION_PORT_R]);
    }

    snapshot.video_session_packets_received = session_stats.common.stat_pkts_received;
    snapshot.video_session_packets_out_of_order = session_stats.common.stat_pkts_out_of_order;
    snapshot.video_session_packets_wrong_ssrc_dropped = session_stats.common.stat_pkts_wrong_ssrc_dropped;
    snapshot.video_session_packets_wrong_payload_type_dropped = session_stats.common.stat_pkts_wrong_pt_dropped;

    snapshot.video_session_bytes_received = session_stats.stat_bytes_received;
    snapshot.video_session_frames_dropped = session_stats.stat_frames_dropped;
    snapshot.video_session_frames_packets_missed = session_stats.stat_frames_pks_missed;
    snapshot.video_session_packets_wrong_length_dropped = session_stats.stat_pkts_wrong_len_dropped;
    snapshot.video_session_slot_get_frame_failures = session_stats.stat_slot_get_frame_fail;
}

bool MtlVideoRxSession::healthy() const noexcept {
    return impl_ && impl_->rx && impl_->health && impl_->health->healthy() &&
           impl_->receive_loop_active.load(std::memory_order_acquire);
}

st2110::Error MtlVideoRxSession::health_error() const noexcept {
    if (!impl_ || !impl_->rx) {
        return st2110::Error::InvalidBackendState;
    }

    if (!impl_->health) {
        return st2110::Error::InvalidBackendState;
    }

    if (!impl_->receive_loop_active.load(std::memory_order_acquire)) {
        return st2110::Error::InvalidBackendState;
    }

    return impl_->health->error();
}

std::string MtlVideoRxSession::health_message() const {
    if (!impl_ || !impl_->rx) {
        return "MTL video RX session has no native handle";
    }

    if (!impl_->health) {
        return "MTL video RX session has no health state";
    }

    if (!impl_->receive_loop_active.load(std::memory_order_acquire)) {
        return "MTL video receive loop is not active";
    }

    return impl_->health->message();
}

} // namespace st2110_mtl_rx_worker
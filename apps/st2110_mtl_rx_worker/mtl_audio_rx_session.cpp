#include "mtl_audio_rx_session.hpp"
#include "mtl_worker_event_writer.hpp"

#include <mtl/st30_api.h>
#include <mtl/st30_pipeline_api.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <stop_token>
#include <thread>
#include <utility>

namespace st2110_mtl_rx_worker {
namespace {

[[nodiscard]] std::expected<st30_fmt, st2110::Error>
map_mtl_audio_pcm_format(const st2110::MtlAudioPcmFormat fmt) noexcept {
    switch (fmt) {
    case st2110::MtlAudioPcmFormat::Pcm16:
        return ST30_FMT_PCM16;
    case st2110::MtlAudioPcmFormat::Pcm24:
        return ST30_FMT_PCM24;
    }

    return std::unexpected(st2110::Error::Unsupported);
}

[[nodiscard]] std::expected<st30_sampling, st2110::Error>
map_mtl_audio_sampling(const st2110::MtlAudioSampling sampling) noexcept {
    switch (sampling) {
    case st2110::MtlAudioSampling::K48:
        return ST30_SAMPLING_48K;
    }

    return std::unexpected(st2110::Error::Unsupported);
}

[[nodiscard]] std::expected<st30_ptime, st2110::Error>
map_mtl_audio_packet_time(const st2110::MtlAudioPacketTime packet_time) noexcept {
    switch (packet_time) {
    case st2110::MtlAudioPacketTime::Ptime1ms:
        return ST30_PTIME_1MS;
    }

    return std::unexpected(st2110::Error::Unsupported);
}

[[nodiscard]] std::expected<bool, st2110::Error>
validate_audio_media_ring(const st2110::MtlWorkerSharedMemoryRingMap *ring) noexcept {
    if (!ring) {
        return true;
    }

    if (!ring->mapped()) {
        return std::unexpected(st2110::Error::InvalidBackendState);
    }

    if (ring->descriptor().media_kind != st2110::MtlWorkerMediaKind::Audio) {
        return std::unexpected(st2110::Error::InvalidValue);
    }

    auto valid_headers = ring->validate_initialized_slot_headers();
    if (!valid_headers.has_value()) {
        return std::unexpected(valid_headers.error());
    }

    return true;
}

[[nodiscard]] std::expected<std::uint64_t, st2110::Error>
audio_bytes_per_sample(const st2110::MtlAudioPcmFormat format) noexcept {
    switch (format) {
    case st2110::MtlAudioPcmFormat::Pcm16:
        return 2;
    case st2110::MtlAudioPcmFormat::Pcm24:
        return 3;
    }

    return std::unexpected(st2110::Error::Unsupported);
}

[[nodiscard]] std::expected<bool, st2110::Error>
acquire_audio_ring_slot(st2110::MtlWorkerSharedMemoryRingMap &ring, std::uint32_t &next_slot_index,
                        std::uint32_t &out_slot_index) noexcept {
    const auto &descriptor = ring.descriptor();
    if (descriptor.slot_count == 0) {
        return std::unexpected(st2110::Error::InvalidBackendState);
    }

    for (std::uint32_t attempt = 0; attempt < descriptor.slot_count; ++attempt) {
        const std::uint32_t candidate = static_cast<std::uint32_t>(
            (static_cast<std::uint64_t>(next_slot_index) + attempt) % descriptor.slot_count);

        auto began = ring.begin_write_slot(candidate);
        if (!began.has_value()) {
            return std::unexpected(began.error());
        }

        if (*began) {
            out_slot_index = candidate;
            next_slot_index = static_cast<std::uint32_t>((static_cast<std::uint64_t>(candidate) + 1) %
                                                         descriptor.slot_count);
            return true;
        }
    }

    return false;
}

[[nodiscard]] std::expected<bool, st2110::Error>
export_audio_frame_to_ring(st2110::MtlWorkerSharedMemoryRingMap *ring, MtlWorkerEventWriter *event_writer,
                           const st2110::MtlWorkerGraphId graph_id, const st2110::MtlAudioStartConfig &cfg,
                           st30_frame *frame, std::uint32_t &next_slot_index,
                           std::uint64_t &next_sequence) noexcept {
    if (!ring || !event_writer || !frame) {
        return false;
    }

    if (!ring->mapped() || !frame->addr || frame->data_size == 0) {
        return false;
    }

    const std::uint64_t payload_size = static_cast<std::uint64_t>(frame->data_size);
    if (payload_size > ring->descriptor().slot_payload_capacity_bytes) {
        return false;
    }

    std::uint32_t slot_index = 0;
    auto acquired = acquire_audio_ring_slot(*ring, next_slot_index, slot_index);
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

    if (payload->size() < frame->data_size) {
        (void)ring->abort_write_slot(slot_index);
        return false;
    }

    std::memcpy(payload->data(), frame->addr, frame->data_size);

    const std::uint64_t sequence = next_sequence++;
    auto published = ring->publish_written_slot(
        slot_index, payload_size, sequence,
        static_cast<std::uint32_t>(st2110::MtlWorkerSharedMemorySlotFlags::None));
    if (!published.has_value()) {
        (void)ring->abort_write_slot(slot_index);
        return std::unexpected(published.error());
    }

    auto bytes_per_sample = audio_bytes_per_sample(cfg.pcm_format);
    if (!bytes_per_sample.has_value()) {
        return std::unexpected(bytes_per_sample.error());
    }

    const std::uint32_t channels = frame->channel != 0 ? frame->channel : cfg.media.channel_count;
    const std::uint64_t bytes_per_frame = static_cast<std::uint64_t>(channels) * *bytes_per_sample;
    const std::uint32_t samples_per_channel =
        bytes_per_frame != 0 ? static_cast<std::uint32_t>(payload_size / bytes_per_frame) : 0;

    st2110::MtlWorkerAudioBlockReadyEvent event{
        .graph_id = graph_id,
        .ring_id = ring->descriptor().ring_id,
        .slot_id = slot_index,
        .sequence = sequence,
        .sample_rate_hz = cfg.media.sampling_rate_hz,
        .channels = channels,
        .samples_per_channel = samples_per_channel,
        .rtp_timestamp = frame->rtp_timestamp,
        .receive_timestamp_ns = static_cast<st2110::TimestampNs>(frame->receive_timestamp),
        .payload_size = static_cast<std::size_t>(payload_size),
        .partial = false,
    };

    auto wrote = event_writer->write_event(st2110::MtlWorkerControlEvent{event});
    if (!wrote.has_value()) {
        return std::unexpected(wrote.error());
    }

    return true;
}

void fill_st30p_session_port(st30p_rx_ops &ops, const mtl_session_port session_port,
                             const st2110::MtlRuntimePortConfig &runtime_port,
                             const st2110::MtlAudioSessionPortConfig &session_port_cfg) {
    std::memcpy(ops.port.ip_addr[session_port], session_port_cfg.ip_addr.data(), session_port_cfg.ip_addr.size());

    if (session_port_cfg.source_ip.has_value()) {
        std::memcpy(ops.port.mcast_sip_addr[session_port], session_port_cfg.source_ip->data(),
                    session_port_cfg.source_ip->size());
    }

    std::snprintf(ops.port.port[session_port], MTL_PORT_MAX_LEN, "%s", runtime_port.port_name.c_str());

    ops.port.udp_port[session_port] = session_port_cfg.udp_port;
}

[[nodiscard]] std::expected<st30p_rx_ops, st2110::Error>
make_st30p_rx_ops(const st2110::MtlAudioStartConfig &cfg) noexcept {
    if (cfg.redundant.has_value() != cfg.runtime.redundant_port.has_value()) {
        return std::unexpected(st2110::Error::InvalidValue);
    }

    if (cfg.media.channel_count == 0 || cfg.frame_buffer_count == 0 || cfg.frame_buffer_duration_ns == 0) {
        return std::unexpected(st2110::Error::InvalidValue);
    }

    auto fmt = map_mtl_audio_pcm_format(cfg.pcm_format);
    if (!fmt.has_value()) {
        return std::unexpected(fmt.error());
    }

    auto sampling = map_mtl_audio_sampling(cfg.sampling);
    if (!sampling.has_value()) {
        return std::unexpected(sampling.error());
    }

    auto ptime = map_mtl_audio_packet_time(cfg.packet_time);
    if (!ptime.has_value()) {
        return std::unexpected(ptime.error());
    }

    const int framebuff_size = st30_calculate_framebuff_size(*fmt, *ptime, *sampling, cfg.media.channel_count,
                                                             cfg.frame_buffer_duration_ns, nullptr);
    if (framebuff_size <= 0) {
        return std::unexpected(st2110::Error::SystemFailure);
    }

    st30p_rx_ops ops{};
    ops.name = "st2110_mtl_worker_audio_rx";
    ops.priv = nullptr;

    ops.port.num_port = cfg.redundant.has_value() ? 2 : 1;

    fill_st30p_session_port(ops, MTL_SESSION_PORT_P, cfg.runtime.primary_port, cfg.primary);

    if (cfg.redundant.has_value()) {
        fill_st30p_session_port(ops, MTL_SESSION_PORT_R, *cfg.runtime.redundant_port, *cfg.redundant);
    }

    ops.port.payload_type = cfg.expected_payload_type;

    ops.fmt = *fmt;
    ops.channel = cfg.media.channel_count;
    ops.sampling = *sampling;
    ops.ptime = *ptime;

    ops.framebuff_cnt = cfg.frame_buffer_count;
    ops.framebuff_size = static_cast<std::uint32_t>(framebuff_size);

    /*
     * Required for the future worker receive thread:
     * st30p_rx_get_frame() blocks until an audio frame is ready, and stop wakes it.
     */
    ops.flags = ST30P_RX_FLAG_BLOCK_GET;

    return ops;
}

} // namespace

struct MtlAudioRxSession::Impl {
    st2110::MtlAudioStartConfig cfg{};
    st30p_rx_handle rx = nullptr;

    MtlWorkerGraphStats *stats = nullptr;
    MtlWorkerEventWriter *event_writer = nullptr;
    st2110::MtlWorkerSharedMemoryRingMap *media_ring = nullptr;
    st2110::MtlWorkerGraphId graph_id = 0;
    std::uint32_t next_slot_index = 0;
    std::uint64_t next_sequence = 1;

    std::jthread receive_thread{};

    explicit Impl(st2110::MtlWorkerGraphId graph, st2110::MtlAudioStartConfig session_cfg,
              st30p_rx_handle session_handle, MtlWorkerGraphStats &graph_stats, MtlWorkerEventWriter &writer,
              st2110::MtlWorkerSharedMemoryRingMap *bound_media_ring)
    : cfg(std::move(session_cfg)),
      graph_id(graph),
      rx(session_handle),
      stats(&graph_stats),
      event_writer(&writer),
      media_ring(bound_media_ring) {}

    ~Impl() {
        stop_thread_noexcept();

        if (rx) {
            st30p_rx_free(rx);
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
            st30p_rx_wake_block(rx);
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
            st30p_rx_wake_block(rx);
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
            st30_frame *frame = st30p_rx_get_frame(rx);
            if (!frame) {
                continue;
            }

            if (stats) {
                stats->record_audio_block_received();
            }

            auto exported = export_audio_frame_to_ring(media_ring, event_writer, graph_id, cfg, frame, next_slot_index,
                                           next_sequence);
            if (!exported.has_value() || !*exported) {
                if (stats) {
                    stats->record_audio_block_dropped();
                }
            }

            st30p_rx_put_frame(rx, frame);
        }
    }
};

std::expected<std::unique_ptr<MtlAudioRxSession>, st2110::Error>
MtlAudioRxSession::create(MtlRuntimeContext &runtime, st2110::MtlWorkerGraphId graph_id,
                          st2110::MtlAudioStartConfig cfg, MtlWorkerGraphStats &stats,
                          MtlWorkerEventWriter &event_writer,
                          st2110::MtlWorkerSharedMemoryRingMap *media_ring) {
    if (!runtime.handle()) {
        return std::unexpected(st2110::Error::InvalidBackendState);
    }

    auto valid_ring = validate_audio_media_ring(media_ring);
    if (!valid_ring.has_value()) {
        return std::unexpected(valid_ring.error());
    }

    auto ops = make_st30p_rx_ops(cfg);
    if (!ops.has_value()) {
        return std::unexpected(ops.error());
    }

    st30p_rx_handle rx = st30p_rx_create(runtime.handle(), &*ops);
    if (!rx) {
        return std::unexpected(st2110::Error::SystemFailure);
    }

    auto impl = std::make_unique<Impl>(graph_id, std::move(cfg), rx, stats, event_writer, media_ring);

    auto started = impl->start_thread();
    if (!started.has_value()) {
        return std::unexpected(started.error());
    }

    return std::unique_ptr<MtlAudioRxSession>(new MtlAudioRxSession(std::move(impl)));
}

MtlAudioRxSession::MtlAudioRxSession(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

MtlAudioRxSession::~MtlAudioRxSession() = default;

void MtlAudioRxSession::wake_block() noexcept {
    if (impl_->rx) {
        st30p_rx_wake_block(impl_->rx);
    }
}

const st2110::MtlAudioStartConfig &MtlAudioRxSession::config() const noexcept { return impl_->cfg; }

const st2110::MtlWorkerSharedMemoryRingMap *MtlAudioRxSession::media_ring() const noexcept {
    return impl_->media_ring;
}

} // namespace st2110_mtl_rx_worker
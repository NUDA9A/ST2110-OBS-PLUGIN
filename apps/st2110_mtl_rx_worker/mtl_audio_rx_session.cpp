#include "mtl_audio_rx_session.hpp"

#include <mtl/st30_api.h>
#include <mtl/st30_pipeline_api.h>

#include <cstdio>
#include <cstring>
#include <cstdint>
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

    const int framebuff_size =
        st30_calculate_framebuff_size(*fmt, *ptime, *sampling, cfg.media.channel_count,
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

    explicit Impl(st2110::MtlAudioStartConfig session_cfg, st30p_rx_handle session_handle)
        : cfg(std::move(session_cfg)), rx(session_handle) {}

    ~Impl() {
        if (rx) {
            st30p_rx_wake_block(rx);
            st30p_rx_free(rx);
            rx = nullptr;
        }
    }
};

std::expected<std::unique_ptr<MtlAudioRxSession>, st2110::Error>
MtlAudioRxSession::create(MtlRuntimeContext &runtime, st2110::MtlAudioStartConfig cfg) {
    if (!runtime.handle()) {
        return std::unexpected(st2110::Error::InvalidBackendState);
    }

    auto ops = make_st30p_rx_ops(cfg);
    if (!ops.has_value()) {
        return std::unexpected(ops.error());
    }

    st30p_rx_handle rx = st30p_rx_create(runtime.handle(), &*ops);
    if (!rx) {
        return std::unexpected(st2110::Error::SystemFailure);
    }

    auto impl = std::make_unique<Impl>(std::move(cfg), rx);
    return std::unique_ptr<MtlAudioRxSession>(new MtlAudioRxSession(std::move(impl)));
}

MtlAudioRxSession::MtlAudioRxSession(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

MtlAudioRxSession::~MtlAudioRxSession() = default;

void MtlAudioRxSession::wake_block() noexcept {
    if (impl_->rx) {
        st30p_rx_wake_block(impl_->rx);
    }
}

const st2110::MtlAudioStartConfig &MtlAudioRxSession::config() const noexcept {
    return impl_->cfg;
}

} // namespace st2110_mtl_rx_worker
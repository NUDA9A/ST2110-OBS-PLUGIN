#include <st2110/backends/mtl/mtl_rx_audio_backend.hpp>

#include <st2110/foundation/error.hpp>
#include <st2110/foundation/timestamp.hpp>
#include <st2110/receive/audio/audio_frame_assembler.hpp>
#include <st2110/receive/audio/audio_packet.hpp>

#include <mtl/mtl_api.h>
#include <mtl/st30_api.h>
#include <mtl/st30_pipeline_api.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <expected>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

namespace st2110 {
namespace {

[[nodiscard]] std::expected<st30_fmt, Error> map_mtl_audio_pcm_format(const MtlAudioPcmFormat fmt) noexcept {
    switch (fmt) {
    case MtlAudioPcmFormat::Pcm16:
        return ST30_FMT_PCM16;
    case MtlAudioPcmFormat::Pcm24:
        return ST30_FMT_PCM24;
    }

    return std::unexpected(Error::Unsupported);
}

[[nodiscard]] std::expected<st30_sampling, Error> map_mtl_audio_sampling(const MtlAudioSampling sampling) noexcept {
    switch (sampling) {
    case MtlAudioSampling::K48:
        return ST30_SAMPLING_48K;
    }

    return std::unexpected(Error::Unsupported);
}

[[nodiscard]] std::expected<st30_ptime, Error> map_mtl_audio_packet_time(const MtlAudioPacketTime packet_time) noexcept {
    switch (packet_time) {
    case MtlAudioPacketTime::Ptime1ms:
        return ST30_PTIME_1MS;
    }

    return std::unexpected(Error::Unsupported);
}

[[nodiscard]] std::expected<mtl_init_params, Error> make_mtl_init_params(const MtlRuntimeConfig &cfg) noexcept {
    mtl_init_params params{};

    params.num_ports = cfg.redundant_port.has_value() ? 2 : 1;

    const auto fill_port = [](struct mtl_init_params &params, const enum mtl_port port_index,
                              const MtlRuntimePortConfig &port_cfg) {
        std::snprintf(params.port[port_index], MTL_PORT_MAX_LEN, "%s", port_cfg.port_name.c_str());

        params.pmd[port_index] = MTL_PMD_DPDK_USER;
        params.net_proto[port_index] = MTL_PROTO_STATIC;

        /*
         * RX audio backend needs one RX queue per initialized MTL device port.
         * TX queues are not needed for RX-only audio backend.
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

void fill_st30p_session_port(st30p_rx_ops &ops, const mtl_session_port session_port,
                             const MtlRuntimePortConfig &runtime_port,
                             const MtlAudioSessionPortConfig &session_port_cfg) {
    std::memcpy(ops.port.ip_addr[session_port], session_port_cfg.ip_addr.data(), session_port_cfg.ip_addr.size());

    if (session_port_cfg.source_ip.has_value()) {
        std::memcpy(ops.port.mcast_sip_addr[session_port], session_port_cfg.source_ip->data(),
                    session_port_cfg.source_ip->size());
    }

    std::snprintf(ops.port.port[session_port], MTL_PORT_MAX_LEN, "%s", runtime_port.port_name.c_str());

    ops.port.udp_port[session_port] = session_port_cfg.udp_port;
}

[[nodiscard]] std::expected<st30p_rx_ops, Error> make_st30p_rx_ops(const MtlAudioStartConfig &cfg) noexcept {
    if (cfg.redundant.has_value() != cfg.runtime.redundant_port.has_value()) {
        return std::unexpected(Error::InvalidValue);
    }

    if (cfg.media.channel_count == 0 || cfg.frame_buffer_count == 0 || cfg.frame_buffer_duration_ns == 0) {
        return std::unexpected(Error::InvalidValue);
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
        st30_calculate_framebuff_size(*fmt, *ptime, *sampling, cfg.media.channel_count, cfg.frame_buffer_duration_ns,
                                      nullptr);
    if (framebuff_size <= 0) {
        return std::unexpected(Error::SystemFailure);
    }

    if (static_cast<unsigned int>(framebuff_size) > std::numeric_limits<std::uint32_t>::max()) {
        return std::unexpected(Error::BufferTooSmall);
    }

    st30p_rx_ops ops{};
    ops.name = "st2110_mtl_audio_rx";
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

    ops.flags = ST30P_RX_FLAG_BLOCK_GET;

    return ops;
}

[[nodiscard]] TimestampNs mtl_receive_timestamp_ns(const st30_frame &frame) noexcept {
    return static_cast<TimestampNs>(frame.receive_timestamp);
}

[[nodiscard]] std::expected<std::uint32_t, Error>
mtl_audio_frame_samples_per_channel(const st30_frame &frame, const AudioPcmBitDepth bit_depth) {
    if (frame.channel == 0) {
        return std::unexpected(Error::InvalidValue);
    }

    if (frame.data_size == 0 || frame.data_size > frame.buffer_size) {
        return std::unexpected(Error::InvalidValue);
    }

    const std::size_t bytes_per_sample = audio_pcm_wire_sample_bytes(bit_depth);
    const std::size_t bytes_per_sample_frame = bytes_per_sample * static_cast<std::size_t>(frame.channel);

    if (bytes_per_sample_frame == 0 || frame.data_size % bytes_per_sample_frame != 0) {
        return std::unexpected(Error::InvalidValue);
    }

    const std::size_t samples_per_channel = frame.data_size / bytes_per_sample_frame;
    if (samples_per_channel == 0 || samples_per_channel > std::numeric_limits<std::uint32_t>::max()) {
        return std::unexpected(Error::InvalidValue);
    }

    return static_cast<std::uint32_t>(samples_per_channel);
}

} // namespace

class MtlRxAudioBackend::Impl {
  public:
    explicit Impl(MtlAudioStartConfig cfg) : cfg_(std::move(cfg)) {}

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

        auto ops = make_st30p_rx_ops(cfg_);
        if (!ops.has_value()) {
            mtl_uninit(staged_mt);
            return std::unexpected(ops.error());
        }

        ops->priv = this;

        st30p_rx_handle staged_rx = st30p_rx_create(staged_mt, &*ops);
        if (!staged_rx) {
            mtl_uninit(staged_mt);
            return std::unexpected(Error::SystemFailure);
        }

        {
            std::lock_guard lock(mutex_);

            sink_ = sink;
            mt_ = staged_mt;
            rx_ = staged_rx;

            expected_mtl_format_ = ops->fmt;
            expected_mtl_sampling_ = ops->sampling;
            expected_mtl_packet_time_ = ops->ptime;

            stop_requested_.store(false);
        }

        receive_thread_ = std::jthread([this](std::stop_token stop_token) { receive_loop(stop_token); });

        return true;
    }

    [[nodiscard]] RxBackendLifecycleResult stop() noexcept {
        st30p_rx_handle rx_to_wake = nullptr;
        std::jthread thread_to_join{};

        {
            std::lock_guard lock(mutex_);

            stop_requested_.store(true);

            rx_to_wake = rx_;
            thread_to_join = std::move(receive_thread_);
        }

        if (rx_to_wake) {
            st30p_rx_wake_block(rx_to_wake);
        }

        thread_to_join = {};

        {
            std::lock_guard lock(mutex_);

            if (rx_) {
                st30p_rx_free(rx_);
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
            st30_frame *frame = st30p_rx_get_frame(rx_);
            if (!frame) {
                continue;
            }

            handle_frame(*frame);

            st30p_rx_put_frame(rx_, frame);
        }
    }

    void handle_frame(const st30_frame &mtl_frame) {
        IFrameSink *sink = nullptr;
        AudioMediaDescription media{};
        st30_fmt expected_format{};
        st30_sampling expected_sampling{};
        st30_ptime expected_packet_time{};

        {
            std::lock_guard lock(mutex_);

            sink = sink_;
            media = cfg_.media;
            expected_format = expected_mtl_format_;
            expected_sampling = expected_mtl_sampling_;
            expected_packet_time = expected_mtl_packet_time_;
        }

        if (!sink) {
            return;
        }

        if (!mtl_frame.addr) {
            return;
        }

        if (mtl_frame.fmt != expected_format || mtl_frame.sampling != expected_sampling ||
            mtl_frame.ptime != expected_packet_time || mtl_frame.channel != media.channel_count) {
            return;
        }

        auto copied_frame = copy_frame(mtl_frame, media);
        if (!copied_frame.has_value()) {
            return;
        }

        sink->on_audio_frame(std::move(*copied_frame), FrameTimingMetadata{
                                                          .rtp_timestamp = mtl_frame.rtp_timestamp,
                                                          .receive_timestamp_ns = mtl_receive_timestamp_ns(mtl_frame),
                                                      });
    }

    [[nodiscard]] std::expected<AudioBuffer, Error> copy_frame(const st30_frame &mtl_frame,
                                                               const AudioMediaDescription &media) const {
        auto samples_per_channel = mtl_audio_frame_samples_per_channel(mtl_frame, media.pcm_bit_depth);
        if (!samples_per_channel.has_value()) {
            return std::unexpected(samples_per_channel.error());
        }

        AudioBuffer out(media.sampling_rate_hz, media.channel_count, *samples_per_channel);

        const std::size_t bytes_per_sample = audio_pcm_wire_sample_bytes(media.pcm_bit_depth);
        const auto *src = static_cast<const std::uint8_t *>(mtl_frame.addr);

        for (std::uint32_t sample_index = 0; sample_index < *samples_per_channel; ++sample_index) {
            for (std::uint16_t channel = 0; channel < media.channel_count; ++channel) {
                const std::size_t sample_ordinal =
                    static_cast<std::size_t>(sample_index) * static_cast<std::size_t>(media.channel_count) +
                    static_cast<std::size_t>(channel);

                const std::size_t payload_offset = sample_ordinal * bytes_per_sample;

                out.sample(sample_index, channel) = decode_audio_pcm_wire_sample_to_s32(
                    ByteSpan{src + payload_offset, bytes_per_sample}, media.pcm_bit_depth);
            }
        }

        return out;
    }

    MtlAudioStartConfig cfg_{};

    mutable std::mutex mutex_{};

    IFrameSink *sink_ = nullptr;

    mtl_handle mt_ = nullptr;
    st30p_rx_handle rx_ = nullptr;

    std::jthread receive_thread_{};
    std::atomic_bool stop_requested_{true};

    st30_fmt expected_mtl_format_{};
    st30_sampling expected_mtl_sampling_{};
    st30_ptime expected_mtl_packet_time_{};
};

MtlRxAudioBackend::MtlRxAudioBackend(MtlAudioStartConfig cfg) : impl_(std::make_unique<Impl>(std::move(cfg))) {}

MtlRxAudioBackend::~MtlRxAudioBackend() = default;

RxBackendLifecycleResult MtlRxAudioBackend::start(IFrameSink *sink) {
    return impl_->start(sink);
}

RxBackendLifecycleResult MtlRxAudioBackend::stop() {
    return impl_->stop();
}

} // namespace st2110
#ifndef ST2110_OBS_PLUGIN_MTL_AUDIO_START_CONFIG_HPP
#define ST2110_OBS_PLUGIN_MTL_AUDIO_START_CONFIG_HPP

#include <st2110/backends/mtl/mtl_runtime_resolver.hpp>
#include <st2110/foundation/error.hpp>
#include <st2110/model/audio/audio_signaling.hpp>
#include <st2110/receive/shared/receive_start_request.hpp>

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace st2110 {

inline constexpr std::uint64_t mtlAudioDefaultFrameBufferDurationNs = 10'000'000ULL;
inline constexpr std::uint16_t mtlAudioDefaultFrameBufferCount = 3;

enum class MtlAudioPcmFormat {
    Pcm16,
    Pcm24,
};

enum class MtlAudioSampling {
    K48,
};

enum class MtlAudioPacketTime {
    Ptime1ms,
};

struct MtlAudioSessionPortConfig {
    /*
     * MTL st30p_rx_ops::port.ip_addr[...] for this session port.
     *
     * For multicast this is the multicast group address.
     * For unicast this is the sender/stream address as required by MTL session setup.
     */
    std::array<std::uint8_t, 4> ip_addr{};

    /*
     * Optional multicast source-filter address.
     *
     * Backend projection maps this to the MTL ST30P RX session source-filter
     * field for the corresponding session port.
     */
    std::optional<std::array<std::uint8_t, 4>> source_ip{};

    /*
     * UDP destination port for this ST30P RX session leg.
     *
     * Projection target:
     *   st30p_rx_ops::port.udp_port[...]
     */
    std::uint16_t udp_port = 0;
};

struct MtlAudioStartConfig {
    /*
     * MTL device/runtime configuration used for mtl_init(...).
     */
    MtlRuntimeConfig runtime{};

    /*
     * Primary ST30P RX session leg.
     *
     * Projection target:
     *   st30p_rx_ops::port fields at MTL_SESSION_PORT_P.
     */
    MtlAudioSessionPortConfig primary{};

    /*
     * Optional redundant ST30P RX session leg.
     *
     * If present, runtime.redundant_port must also be present so the backend can
     * initialize MTL_PORT_R and fill MTL_SESSION_PORT_R.
     */
    std::optional<MtlAudioSessionPortConfig> redundant{};

    /*
     * RTP payload type expected for this ST30 stream.
     *
     * Projection target:
     *   st30p_rx_ops::port.payload_type
     */
    std::uint8_t expected_payload_type = 0;

    /*
     * Project audio media model.
     *
     * The MTL backend supports only the explicit MVP branch:
     *   linear PCM, 48 kHz, 1 ms packet time, 1..8 channels, PCM16/PCM24.
     */
    AudioMediaDescription media{};

    /*
     * Derived from media.sampling_rate_hz and media.packet_time_us.
     *
     * This is kept explicit so the MTL audio backend does not hardcode 48 samples.
     */
    std::uint32_t samples_per_packet = 0;

    /*
     * MTL-supported audio projection axes.
     *
     * Projection targets:
     *   st30p_rx_ops::fmt
     *   st30p_rx_ops::sampling
     *   st30p_rx_ops::ptime
     */
    MtlAudioPcmFormat pcm_format = MtlAudioPcmFormat::Pcm24;
    MtlAudioSampling sampling = MtlAudioSampling::K48;
    MtlAudioPacketTime packet_time = MtlAudioPacketTime::Ptime1ms;

    /*
     * ST30P frame buffer policy.
     *
     * The default is a short 10 ms buffer, matching the MTL sample pattern and
     * keeping the policy local to the MTL audio projection/backend boundary.
     */
    std::uint16_t frame_buffer_count = mtlAudioDefaultFrameBufferCount;
    std::uint64_t frame_buffer_duration_ns = mtlAudioDefaultFrameBufferDurationNs;
};

struct MtlAudioStartProjectionSettings {
    std::uint16_t frame_buffer_count = mtlAudioDefaultFrameBufferCount;
    std::uint64_t frame_buffer_duration_ns = mtlAudioDefaultFrameBufferDurationNs;
};

[[nodiscard]] inline std::expected<MtlAudioPcmFormat, Error>
project_audio_media_to_mtl_pcm_format(const AudioMediaDescription &media) {
    if (media.pcm_encoding != AudioPcmEncoding::LinearPcm) {
        return std::unexpected(Error::Unsupported);
    }

    switch (media.pcm_bit_depth) {
    case AudioPcmBitDepth::Bits16:
        return MtlAudioPcmFormat::Pcm16;
    case AudioPcmBitDepth::Bits24:
        return MtlAudioPcmFormat::Pcm24;
    }

    return std::unexpected(Error::Unsupported);
}

[[nodiscard]] inline std::expected<MtlAudioSampling, Error>
project_audio_media_to_mtl_sampling(const AudioMediaDescription &media) {
    switch (media.sampling_rate_hz) {
    case 0:
        return std::unexpected(Error::InvalidValue);
    case 48000:
        return MtlAudioSampling::K48;
    default:
        return std::unexpected(Error::Unsupported);
    }
}

[[nodiscard]] inline std::expected<MtlAudioPacketTime, Error>
project_audio_media_to_mtl_packet_time(const AudioMediaDescription &media) {
    switch (media.packet_time_us) {
    case 0:
        return std::unexpected(Error::InvalidValue);
    case 1000:
        return MtlAudioPacketTime::Ptime1ms;
    default:
        return std::unexpected(Error::Unsupported);
    }
}

[[nodiscard]] inline std::expected<std::uint16_t, Error>
project_audio_media_to_mtl_channel_count(const AudioMediaDescription &media) {
    if (media.channel_count == 0) {
        return std::unexpected(Error::InvalidValue);
    }

    if (media.channel_count > 8) {
        return std::unexpected(Error::Unsupported);
    }

    return media.channel_count;
}

[[nodiscard]] inline std::expected<MtlAudioSessionPortConfig, Error>
project_receive_remote_leg_to_mtl_audio_session_port(const ReceiveRemoteLeg &leg) {
    auto ip_addr = parse_mtl_ipv4_address(leg.destination.destination_address);
    if (!ip_addr.has_value()) {
        return std::unexpected(ip_addr.error());
    }

    MtlAudioSessionPortConfig result{
        .ip_addr = *ip_addr,
        .source_ip = std::nullopt,
        .udp_port = leg.udp_port,
    };

    if (!leg.source_filter.source_addresses.empty()) {
        auto source_ip = parse_mtl_ipv4_address(leg.source_filter.source_addresses.front());
        if (!source_ip.has_value()) {
            return std::unexpected(source_ip.error());
        }

        result.source_ip = *source_ip;
    }

    return result;
}

[[nodiscard]] inline std::expected<MtlAudioStartConfig, Error>
project_receive_start_request_to_mtl_audio_start(const ReceiveStartRequest &request,
                                                 const MtlAudioStartProjectionSettings &settings) {
    if (settings.frame_buffer_count == 0 || settings.frame_buffer_duration_ns == 0) {
        return std::unexpected(Error::InvalidValue);
    }

    const auto *bootstrap = std::get_if<AudioReceiveBootstrap>(&request.media);
    if (!bootstrap) {
        return std::unexpected(Error::InvalidValue);
    }

    if (bootstrap->receive_bootstrap.legs.empty() || request.local.legs.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    if (bootstrap->receive_bootstrap.legs.size() != request.local.legs.size()) {
        return std::unexpected(Error::InvalidValue);
    }

    const auto &media = bootstrap->stream.media;

    auto pcm_format = project_audio_media_to_mtl_pcm_format(media);
    if (!pcm_format.has_value()) {
        return std::unexpected(pcm_format.error());
    }

    auto sampling = project_audio_media_to_mtl_sampling(media);
    if (!sampling.has_value()) {
        return std::unexpected(sampling.error());
    }

    auto packet_time = project_audio_media_to_mtl_packet_time(media);
    if (!packet_time.has_value()) {
        return std::unexpected(packet_time.error());
    }

    auto channel_count = project_audio_media_to_mtl_channel_count(media);
    if (!channel_count.has_value()) {
        return std::unexpected(channel_count.error());
    }

    auto samples_per_packet = audio_samples_per_packet_from_media_description(media);
    if (!samples_per_packet.has_value()) {
        return std::unexpected(samples_per_packet.error());
    }

    auto runtime = project_receive_local_policy_to_mtl_runtime_config(bootstrap->receive_bootstrap, request.local);
    if (!runtime.has_value()) {
        return std::unexpected(runtime.error());
    }

    auto primary = project_receive_remote_leg_to_mtl_audio_session_port(bootstrap->receive_bootstrap.legs[0]);
    if (!primary.has_value()) {
        return std::unexpected(primary.error());
    }

    MtlAudioStartConfig result{
        .runtime = *runtime,
        .primary = *primary,
        .redundant = std::nullopt,
        .expected_payload_type = bootstrap->stream.receive_signaled_stream.expected_payload_type,
        .media = media,
        .samples_per_packet = *samples_per_packet,
        .pcm_format = *pcm_format,
        .sampling = *sampling,
        .packet_time = *packet_time,
        .frame_buffer_count = settings.frame_buffer_count,
        .frame_buffer_duration_ns = settings.frame_buffer_duration_ns,
    };

    if (bootstrap->receive_bootstrap.topology == ReceiveTopologyKind::RedundantPair) {
        if (bootstrap->receive_bootstrap.legs.size() < 2 || request.local.legs.size() < 2) {
            return std::unexpected(Error::InvalidValue);
        }

        auto redundant = project_receive_remote_leg_to_mtl_audio_session_port(bootstrap->receive_bootstrap.legs[1]);
        if (!redundant.has_value()) {
            return std::unexpected(redundant.error());
        }

        result.redundant = *redundant;
    }

    return result;
}

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_MTL_AUDIO_START_CONFIG_HPP
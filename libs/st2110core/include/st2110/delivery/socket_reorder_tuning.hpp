#ifndef ST2110_OBS_PLUGIN_SOCKET_REORDER_TUNING_HPP
#define ST2110_OBS_PLUGIN_SOCKET_REORDER_TUNING_HPP

#include <st2110/model/audio/audio_signaling.hpp>
#include <st2110/model/video/video_media_types.hpp>
#include <st2110/receive/audio/audio_receive_bootstrap.hpp>
#include <st2110/receive/shared/receive_reorder_tolerance_policy.hpp>
#include <st2110/receive/video/video_receive_bootstrap.hpp>

#include <cstddef>
#include <cstdint>

namespace st2110 {

inline constexpr std::uint64_t socketReorderJitterBudgetNs = 5'000'000ULL;

inline constexpr std::uint32_t socketVideoMinimumReorderWindowPackets = 64;
inline constexpr std::uint32_t socketVideoMaximumReorderWindowPackets = 8192;
inline constexpr std::uint32_t socketVideoReorderMarginPackets = 16;

inline constexpr std::uint32_t socketAudioMinimumReorderWindowPackets = 8;
inline constexpr std::uint32_t socketAudioMaximumReorderWindowPackets = 512;
inline constexpr std::uint32_t socketAudioReorderMarginPackets = 4;

inline constexpr std::size_t socketDefaultMaxUdpDatagramBytes = 1460;
inline constexpr std::size_t socketUdpHeaderBytes = 8;
inline constexpr std::size_t socketRtpHeaderBytes = 12;
inline constexpr std::size_t socketSt20PayloadHeaderMinimumBytes = 8;

struct VideoBitsPerPixelRatio {
    std::uint32_t numerator = 0;
    std::uint32_t denominator = 1;
};

[[nodiscard]] inline std::uint64_t socket_reorder_ceil_div_u64(const std::uint64_t numerator,
                                                               const std::uint64_t denominator) noexcept {
    if (denominator == 0) {
        return 0;
    }

    return numerator / denominator + ((numerator % denominator) == 0 ? 0 : 1);
}

[[nodiscard]] inline std::uint32_t socket_reorder_clamp_to_u32(const std::uint64_t value, const std::uint32_t minimum,
                                                               const std::uint32_t maximum) noexcept {
    if (value < minimum) {
        return minimum;
    }

    if (value > maximum) {
        return maximum;
    }

    return static_cast<std::uint32_t>(value);
}

[[nodiscard]] inline std::uint64_t socket_reorder_video_frame_period_ns(const VideoMediaDescription &media) noexcept {
    if (media.fps_num == 0 || media.fps_den == 0) {
        return 0;
    }

    constexpr std::uint64_t ns_per_second = 1'000'000'000ULL;
    return socket_reorder_ceil_div_u64(ns_per_second * static_cast<std::uint64_t>(media.fps_den),
                                       static_cast<std::uint64_t>(media.fps_num));
}

[[nodiscard]] inline std::uint64_t socket_reorder_audio_packet_time_ns(const AudioMediaDescription &media) noexcept {
    return static_cast<std::uint64_t>(media.packet_time_us) * 1'000ULL;
}

[[nodiscard]] inline VideoBitsPerPixelRatio
socket_reorder_video_bits_per_pixel_ratio(const VideoMediaDescription &media) noexcept {
    const std::uint32_t bits = media.depth.bits;

    switch (media.sampling.known) {
    case VideoSampling::Known::YCbCr420:
    case VideoSampling::Known::CLYCbCr420:
    case VideoSampling::Known::ICtCp420:
        return VideoBitsPerPixelRatio{.numerator = 3U * bits, .denominator = 2};

    case VideoSampling::Known::YCbCr422:
    case VideoSampling::Known::CLYCbCr422:
    case VideoSampling::Known::ICtCp422:
        return VideoBitsPerPixelRatio{.numerator = 2U * bits, .denominator = 1};

    case VideoSampling::Known::Key:
        return VideoBitsPerPixelRatio{.numerator = bits, .denominator = 1};

    case VideoSampling::Known::YCbCr444:
    case VideoSampling::Known::RGB:
    case VideoSampling::Known::XYZ:
    case VideoSampling::Known::CLYCbCr444:
    case VideoSampling::Known::ICtCp444:
    case VideoSampling::Known::Other:
        return VideoBitsPerPixelRatio{.numerator = 3U * bits, .denominator = 1};
    }

    return VideoBitsPerPixelRatio{.numerator = 3U * bits, .denominator = 1};
}

[[nodiscard]] inline std::uint64_t
socket_reorder_video_active_frame_bytes(const VideoMediaDescription &media) noexcept {
    if (media.width == 0 || media.height == 0 || media.depth.bits == 0) {
        return 0;
    }

    const VideoBitsPerPixelRatio bits_per_pixel = socket_reorder_video_bits_per_pixel_ratio(media);
    if (bits_per_pixel.numerator == 0 || bits_per_pixel.denominator == 0) {
        return 0;
    }

    const std::uint64_t pixels = static_cast<std::uint64_t>(media.width) * static_cast<std::uint64_t>(media.height);

    return socket_reorder_ceil_div_u64(pixels * bits_per_pixel.numerator,
                                       static_cast<std::uint64_t>(bits_per_pixel.denominator) * 8ULL);
}

[[nodiscard]] inline std::size_t
socket_reorder_video_effective_payload_bytes(const VideoReceiveBootstrap &bootstrap) noexcept {
    std::size_t max_udp_datagram_bytes = socketDefaultMaxUdpDatagramBytes;

    if (!bootstrap.receive_bootstrap.legs.empty() &&
        bootstrap.receive_bootstrap.legs.front().max_udp_datagram_bytes != 0) {
        max_udp_datagram_bytes = bootstrap.receive_bootstrap.legs.front().max_udp_datagram_bytes;
    }

    constexpr std::size_t overhead = socketUdpHeaderBytes + socketRtpHeaderBytes + socketSt20PayloadHeaderMinimumBytes;

    if (max_udp_datagram_bytes <= overhead) {
        return 1;
    }

    return max_udp_datagram_bytes - overhead;
}

[[nodiscard]] inline std::uint64_t
derive_socket_video_packets_per_frame(const VideoReceiveBootstrap &bootstrap) noexcept {
    const std::uint64_t active_frame_bytes = socket_reorder_video_active_frame_bytes(bootstrap.stream.media);
    const std::size_t payload_bytes = socket_reorder_video_effective_payload_bytes(bootstrap);

    const std::uint64_t packets_per_frame =
        socket_reorder_ceil_div_u64(active_frame_bytes, static_cast<std::uint64_t>(payload_bytes));

    return packets_per_frame == 0 ? 1 : packets_per_frame;
}

[[nodiscard]] inline std::uint32_t
derive_socket_video_reorder_window_packets(const VideoReceiveBootstrap &bootstrap) noexcept {
    const std::uint64_t frame_period_ns = socket_reorder_video_frame_period_ns(bootstrap.stream.media);
    if (frame_period_ns == 0) {
        return socketVideoMinimumReorderWindowPackets;
    }

    const std::uint64_t packets_per_frame = derive_socket_video_packets_per_frame(bootstrap);
    const std::uint64_t packet_spacing_ns = frame_period_ns / packets_per_frame;
    const std::uint64_t safe_packet_spacing_ns = packet_spacing_ns == 0 ? 1 : packet_spacing_ns;

    const std::uint64_t required_window =
        socket_reorder_ceil_div_u64(socketReorderJitterBudgetNs, safe_packet_spacing_ns) +
        socketVideoReorderMarginPackets;

    return socket_reorder_clamp_to_u32(required_window, socketVideoMinimumReorderWindowPackets,
                                       socketVideoMaximumReorderWindowPackets);
}

[[nodiscard]] inline std::uint32_t
derive_socket_audio_reorder_window_packets(const AudioReceiveBootstrap &bootstrap) noexcept {
    const std::uint64_t packet_time_ns = socket_reorder_audio_packet_time_ns(bootstrap.stream.media);
    if (packet_time_ns == 0) {
        return socketAudioMinimumReorderWindowPackets;
    }

    const std::uint64_t required_window =
        socket_reorder_ceil_div_u64(socketReorderJitterBudgetNs, packet_time_ns) + socketAudioReorderMarginPackets;

    return socket_reorder_clamp_to_u32(required_window, socketAudioMinimumReorderWindowPackets,
                                       socketAudioMaximumReorderWindowPackets);
}

[[nodiscard]] inline ReorderBufferConfig
derive_socket_video_reorder_buffer_config(const VideoReceiveBootstrap &bootstrap,
                                          const ReceiveReorderGapPolicy reorder_tolerance_policy) noexcept {
    const std::uint32_t window_size_packets = derive_socket_video_reorder_window_packets(bootstrap);

    return ReorderBufferConfig{
        .window_size_packets = window_size_packets,
        .reorder_tolerance_policy = reorder_tolerance_policy,
        .flush_after_n_packets = window_size_packets,
    };
}

[[nodiscard]] inline ReorderBufferConfig
derive_socket_audio_reorder_buffer_config(const AudioReceiveBootstrap &bootstrap,
                                          const ReceiveReorderGapPolicy reorder_tolerance_policy) noexcept {
    const std::uint32_t window_size_packets = derive_socket_audio_reorder_window_packets(bootstrap);

    return ReorderBufferConfig{
        .window_size_packets = window_size_packets,
        .reorder_tolerance_policy = reorder_tolerance_policy,
        .flush_after_n_packets = window_size_packets,
    };
}

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_SOCKET_REORDER_TUNING_HPP
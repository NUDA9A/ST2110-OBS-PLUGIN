#ifndef ST2110_OBS_PLUGIN_MTL_VIDEO_START_CONFIG_HPP
#define ST2110_OBS_PLUGIN_MTL_VIDEO_START_CONFIG_HPP

#include <st2110/backends/mtl/mtl_runtime_resolver.hpp>
#include <st2110/delivery/video/pixel_format.hpp>
#include <st2110/foundation/error.hpp>
#include <st2110/model/video/video_media_types.hpp>
#include <st2110/receive/shared/receive_start_request.hpp>

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace st2110 {

enum class MtlVideoFrameRate {
    P23_98,
    P24,
    P25,
    P29_97,
    P30,
    P50,
    P59_94,
    P60,
    P100,
    P119_88,
    P120,
};

enum class MtlVideoTransportFormat {
    Yuv422_8Bit,
    Yuv422_10Bit,
    Yuv422_12Bit,
    Yuv422_16Bit,
    Yuv420_8Bit,
    Yuv420_10Bit,
    Yuv420_12Bit,
    Yuv420_16Bit,
    Yuv444_8Bit,
    Yuv444_10Bit,
    Yuv444_12Bit,
    Yuv444_16Bit,
    Rgb8,
    Rgb10,
    Rgb12,
    Rgb16,
};

struct MtlVideoSessionPortConfig {
    /*
     * MTL st20p_rx_ops::port.ip_addr[...] for this session port.
     *
     * For multicast this is the multicast group address.
     * For unicast this is the sender/stream address as required by MTL session setup.
     */
    std::array<std::uint8_t, 4> ip_addr{};

    /*
     * Optional multicast source-filter address.
     *
     * Backend projection maps this to the MTL ST20P RX session source-filter
     * field for the corresponding session port.
     */
    std::optional<std::array<std::uint8_t, 4>> source_ip{};

    /*
     * UDP destination port for this ST20P RX session leg.
     *
     * Projection target:
     *   st20p_rx_ops::port.udp_port[...]
     */
    std::uint16_t udp_port = 0;
};

struct MtlVideoStartConfig {
    /*
     * MTL device/runtime configuration used for mtl_init(...).
     */
    MtlRuntimeConfig runtime{};

    /*
     * Primary ST20P RX session leg.
     *
     * Projection target:
     *   st20p_rx_ops::port fields at MTL_SESSION_PORT_P.
     */
    MtlVideoSessionPortConfig primary{};

    /*
     * Optional redundant ST20P RX session leg.
     *
     * If present, runtime.redundant_port must also be present so the backend can
     * initialize MTL_PORT_R and fill MTL_SESSION_PORT_R.
     */
    std::optional<MtlVideoSessionPortConfig> redundant{};

    /*
     * RTP payload type expected for this ST20 stream.
     *
     * Projection target:
     *   st20p_rx_ops::port.payload_type
     */
    std::uint8_t expected_payload_type = 0;

    /*
     * Signaled video dimensions.
     *
     * Projection target:
     *   st20p_rx_ops::width
     *   st20p_rx_ops::height
     */
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    /*
     * MTL-supported frame rate value.
     *
     * Projection target:
     *   st20p_rx_ops::fps
     */
    MtlVideoFrameRate fps = MtlVideoFrameRate::P59_94;

    /*
     * Project scan-mode axis.
     *
     * Backend projection maps this to MTL st20p_rx_ops::interlaced where
     * possible, or returns Unsupported from the concrete MTL branch.
     */
    VideoScanMode scan_mode = VideoScanMode::Progressive;

    /*
     * ST20 transport format and MTL output frame format.
     *
     * Projection targets:
     *   st20p_rx_ops::transport_fmt
     *   st20p_rx_ops::output_fmt
     */
    MtlVideoTransportFormat transport_format = MtlVideoTransportFormat::Yuv422_8Bit;
    PixelFormat output_format = PixelFormat::UYVY;

    /*
     * ST20P frame buffer count.
     *
     * Projection target:
     *   st20p_rx_ops::framebuff_cnt
     */
    std::uint16_t frame_buffer_count = 3;
};

struct MtlVideoStartProjectionSettings {
    PixelFormat output_format = PixelFormat::UYVY;
    std::uint16_t frame_buffer_count = 3;
};

[[nodiscard]] inline std::expected<MtlVideoFrameRate, Error>
project_video_media_to_mtl_frame_rate(const VideoMediaDescription &media) {
    const auto matches = [&media](const std::uint32_t num, const std::uint32_t den) {
        return static_cast<std::uint64_t>(media.fps_num) * den == static_cast<std::uint64_t>(num) * media.fps_den;
    };

    if (matches(24000, 1001)) {
        return MtlVideoFrameRate::P23_98;
    }
    if (matches(24, 1)) {
        return MtlVideoFrameRate::P24;
    }
    if (matches(25, 1)) {
        return MtlVideoFrameRate::P25;
    }
    if (matches(30000, 1001)) {
        return MtlVideoFrameRate::P29_97;
    }
    if (matches(30, 1)) {
        return MtlVideoFrameRate::P30;
    }
    if (matches(50, 1)) {
        return MtlVideoFrameRate::P50;
    }
    if (matches(60000, 1001)) {
        return MtlVideoFrameRate::P59_94;
    }
    if (matches(60, 1)) {
        return MtlVideoFrameRate::P60;
    }
    if (matches(100, 1)) {
        return MtlVideoFrameRate::P100;
    }
    if (matches(120000, 1001)) {
        return MtlVideoFrameRate::P119_88;
    }
    if (matches(120, 1)) {
        return MtlVideoFrameRate::P120;
    }

    return std::unexpected(Error::Unsupported);
}

[[nodiscard]] inline std::expected<MtlVideoTransportFormat, Error>
project_video_media_to_mtl_transport_format(const VideoMediaDescription &media) {
    if (media.depth.floating_point) {
        return std::unexpected(Error::Unsupported);
    }

    const auto bits = media.depth.bits;

    switch (media.sampling.known) {
    case VideoSampling::Known::YCbCr422:
        switch (bits) {
        case 8:
            return MtlVideoTransportFormat::Yuv422_8Bit;
        case 10:
            return MtlVideoTransportFormat::Yuv422_10Bit;
        case 12:
            return MtlVideoTransportFormat::Yuv422_12Bit;
        case 16:
            return MtlVideoTransportFormat::Yuv422_16Bit;
        default:
            return std::unexpected(Error::Unsupported);
        }

    case VideoSampling::Known::YCbCr420:
        switch (bits) {
        case 8:
            return MtlVideoTransportFormat::Yuv420_8Bit;
        case 10:
            return MtlVideoTransportFormat::Yuv420_10Bit;
        case 12:
            return MtlVideoTransportFormat::Yuv420_12Bit;
        case 16:
            return MtlVideoTransportFormat::Yuv420_16Bit;
        default:
            return std::unexpected(Error::Unsupported);
        }

    case VideoSampling::Known::YCbCr444:
        switch (bits) {
        case 8:
            return MtlVideoTransportFormat::Yuv444_8Bit;
        case 10:
            return MtlVideoTransportFormat::Yuv444_10Bit;
        case 12:
            return MtlVideoTransportFormat::Yuv444_12Bit;
        case 16:
            return MtlVideoTransportFormat::Yuv444_16Bit;
        default:
            return std::unexpected(Error::Unsupported);
        }

    case VideoSampling::Known::RGB:
        switch (bits) {
        case 8:
            return MtlVideoTransportFormat::Rgb8;
        case 10:
            return MtlVideoTransportFormat::Rgb10;
        case 12:
            return MtlVideoTransportFormat::Rgb12;
        case 16:
            return MtlVideoTransportFormat::Rgb16;
        default:
            return std::unexpected(Error::Unsupported);
        }

    default:
        return std::unexpected(Error::Unsupported);
    }
}

[[nodiscard]] inline std::expected<MtlVideoSessionPortConfig, Error>
project_receive_remote_leg_to_mtl_video_session_port(const ReceiveRemoteLeg &leg) {
    auto ip_addr = parse_mtl_ipv4_address(leg.destination.destination_address);
    if (!ip_addr.has_value()) {
        return std::unexpected(ip_addr.error());
    }

    MtlVideoSessionPortConfig result{
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

[[nodiscard]] inline std::expected<MtlVideoStartConfig, Error>
project_receive_start_request_to_mtl_video_start(const ReceiveStartRequest &request,
                                                 const MtlVideoStartProjectionSettings &settings) {
    const auto *bootstrap = std::get_if<VideoReceiveBootstrap>(&request.media);
    if (!bootstrap) {
        return std::unexpected(Error::InvalidValue);
    }

    if (bootstrap->receive_bootstrap.legs.empty() || request.local.legs.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    if (bootstrap->receive_bootstrap.legs.size() != request.local.legs.size()) {
        return std::unexpected(Error::InvalidValue);
    }

    auto runtime = project_receive_local_policy_to_mtl_runtime_config(bootstrap->receive_bootstrap, request.local);
    if (!runtime.has_value()) {
        return std::unexpected(runtime.error());
    }

    auto primary = project_receive_remote_leg_to_mtl_video_session_port(bootstrap->receive_bootstrap.legs[0]);
    if (!primary.has_value()) {
        return std::unexpected(primary.error());
    }

    auto fps = project_video_media_to_mtl_frame_rate(bootstrap->stream.media);
    if (!fps.has_value()) {
        return std::unexpected(fps.error());
    }

    auto transport_format = project_video_media_to_mtl_transport_format(bootstrap->stream.media);
    if (!transport_format.has_value()) {
        return std::unexpected(transport_format.error());
    }

    MtlVideoStartConfig result{
        .runtime = *runtime,
        .primary = *primary,
        .redundant = std::nullopt,
        .expected_payload_type = bootstrap->stream.receive_signaled_stream.expected_payload_type,
        .width = bootstrap->stream.media.width,
        .height = bootstrap->stream.media.height,
        .fps = *fps,
        .scan_mode = bootstrap->stream.scan_mode,
        .transport_format = *transport_format,
        .output_format = settings.output_format,
        .frame_buffer_count = settings.frame_buffer_count,
    };

    if (bootstrap->receive_bootstrap.topology == ReceiveTopologyKind::RedundantPair) {
        if (bootstrap->receive_bootstrap.legs.size() < 2 || request.local.legs.size() < 2) {
            return std::unexpected(Error::InvalidValue);
        }

        auto redundant = project_receive_remote_leg_to_mtl_video_session_port(bootstrap->receive_bootstrap.legs[1]);
        if (!redundant.has_value()) {
            return std::unexpected(redundant.error());
        }

        result.redundant = *redundant;
    }

    return result;
}

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_MTL_VIDEO_START_CONFIG_HPP
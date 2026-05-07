#ifndef ST2110_OBS_PLUGIN_VIDEO_RECEIVE_CAPABILITY_HPP
#define ST2110_OBS_PLUGIN_VIDEO_RECEIVE_CAPABILITY_HPP

#include <cstdint>
#include <expected>
#include <optional>
#include <string>

#include "config_validation.hpp"
#include "error.hpp"
#include "pixel_format.hpp"
#include "video_packing_mode.hpp"
#include "video_scan_mode.hpp"

namespace st2110 {

struct VideoSampling {
    enum class Known {
        YCbCr422,
        YCbCr444,
        YCbCr420,
        RGB,
        XYZ,
        Key,
        CLYCbCr444,
        CLYCbCr422,
        CLYCbCr420,
        ICtCp444,
        ICtCp422,
        ICtCp420,
        Other
    };

    Known known = Known::YCbCr422;
    std::optional<std::string> raw_token{};
};

struct VideoBitDepth {
    std::uint8_t bits = 8;
    bool floating_point = false;
};

struct VideoColorimetry {
    enum class Known { Bt601, Bt709, Bt2020, Bt2100, St2065_1, St2065_3, Unspecified, Xyz, Alpha, Other };

    Known known = Known::Bt709;
    std::optional<std::string> raw_token{};
};

struct VideoTransferCharacteristicSystem {
    enum class Known {
        SDR,
        PQ,
        HLG,
        Linear,
        Bt2100LinPq,
        Bt2100LinHlg,
        St2065_1,
        St428_1,
        Density,
        St2115LogS3,
        Unspecified,
        Other
    };

    Known known = Known::SDR;
    std::optional<std::string> raw_token{};
};

struct VideoSignalStandard {
    enum class Known { St2110_20_2017, St2110_20_2022, Other };

    Known known = Known::St2110_20_2022;
    std::optional<std::string> raw_token{};
};

struct VideoRange {
    enum class Known { Narrow, FullProtect, Full, Other };

    Known known = Known::Narrow;
    std::optional<std::string> raw_token{};
};

struct VideoPixelAspectRatio {
    std::uint32_t width = 1;
    std::uint32_t height = 1;
};

struct VideoMediaDescription {
    VideoSampling sampling{};
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t fps_num = 0;
    std::uint32_t fps_den = 1;
    VideoBitDepth depth{};
    VideoColorimetry colorimetry{};
    std::optional<VideoTransferCharacteristicSystem> transfer_characteristic_system{};
    std::optional<VideoSignalStandard> signal_standard{};
    std::optional<VideoRange> range{};
    VideoPixelAspectRatio pixel_aspect_ratio{};
};

enum class VideoTransportPayloadFormat {
    /*
     * Generic RFC 4175 / ST 2110-20 payload transport.
     * Exact sampling/depth/colorimetry/range semantics remain in VideoMediaDescription.
     */
    Rfc4175,

    Rfc4175Ycbcr422_8Bit,
    Rfc4175Ycbcr422_10Bit,
    Rfc4175Ycbcr422_12Bit,
    Rfc4175Ycbcr422_16Bit,
    Rfc4175Ycbcr420_8Bit,
    Rfc4175Ycbcr420_10Bit,
    Rfc4175Ycbcr420_12Bit,
    Rfc4175Ycbcr420_16Bit,
    Rfc4175Rgb_8Bit,
    Rfc4175Rgb_10Bit,
    Rfc4175Rgb_12Bit,
    Rfc4175Rgb_16Bit,
    Rfc4175Ycbcr444_8Bit,
    Rfc4175Ycbcr444_10Bit,
    Rfc4175Ycbcr444_12Bit,
    Rfc4175Ycbcr444_16Bit,

    CustomYcbcr422Planar10Le,
    CustomV210,
};

enum class VideoFrameHandoffFormat {
    Uyvy,
    Yuv422Planar8,
    Yuv422Planar10Le,
    Yuv422Planar12Le,
    Yuv422Planar16Le,
    V210,
    Y210,
    Yuv422Rfc4175Pgroup2Be10,
    Yuv422Rfc4175Pgroup2Be12,
    Yuv444Planar10Le,
    Yuv444Planar12Le,
    Yuv444Rfc4175Pgroup4Be10,
    Yuv444Rfc4175Pgroup2Be12,
    Yuv420Planar8,
    Argb,
    Bgra,
    Rgb8,
    GbrPlanar10Le,
    GbrPlanar12Le,
    RgbRfc4175Pgroup4Be10,
    RgbRfc4175Pgroup2Be12,
};

struct VideoReceiveRtpClock {
    std::uint32_t rtp_clock_rate = 90000;
};

enum class VideoReceiveTopologyKind {
    SingleStream,
    RedundantStreams,
};

struct VideoReceiveTopology {
    VideoReceiveTopologyKind kind = VideoReceiveTopologyKind::SingleStream;
    std::uint8_t stream_count = 1;
    std::optional<std::string> primary_mid{};
    std::optional<std::string> redundant_mid{};
};

struct VideoReceiveCapability {
    VideoMediaDescription media{};
    VideoScanMode scan_mode = VideoScanMode::Progressive;
    VideoPackingMode packing_mode = VideoPackingMode::Gpm;
    VideoTransportPayloadFormat transport_format = VideoTransportPayloadFormat::Rfc4175Ycbcr422_8Bit;
    VideoFrameHandoffFormat handoff_format = VideoFrameHandoffFormat::Uyvy;
    VideoReceiveRtpClock rtp_clock{};
    VideoReceiveTopology topology{};
};

[[nodiscard]] inline Error validate_video_sampling(const VideoSampling &sampling) {
    if (sampling.known == VideoSampling::Known::Other) {
        if (!sampling.raw_token.has_value() || sampling.raw_token->empty()) {
            return Error::InvalidValue;
        }
    } else if (sampling.raw_token.has_value()) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

[[nodiscard]] inline Error validate_video_bit_depth(const VideoBitDepth &depth) {
    if (depth.floating_point) {
        if (depth.bits != 16) {
            return Error::InvalidValue;
        }
    } else if (depth.bits != 8 && depth.bits != 10 && depth.bits != 12 && depth.bits != 16) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

[[nodiscard]] inline Error validate_video_colorimetry(const VideoColorimetry &colorimetry) {
    if (colorimetry.known == VideoColorimetry::Known::Other) {
        if (!colorimetry.raw_token.has_value() || colorimetry.raw_token->empty()) {
            return Error::InvalidValue;
        }
    } else if (colorimetry.raw_token.has_value()) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

[[nodiscard]] inline Error validate_video_transfer_characteristic_system(const VideoTransferCharacteristicSystem &tcs) {
    if (tcs.known == VideoTransferCharacteristicSystem::Known::Other) {
        if (!tcs.raw_token.has_value() || tcs.raw_token->empty()) {
            return Error::InvalidValue;
        }
    } else if (tcs.raw_token.has_value()) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

[[nodiscard]] inline Error validate_video_signal_standard(const VideoSignalStandard &ssn) {
    if (ssn.known == VideoSignalStandard::Known::Other) {
        if (!ssn.raw_token.has_value() || ssn.raw_token->empty()) {
            return Error::InvalidValue;
        }
    } else if (ssn.raw_token.has_value()) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

[[nodiscard]] inline Error validate_video_range(const VideoRange &range) {
    if (range.known == VideoRange::Known::Other) {
        if (!range.raw_token.has_value() || range.raw_token->empty()) {
            return Error::InvalidValue;
        }
    } else if (range.raw_token.has_value()) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

[[nodiscard]] inline Error validate_video_pixel_aspect_ratio(const VideoPixelAspectRatio &par) {
    if (par.width == 0 || par.height == 0) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

[[nodiscard]] inline Error validate_video_media_description_dimensions(std::uint32_t width, std::uint32_t height) {
    constexpr std::uint32_t max_signaled_video_dimension = 32767;

    if (width == 0 || height == 0) {
        return Error::InvalidValue;
    }

    if (width > max_signaled_video_dimension || height > max_signaled_video_dimension) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

[[nodiscard]] inline bool is_420_video_sampling(const VideoSampling &sampling) {
    switch (sampling.known) {
    case VideoSampling::Known::YCbCr420:
    case VideoSampling::Known::CLYCbCr420:
    case VideoSampling::Known::ICtCp420:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] inline Error validate_video_media_description_structure(const VideoMediaDescription &media) {
    if (Error err = validate_video_sampling(media.sampling); err != Error::Ok) {
        return err;
    }

    if (Error err = validate_video_bit_depth(media.depth); err != Error::Ok) {
        return err;
    }

    if (Error err = validate_video_colorimetry(media.colorimetry); err != Error::Ok) {
        return err;
    }

    if (media.transfer_characteristic_system.has_value()) {
        if (Error err = validate_video_transfer_characteristic_system(*media.transfer_characteristic_system);
            err != Error::Ok) {
            return err;
        }
    }

    if (media.signal_standard.has_value()) {
        if (Error err = validate_video_signal_standard(*media.signal_standard); err != Error::Ok) {
            return err;
        }
    }

    if (media.range.has_value()) {
        if (Error err = validate_video_range(*media.range); err != Error::Ok) {
            return err;
        }
    }

    if (Error err = validate_video_pixel_aspect_ratio(media.pixel_aspect_ratio); err != Error::Ok) {
        return err;
    }

    if (Error err = validate_video_media_description_dimensions(media.width, media.height); err != Error::Ok) {
        return err;
    }

    if (Error err = config_validation::validate_frame_rate(media.fps_num, media.fps_den); err != Error::Ok) {
        return err;
    }

    return Error::Ok;
}

[[nodiscard]] inline Error validate_video_media_description_cross_field_constraints(const VideoMediaDescription &media,
                                                                                    VideoScanMode scan_mode) {
    if (is_420_video_sampling(media.sampling) && scan_mode != VideoScanMode::Progressive) {
        return Error::InvalidValue;
    }

    if (media.sampling.known == VideoSampling::Known::Key) {
        if (media.colorimetry.known != VideoColorimetry::Known::Alpha || media.colorimetry.raw_token.has_value()) {
            return Error::InvalidValue;
        }

        if (media.transfer_characteristic_system.has_value()) {
            return Error::InvalidValue;
        }
    }

    const bool uses_alpha_colorimetry = (media.colorimetry.known == VideoColorimetry::Known::Alpha);
    const bool uses_st2115logs3_tcs =
        media.transfer_characteristic_system.has_value() &&
        media.transfer_characteristic_system->known == VideoTransferCharacteristicSystem::Known::St2115LogS3;

    const bool requires_st2110_20_2022 = uses_alpha_colorimetry || uses_st2115logs3_tcs;

    if (media.signal_standard.has_value()) {
        switch (media.signal_standard->known) {
        case VideoSignalStandard::Known::Other:
            break;

        case VideoSignalStandard::Known::St2110_20_2017:
            /*
             * 2017 cannot signal media that requires 2022-only extensions.
             */
            if (requires_st2110_20_2022) {
                return Error::InvalidValue;
            }
            break;

        case VideoSignalStandard::Known::St2110_20_2022:
            /*
             * 2022 is valid both for 2022-only media and for ordinary ST 2110-20 media.
             */
            break;

        default:
            return Error::InvalidValue;
        }
    }

    if (!media.range.has_value() || media.range->known == VideoRange::Known::Other) {
        return Error::Ok;
    }

    if (media.colorimetry.known == VideoColorimetry::Known::Bt2100) {
        switch (media.range->known) {
        case VideoRange::Known::Narrow:
        case VideoRange::Known::Full:
            return Error::Ok;
        case VideoRange::Known::FullProtect:
        default:
            return Error::InvalidValue;
        }
    }

    switch (media.range->known) {
    case VideoRange::Known::Narrow:
    case VideoRange::Known::FullProtect:
    case VideoRange::Known::Full:
        return Error::Ok;
    default:
        return Error::InvalidValue;
    }
}

[[nodiscard]] inline bool video_sampling_equal(const VideoSampling &lhs, const VideoSampling &rhs) {
    return lhs.known == rhs.known && lhs.raw_token == rhs.raw_token;
}

[[nodiscard]] inline bool video_bit_depth_equal(const VideoBitDepth &lhs, const VideoBitDepth &rhs) {
    return lhs.bits == rhs.bits && lhs.floating_point == rhs.floating_point;
}

[[nodiscard]] inline bool video_colorimetry_equal(const VideoColorimetry &lhs, const VideoColorimetry &rhs) {
    return lhs.known == rhs.known && lhs.raw_token == rhs.raw_token;
}

[[nodiscard]] inline bool video_transfer_characteristic_system_equal(const VideoTransferCharacteristicSystem &lhs,
                                                                     const VideoTransferCharacteristicSystem &rhs) {
    return lhs.known == rhs.known && lhs.raw_token == rhs.raw_token;
}

[[nodiscard]] inline bool video_signal_standard_equal(const VideoSignalStandard &lhs, const VideoSignalStandard &rhs) {
    return lhs.known == rhs.known && lhs.raw_token == rhs.raw_token;
}

[[nodiscard]] inline bool video_range_equal(const VideoRange &lhs, const VideoRange &rhs) {
    return lhs.known == rhs.known && lhs.raw_token == rhs.raw_token;
}

[[nodiscard]] inline bool
video_optional_transfer_characteristic_system_equal(const std::optional<VideoTransferCharacteristicSystem> &lhs,
                                                    const std::optional<VideoTransferCharacteristicSystem> &rhs) {
    if (lhs.has_value() != rhs.has_value()) {
        return false;
    }

    return !lhs.has_value() || video_transfer_characteristic_system_equal(*lhs, *rhs);
}

[[nodiscard]] inline bool video_optional_signal_standard_equal(const std::optional<VideoSignalStandard> &lhs,
                                                               const std::optional<VideoSignalStandard> &rhs) {
    if (lhs.has_value() != rhs.has_value()) {
        return false;
    }

    return !lhs.has_value() || video_signal_standard_equal(*lhs, *rhs);
}

[[nodiscard]] inline bool video_optional_range_equal(const std::optional<VideoRange> &lhs,
                                                     const std::optional<VideoRange> &rhs) {
    if (lhs.has_value() != rhs.has_value()) {
        return false;
    }

    return !lhs.has_value() || video_range_equal(*lhs, *rhs);
}

[[nodiscard]] inline bool video_pixel_aspect_ratio_equal(const VideoPixelAspectRatio &lhs,
                                                         const VideoPixelAspectRatio &rhs) {
    return lhs.width == rhs.width && lhs.height == rhs.height;
}

[[nodiscard]] inline bool video_media_description_equal(const VideoMediaDescription &lhs,
                                                        const VideoMediaDescription &rhs) {
    return video_sampling_equal(lhs.sampling, rhs.sampling) && lhs.width == rhs.width && lhs.height == rhs.height &&
           lhs.fps_num == rhs.fps_num && lhs.fps_den == rhs.fps_den && video_bit_depth_equal(lhs.depth, rhs.depth) &&
           video_colorimetry_equal(lhs.colorimetry, rhs.colorimetry) &&
           video_optional_transfer_characteristic_system_equal(lhs.transfer_characteristic_system,
                                                               rhs.transfer_characteristic_system) &&
           video_optional_signal_standard_equal(lhs.signal_standard, rhs.signal_standard) &&
           video_optional_range_equal(lhs.range, rhs.range) &&
           video_pixel_aspect_ratio_equal(lhs.pixel_aspect_ratio, rhs.pixel_aspect_ratio);
}

[[nodiscard]] inline Error validate_video_transport_payload_format(VideoTransportPayloadFormat format) noexcept {
    switch (format) {
    case VideoTransportPayloadFormat::Rfc4175:
    case VideoTransportPayloadFormat::Rfc4175Ycbcr422_8Bit:
    case VideoTransportPayloadFormat::Rfc4175Ycbcr422_10Bit:
    case VideoTransportPayloadFormat::Rfc4175Ycbcr422_12Bit:
    case VideoTransportPayloadFormat::Rfc4175Ycbcr422_16Bit:
    case VideoTransportPayloadFormat::Rfc4175Ycbcr420_8Bit:
    case VideoTransportPayloadFormat::Rfc4175Ycbcr420_10Bit:
    case VideoTransportPayloadFormat::Rfc4175Ycbcr420_12Bit:
    case VideoTransportPayloadFormat::Rfc4175Ycbcr420_16Bit:
    case VideoTransportPayloadFormat::Rfc4175Rgb_8Bit:
    case VideoTransportPayloadFormat::Rfc4175Rgb_10Bit:
    case VideoTransportPayloadFormat::Rfc4175Rgb_12Bit:
    case VideoTransportPayloadFormat::Rfc4175Rgb_16Bit:
    case VideoTransportPayloadFormat::Rfc4175Ycbcr444_8Bit:
    case VideoTransportPayloadFormat::Rfc4175Ycbcr444_10Bit:
    case VideoTransportPayloadFormat::Rfc4175Ycbcr444_12Bit:
    case VideoTransportPayloadFormat::Rfc4175Ycbcr444_16Bit:
    case VideoTransportPayloadFormat::CustomYcbcr422Planar10Le:
    case VideoTransportPayloadFormat::CustomV210:
        return Error::Ok;
    default:
        return Error::InvalidValue;
    }
}

[[nodiscard]] inline Error validate_video_frame_handoff_format(VideoFrameHandoffFormat format) noexcept {
    switch (format) {
    case VideoFrameHandoffFormat::Uyvy:
    case VideoFrameHandoffFormat::Yuv422Planar8:
    case VideoFrameHandoffFormat::Yuv422Planar10Le:
    case VideoFrameHandoffFormat::Yuv422Planar12Le:
    case VideoFrameHandoffFormat::Yuv422Planar16Le:
    case VideoFrameHandoffFormat::V210:
    case VideoFrameHandoffFormat::Y210:
    case VideoFrameHandoffFormat::Yuv422Rfc4175Pgroup2Be10:
    case VideoFrameHandoffFormat::Yuv422Rfc4175Pgroup2Be12:
    case VideoFrameHandoffFormat::Yuv444Planar10Le:
    case VideoFrameHandoffFormat::Yuv444Planar12Le:
    case VideoFrameHandoffFormat::Yuv444Rfc4175Pgroup4Be10:
    case VideoFrameHandoffFormat::Yuv444Rfc4175Pgroup2Be12:
    case VideoFrameHandoffFormat::Yuv420Planar8:
    case VideoFrameHandoffFormat::Argb:
    case VideoFrameHandoffFormat::Bgra:
    case VideoFrameHandoffFormat::Rgb8:
    case VideoFrameHandoffFormat::GbrPlanar10Le:
    case VideoFrameHandoffFormat::GbrPlanar12Le:
    case VideoFrameHandoffFormat::RgbRfc4175Pgroup4Be10:
    case VideoFrameHandoffFormat::RgbRfc4175Pgroup2Be12:
        return Error::Ok;
    default:
        return Error::InvalidValue;
    }
}

[[nodiscard]] inline Error validate_video_receive_rtp_clock(const VideoReceiveRtpClock &clock) noexcept {
    if (clock.rtp_clock_rate == 0) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

[[nodiscard]] inline Error validate_video_receive_topology(const VideoReceiveTopology &topology) noexcept {
    switch (topology.kind) {
    case VideoReceiveTopologyKind::SingleStream:
        if (topology.stream_count != 1) {
            return Error::InvalidValue;
        }
        if (topology.redundant_mid.has_value()) {
            return Error::InvalidValue;
        }
        return Error::Ok;

    case VideoReceiveTopologyKind::RedundantStreams:
        if (topology.stream_count < 2) {
            return Error::InvalidValue;
        }
        if (topology.primary_mid.has_value() && topology.redundant_mid.has_value() &&
            *topology.primary_mid == *topology.redundant_mid) {
            return Error::InvalidValue;
        }
        return Error::Ok;

    default:
        return Error::InvalidValue;
    }
}

[[nodiscard]] inline std::expected<VideoTransportPayloadFormat, Error>
video_transport_payload_format_from_media_description(const VideoMediaDescription &media) {
    if (Error err = validate_video_media_description_structure(media); err != Error::Ok) {
        return std::unexpected(err);
    }

    if (media.depth.floating_point) {
        switch (media.sampling.known) {
        case VideoSampling::Known::YCbCr422:
        case VideoSampling::Known::YCbCr444:
        case VideoSampling::Known::YCbCr420:
        case VideoSampling::Known::RGB:
        case VideoSampling::Known::XYZ:
        case VideoSampling::Known::Key:
        case VideoSampling::Known::CLYCbCr444:
        case VideoSampling::Known::CLYCbCr422:
        case VideoSampling::Known::CLYCbCr420:
        case VideoSampling::Known::ICtCp444:
        case VideoSampling::Known::ICtCp422:
        case VideoSampling::Known::ICtCp420:
            return VideoTransportPayloadFormat::Rfc4175;

        case VideoSampling::Known::Other:
            return std::unexpected(Error::Unsupported);

        default:
            return std::unexpected(Error::InvalidValue);
        }
    }

    switch (media.sampling.known) {
    case VideoSampling::Known::YCbCr422:
        switch (media.depth.bits) {
        case 8:
            return VideoTransportPayloadFormat::Rfc4175Ycbcr422_8Bit;
        case 10:
            return VideoTransportPayloadFormat::Rfc4175Ycbcr422_10Bit;
        case 12:
            return VideoTransportPayloadFormat::Rfc4175Ycbcr422_12Bit;
        case 16:
            return VideoTransportPayloadFormat::Rfc4175Ycbcr422_16Bit;
        default:
            return std::unexpected(Error::InvalidValue);
        }

    case VideoSampling::Known::YCbCr420:
        switch (media.depth.bits) {
        case 8:
            return VideoTransportPayloadFormat::Rfc4175Ycbcr420_8Bit;
        case 10:
            return VideoTransportPayloadFormat::Rfc4175Ycbcr420_10Bit;
        case 12:
            return VideoTransportPayloadFormat::Rfc4175Ycbcr420_12Bit;
        case 16:
            return VideoTransportPayloadFormat::Rfc4175Ycbcr420_16Bit;
        default:
            return std::unexpected(Error::InvalidValue);
        }

    case VideoSampling::Known::RGB:
        switch (media.depth.bits) {
        case 8:
            return VideoTransportPayloadFormat::Rfc4175Rgb_8Bit;
        case 10:
            return VideoTransportPayloadFormat::Rfc4175Rgb_10Bit;
        case 12:
            return VideoTransportPayloadFormat::Rfc4175Rgb_12Bit;
        case 16:
            return VideoTransportPayloadFormat::Rfc4175Rgb_16Bit;
        default:
            return std::unexpected(Error::InvalidValue);
        }

    case VideoSampling::Known::YCbCr444:
        switch (media.depth.bits) {
        case 8:
            return VideoTransportPayloadFormat::Rfc4175Ycbcr444_8Bit;
        case 10:
            return VideoTransportPayloadFormat::Rfc4175Ycbcr444_10Bit;
        case 12:
            return VideoTransportPayloadFormat::Rfc4175Ycbcr444_12Bit;
        case 16:
            return VideoTransportPayloadFormat::Rfc4175Ycbcr444_16Bit;
        default:
            return std::unexpected(Error::InvalidValue);
        }

    case VideoSampling::Known::XYZ:
    case VideoSampling::Known::Key:
    case VideoSampling::Known::CLYCbCr444:
    case VideoSampling::Known::CLYCbCr422:
    case VideoSampling::Known::CLYCbCr420:
    case VideoSampling::Known::ICtCp444:
    case VideoSampling::Known::ICtCp422:
    case VideoSampling::Known::ICtCp420:
        return VideoTransportPayloadFormat::Rfc4175;

    case VideoSampling::Known::Other:
        return std::unexpected(Error::Unsupported);

    default:
        return std::unexpected(Error::InvalidValue);
    }
}

[[nodiscard]] inline bool video_media_description_is_ycbcr422_integer_depth(const VideoMediaDescription &media,
                                                                            std::uint8_t bits) {
    return media.sampling.known == VideoSampling::Known::YCbCr422 && !media.depth.floating_point &&
           media.depth.bits == bits;
}

[[nodiscard]] inline Error
validate_video_transport_payload_format_matches_media_description(VideoTransportPayloadFormat format,
                                                                  const VideoMediaDescription &media) {
    if (Error err = validate_video_transport_payload_format(format); err != Error::Ok) {
        return err;
    }

    if (Error err = validate_video_media_description_structure(media); err != Error::Ok) {
        return err;
    }

    switch (format) {
    case VideoTransportPayloadFormat::Rfc4175: {
        /*
         * Generic RFC 4175 / ST 2110-20 transport marker.
         * It is valid for known media shapes that can be projected onto RFC 4175 transport.
         */
        auto projected_format = video_transport_payload_format_from_media_description(media);
        if (!projected_format.has_value()) {
            return projected_format.error();
        }

        return Error::Ok;
    }

    case VideoTransportPayloadFormat::CustomYcbcr422Planar10Le:
    case VideoTransportPayloadFormat::CustomV210:
        return video_media_description_is_ycbcr422_integer_depth(media, 10) ? Error::Ok : Error::InvalidValue;

    default:
        break;
    }

    auto expected_format = video_transport_payload_format_from_media_description(media);
    if (!expected_format.has_value()) {
        return expected_format.error();
    }

    if (format != *expected_format) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

[[nodiscard]] inline bool video_media_description_is_integer_sampling_depth(const VideoMediaDescription &media,
                                                                            VideoSampling::Known sampling,
                                                                            std::uint8_t bits) {
    return media.sampling.known == sampling && !media.depth.floating_point && media.depth.bits == bits;
}

[[nodiscard]] inline Error
validate_video_frame_handoff_format_matches_media_description(VideoFrameHandoffFormat handoff_format,
                                                              const VideoMediaDescription &media) {
    if (Error err = validate_video_frame_handoff_format(handoff_format); err != Error::Ok) {
        return err;
    }

    if (Error err = validate_video_media_description_structure(media); err != Error::Ok) {
        return err;
    }

    switch (handoff_format) {
    case VideoFrameHandoffFormat::Uyvy:
    case VideoFrameHandoffFormat::Yuv422Planar8:
        return video_media_description_is_integer_sampling_depth(media, VideoSampling::Known::YCbCr422, 8)
                   ? Error::Ok
                   : Error::InvalidValue;

    case VideoFrameHandoffFormat::Yuv422Planar10Le:
    case VideoFrameHandoffFormat::V210:
    case VideoFrameHandoffFormat::Y210:
    case VideoFrameHandoffFormat::Yuv422Rfc4175Pgroup2Be10:
        return video_media_description_is_integer_sampling_depth(media, VideoSampling::Known::YCbCr422, 10)
                   ? Error::Ok
                   : Error::InvalidValue;

    case VideoFrameHandoffFormat::Yuv422Planar12Le:
    case VideoFrameHandoffFormat::Yuv422Rfc4175Pgroup2Be12:
        return video_media_description_is_integer_sampling_depth(media, VideoSampling::Known::YCbCr422, 12)
                   ? Error::Ok
                   : Error::InvalidValue;

    case VideoFrameHandoffFormat::Yuv422Planar16Le:
        return video_media_description_is_integer_sampling_depth(media, VideoSampling::Known::YCbCr422, 16)
                   ? Error::Ok
                   : Error::InvalidValue;

    case VideoFrameHandoffFormat::Yuv444Planar10Le:
    case VideoFrameHandoffFormat::Yuv444Rfc4175Pgroup4Be10:
        return video_media_description_is_integer_sampling_depth(media, VideoSampling::Known::YCbCr444, 10)
                   ? Error::Ok
                   : Error::InvalidValue;

    case VideoFrameHandoffFormat::Yuv444Planar12Le:
    case VideoFrameHandoffFormat::Yuv444Rfc4175Pgroup2Be12:
        return video_media_description_is_integer_sampling_depth(media, VideoSampling::Known::YCbCr444, 12)
                   ? Error::Ok
                   : Error::InvalidValue;

    case VideoFrameHandoffFormat::Yuv420Planar8:
        return video_media_description_is_integer_sampling_depth(media, VideoSampling::Known::YCbCr420, 8)
                   ? Error::Ok
                   : Error::InvalidValue;

    case VideoFrameHandoffFormat::Argb:
    case VideoFrameHandoffFormat::Bgra:
    case VideoFrameHandoffFormat::Rgb8:
        return video_media_description_is_integer_sampling_depth(media, VideoSampling::Known::RGB, 8)
                   ? Error::Ok
                   : Error::InvalidValue;

    case VideoFrameHandoffFormat::GbrPlanar10Le:
    case VideoFrameHandoffFormat::RgbRfc4175Pgroup4Be10:
        return video_media_description_is_integer_sampling_depth(media, VideoSampling::Known::RGB, 10)
                   ? Error::Ok
                   : Error::InvalidValue;

    case VideoFrameHandoffFormat::GbrPlanar12Le:
    case VideoFrameHandoffFormat::RgbRfc4175Pgroup2Be12:
        return video_media_description_is_integer_sampling_depth(media, VideoSampling::Known::RGB, 12)
                   ? Error::Ok
                   : Error::InvalidValue;

    default:
        return Error::InvalidValue;
    }
}

[[nodiscard]] inline Error
validate_video_receive_capability_structure(const VideoReceiveCapability &capability) noexcept {
    if (Error err = validate_video_media_description_structure(capability.media); err != Error::Ok) {
        return err;
    }

    if (Error err = config_validation::validate_video_scan_mode(capability.scan_mode); err != Error::Ok) {
        return err;
    }

    if (Error err = validate_video_media_description_cross_field_constraints(capability.media, capability.scan_mode);
        err != Error::Ok) {
        return err;
    }

    if (Error err = validate_video_packing_mode(capability.packing_mode); err != Error::Ok) {
        return err;
    }

    if (Error err = validate_video_frame_handoff_format(capability.handoff_format); err != Error::Ok) {
        return err;
    }

    if (Error err = validate_video_receive_rtp_clock(capability.rtp_clock); err != Error::Ok) {
        return err;
    }

    if (Error err = validate_video_receive_topology(capability.topology); err != Error::Ok) {
        return err;
    }

    if (Error err = validate_video_transport_payload_format_matches_media_description(capability.transport_format,
                                                                                      capability.media);
        err != Error::Ok) {
        return err;
    }

    return Error::Ok;
}

[[nodiscard]] inline std::expected<VideoFrameHandoffFormat, Error>
video_frame_handoff_format_from_project_pixel_format(PixelFormat format) {
    switch (format) {
    case PixelFormat::UYVY:
        return VideoFrameHandoffFormat::Uyvy;
    default:
        return std::unexpected(Error::InvalidValue);
    }
}

[[nodiscard]] inline std::expected<PixelFormat, Error>
project_pixel_format_from_video_frame_handoff_format(VideoFrameHandoffFormat format) {
    if (Error err = validate_video_frame_handoff_format(format); err != Error::Ok) {
        return std::unexpected(err);
    }

    switch (format) {
    case VideoFrameHandoffFormat::Uyvy:
        return PixelFormat::UYVY;
    default:
        return std::unexpected(Error::Unsupported);
    }
}

[[nodiscard]] inline bool
video_frame_handoff_format_maps_to_project_pixel_format(VideoFrameHandoffFormat format) noexcept {
    return project_pixel_format_from_video_frame_handoff_format(format).has_value();
}

[[nodiscard]] inline Error validate_project_video_frame_handoff_format_support(VideoFrameHandoffFormat handoff_format) {
    auto project_format = project_pixel_format_from_video_frame_handoff_format(handoff_format);
    if (!project_format.has_value()) {
        return project_format.error();
    }

    return Error::Ok;
}

[[nodiscard]] inline Error
validate_project_video_frame_handoff_format_matches_pixel_format(VideoFrameHandoffFormat handoff_format,
                                                                 PixelFormat pixel_format) {
    auto project_format = project_pixel_format_from_video_frame_handoff_format(handoff_format);
    if (!project_format.has_value()) {
        return project_format.error();
    }

    if (pixel_format != *project_format) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

[[nodiscard]] inline Error validate_project_video_frame_storage_compatibility(const VideoReceiveCapability &capability,
                                                                              PixelFormat pixel_format) {
    if (Error err = validate_video_receive_capability_structure(capability); err != Error::Ok) {
        return err;
    }

    switch (pixel_format) {
    case PixelFormat::UYVY:
        /*
         * Current project storage path:
         * - supports only integer YCbCr 4:2:2 8-bit in UYVY storage;
         * - structurally valid but non-deliverable media must return Unsupported;
         * - malformed/contradictory capability values must already have been rejected
         *   by validate_video_receive_capability_structure(...).
         */
        if (capability.media.depth.floating_point) {
            return Error::Unsupported;
        }

        switch (capability.media.sampling.known) {
        case VideoSampling::Known::YCbCr422:
            switch (capability.media.depth.bits) {
            case 8:
                break;
            case 10:
            case 12:
            case 16:
                return Error::Unsupported;
            default:
                return Error::InvalidValue;
            }
            break;

        case VideoSampling::Known::YCbCr444:
        case VideoSampling::Known::YCbCr420:
        case VideoSampling::Known::RGB:
        case VideoSampling::Known::XYZ:
        case VideoSampling::Known::Key:
        case VideoSampling::Known::CLYCbCr444:
        case VideoSampling::Known::CLYCbCr422:
        case VideoSampling::Known::CLYCbCr420:
        case VideoSampling::Known::ICtCp444:
        case VideoSampling::Known::ICtCp422:
        case VideoSampling::Known::ICtCp420:
        case VideoSampling::Known::Other:
            return Error::Unsupported;

        default:
            return Error::InvalidValue;
        }

        return config_validation::validate_video_format_constraints(pixel_format, capability.media.width,
                                                                    capability.media.height);

    default:
        return Error::InvalidValue;
    }
}

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_VIDEO_RECEIVE_CAPABILITY_HPP
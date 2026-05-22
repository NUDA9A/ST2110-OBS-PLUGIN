#ifndef ST2110_OBS_VIDEO_MEDIA_TYPES_HPP
#define ST2110_OBS_VIDEO_MEDIA_TYPES_HPP

#include <st2110/foundation/error.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace st2110 {
enum class VideoPackingMode { Gpm, Bpm };

enum class VideoScanMode { Progressive, Interlaced, PsF };

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

    Known known = Known::Other;
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
    VideoSignalStandard signal_standard{};
    std::optional<VideoRange> range{};
    VideoPixelAspectRatio pixel_aspect_ratio{};
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

[[nodiscard]] inline Error validate_video_media_description_dimensions(const std::uint32_t width,
                                                                       const std::uint32_t height) {
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

[[nodiscard]] inline bool video_bit_depth_is_integer(const VideoBitDepth &depth, const std::uint8_t bits) {
    return depth.bits == bits && !depth.floating_point;
}

[[nodiscard]] inline bool video_bit_depth_is_16f(const VideoBitDepth &depth) {
    return depth.bits == 16 && depth.floating_point;
}

[[nodiscard]] inline bool video_bit_depth_is_standard_8_10_12_16_or_16f(const VideoBitDepth &depth) {
    return video_bit_depth_is_integer(depth, 8) || video_bit_depth_is_integer(depth, 10) ||
           video_bit_depth_is_integer(depth, 12) || video_bit_depth_is_integer(depth, 16) ||
           video_bit_depth_is_16f(depth);
}

[[nodiscard]] inline Error validate_video_sampling_depth_combination(const VideoSampling &sampling,
                                                                     const VideoBitDepth &depth) {
    switch (sampling.known) {
    case VideoSampling::Known::YCbCr422:
    case VideoSampling::Known::YCbCr444:
    case VideoSampling::Known::RGB:
    case VideoSampling::Known::Key:
    case VideoSampling::Known::CLYCbCr444:
    case VideoSampling::Known::CLYCbCr422:
    case VideoSampling::Known::ICtCp444:
    case VideoSampling::Known::ICtCp422:
        return video_bit_depth_is_standard_8_10_12_16_or_16f(depth) ? Error::Ok : Error::InvalidValue;

    case VideoSampling::Known::YCbCr420:
    case VideoSampling::Known::CLYCbCr420:
    case VideoSampling::Known::ICtCp420:
        if (video_bit_depth_is_integer(depth, 8) || video_bit_depth_is_integer(depth, 10) ||
            video_bit_depth_is_integer(depth, 12)) {
            return Error::Ok;
        }
        return Error::InvalidValue;

    case VideoSampling::Known::XYZ:
        if (video_bit_depth_is_integer(depth, 12) || video_bit_depth_is_integer(depth, 16) ||
            video_bit_depth_is_16f(depth)) {
            return Error::Ok;
        }
        return Error::InvalidValue;

    case VideoSampling::Known::Other:
        return Error::Ok;

    default:
        return Error::InvalidValue;
    }
}

[[nodiscard]] inline Error
validate_video_tcs_depth_combination(const std::optional<VideoTransferCharacteristicSystem> &tcs,
                                     const VideoBitDepth &depth) {
    if (!tcs.has_value()) {
        return Error::Ok;
    }

    switch (tcs->known) {
    case VideoTransferCharacteristicSystem::Known::Linear:
    case VideoTransferCharacteristicSystem::Known::Bt2100LinPq:
    case VideoTransferCharacteristicSystem::Known::Bt2100LinHlg:
    case VideoTransferCharacteristicSystem::Known::St2065_1:
        return video_bit_depth_is_16f(depth) ? Error::Ok : Error::InvalidValue;

    case VideoTransferCharacteristicSystem::Known::SDR:
    case VideoTransferCharacteristicSystem::Known::PQ:
    case VideoTransferCharacteristicSystem::Known::HLG:
    case VideoTransferCharacteristicSystem::Known::St428_1:
    case VideoTransferCharacteristicSystem::Known::Density:
    case VideoTransferCharacteristicSystem::Known::St2115LogS3:
    case VideoTransferCharacteristicSystem::Known::Unspecified:
    case VideoTransferCharacteristicSystem::Known::Other:
        return Error::Ok;

    default:
        return Error::InvalidValue;
    }
}

[[nodiscard]] inline Error validate_video_media_description_structure(const VideoMediaDescription &media) {
    if (const Error err = validate_video_sampling(media.sampling); err != Error::Ok) {
        return err;
    }

    if (const Error err = validate_video_bit_depth(media.depth); err != Error::Ok) {
        return err;
    }

    if (const Error err = validate_video_colorimetry(media.colorimetry); err != Error::Ok) {
        return err;
    }

    if (media.transfer_characteristic_system.has_value()) {
        if (const Error err = validate_video_transfer_characteristic_system(*media.transfer_characteristic_system);
            err != Error::Ok) {
            return err;
        }
    }

    if (const Error err = validate_video_signal_standard(media.signal_standard); err != Error::Ok) {
        return err;
    }

    if (media.range.has_value()) {
        if (const Error err = validate_video_range(*media.range); err != Error::Ok) {
            return err;
        }
    }

    if (media.pixel_aspect_ratio.height == 0 || media.pixel_aspect_ratio.width == 0) {
        return Error::InvalidValue;
    }

    if (const Error err = validate_video_media_description_dimensions(media.width, media.height); err != Error::Ok) {
        return err;
    }

    if (media.fps_den == 0 || media.fps_num == 0) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

[[nodiscard]] inline Error validate_video_media_description_cross_field_constraints(const VideoMediaDescription &media,
                                                                                    const VideoScanMode scan_mode) {
    if (const Error err = validate_video_media_description_structure(media); err != Error::Ok) {
        return err;
    }

    if (const Error err = validate_video_sampling_depth_combination(media.sampling, media.depth); err != Error::Ok) {
        return err;
    }

    if (const Error err = validate_video_tcs_depth_combination(media.transfer_characteristic_system, media.depth);
        err != Error::Ok) {
        return err;
    }

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
    } else if (media.colorimetry.known == VideoColorimetry::Known::Alpha) {
        return Error::InvalidValue;
    }

    const bool uses_alpha_colorimetry = (media.colorimetry.known == VideoColorimetry::Known::Alpha);
    const bool uses_st2115logs3_tcs =
        media.transfer_characteristic_system.has_value() &&
        media.transfer_characteristic_system->known == VideoTransferCharacteristicSystem::Known::St2115LogS3;

    const bool requires_st2110_20_2022 = uses_alpha_colorimetry || uses_st2115logs3_tcs;

    switch (media.signal_standard.known) {
    case VideoSignalStandard::Known::Other:
        break;

    case VideoSignalStandard::Known::St2110_20_2017:
        if (requires_st2110_20_2022) {
            return Error::InvalidValue;
        }
        break;

    case VideoSignalStandard::Known::St2110_20_2022:
        break;

    default:
        return Error::InvalidValue;
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
} // namespace st2110

#endif // ST2110_OBS_VIDEO_MEDIA_TYPES_HPP

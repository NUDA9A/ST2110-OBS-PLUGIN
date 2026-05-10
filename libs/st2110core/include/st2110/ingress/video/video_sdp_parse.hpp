#ifndef ST2110_OBS_PLUGIN_VIDEO_SDP_PARSE_HPP
#define ST2110_OBS_PLUGIN_VIDEO_SDP_PARSE_HPP

#include "st2110/foundation/error.hpp"
#include "st2110/ingress/shared/sdp_common.hpp"
#include "st2110/model/video/video_signaling_types.hpp"

#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace st2110 {

struct RawVideoSdpParseExactFrameRate {
    std::uint32_t numerator = 0;
    std::uint32_t denominator = 1;
};

struct RawVideoSdpParsePixelAspectRatio {
    std::uint32_t width = 1;
    std::uint32_t height = 1;
};

struct RawVideoSdpParseRtpMap {
    std::uint8_t payload_type = 0;
    std::string encoding_name{};
    std::uint32_t clock_rate = 0;
    std::optional<std::string> encoding_parameters{};
};

[[nodiscard]] inline std::expected<std::vector<std::uint8_t>, Error>
parse_video_media_line_payload_types(std::string_view media_value) {
    media_value = trim_ws(strip_cr(media_value));

    const auto tokens = split_ws(media_value);
    if (tokens.size() < 4) {
        return std::unexpected(Error::InvalidValue);
    }

    if (tokens[0] != "video") {
        return std::unexpected(Error::InvalidValue);
    }

    std::vector<std::uint8_t> payload_types{};
    payload_types.reserve(tokens.size() - 3);

    for (std::size_t i = 3; i < tokens.size(); ++i) {
        const auto payload_type = parse_payload_type(tokens[i]);
        if (!payload_type.has_value()) {
            return std::unexpected(Error::InvalidValue);
        }

        payload_types.push_back(*payload_type);
    }

    return payload_types;
}

[[nodiscard]] inline bool video_media_line_contains_payload_type(const std::vector<std::uint8_t> &payload_types,
                                                                 const std::uint8_t expected_payload_type) {
    for (const std::uint8_t payload_type : payload_types) {
        if (payload_type == expected_payload_type) {
            return true;
        }
    }

    return false;
}

[[nodiscard]] inline std::expected<const RawSdpMediaSectionLines *, Error>
select_raw_video_sdp_media_section(const RawSdpDocument &raw_sdp, const std::uint8_t expected_payload_type) {
    for (const RawSdpMediaSectionLines &media : raw_sdp.media_sections) {
        auto payload_types = parse_video_media_line_payload_types(media.media_value);
        if (!payload_types.has_value()) {
            continue;
        }

        if (video_media_line_contains_payload_type(*payload_types, expected_payload_type)) {
            return &media;
        }
    }

    return std::unexpected(Error::InvalidValue);
}

[[nodiscard]] inline std::expected<RawVideoSdpParseRtpMap, Error>
parse_video_sdp_parse_rtpmap_payload(std::string_view raw_rtpmap) {
    raw_rtpmap = trim_ws(strip_cr(raw_rtpmap));
    if (raw_rtpmap.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    const std::size_t pt_end = raw_rtpmap.find_first_of(" \t");
    if (pt_end == std::string_view::npos) {
        return std::unexpected(Error::InvalidValue);
    }

    const std::string_view pt_text = raw_rtpmap.substr(0, pt_end);
    const auto payload_type = parse_payload_type(pt_text);
    if (!payload_type.has_value()) {
        return std::unexpected(Error::InvalidValue);
    }

    std::string_view payload = raw_rtpmap.substr(pt_end);
    payload = trim_left_ws(payload);
    if (payload.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    const std::size_t first_slash = payload.find('/');
    if (first_slash == std::string_view::npos || first_slash == 0) {
        return std::unexpected(Error::InvalidValue);
    }

    RawVideoSdpParseRtpMap out{};
    out.payload_type = *payload_type;
    out.encoding_name = std::string(payload.substr(0, first_slash));

    payload.remove_prefix(first_slash + 1);
    if (payload.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    const std::size_t second_slash = payload.find('/');
    const std::string_view clock_rate_text =
        second_slash == std::string_view::npos ? payload : payload.substr(0, second_slash);

    auto parsed_clock_rate = parse_sdp_numeric_value<std::uint32_t>(clock_rate_text);
    if (!parsed_clock_rate.has_value() || *parsed_clock_rate == 0) {
        return std::unexpected(Error::InvalidValue);
    }

    out.clock_rate = *parsed_clock_rate;

    if (second_slash != std::string_view::npos) {
        payload.remove_prefix(second_slash + 1);
        if (payload.empty() || payload.find('/') != std::string_view::npos) {
            return std::unexpected(Error::InvalidValue);
        }

        out.encoding_parameters = std::string(payload);
    }

    return out;
}

struct RawVideoSdpParseFmtpToken {
    std::string_view name{};
    std::optional<std::string_view> value{};
};

[[nodiscard]] inline std::expected<RawVideoSdpParseFmtpToken, Error>
parse_video_sdp_parse_fmtp_token(std::string_view parameter) {
    parameter = trim_ws(parameter);
    if (parameter.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    const std::size_t eq_pos = parameter.find('=');
    if (eq_pos == std::string_view::npos) {
        return RawVideoSdpParseFmtpToken{
            .name = parameter,
            .value = std::nullopt,
        };
    }

    const std::string_view name = trim_ws(parameter.substr(0, eq_pos));
    const std::string_view value = trim_ws(parameter.substr(eq_pos + 1));

    if (name.empty() || value.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    return RawVideoSdpParseFmtpToken{
        .name = name,
        .value = value,
    };
}

[[nodiscard]] inline std::expected<std::uint32_t, Error> parse_video_sdp_parse_positive_u32(std::string_view text) {
    auto parsed = parse_sdp_numeric_value<std::uint32_t>(trim_ws(text));
    if (!parsed.has_value() || *parsed == 0) {
        return std::unexpected(Error::InvalidValue);
    }

    return *parsed;
}

[[nodiscard]] inline std::expected<RawVideoSdpParseExactFrameRate, Error>
parse_video_sdp_parse_exactframerate(std::string_view text) {
    text = trim_ws(text);
    if (text.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    const std::size_t slash_pos = text.find('/');
    if (slash_pos == std::string_view::npos) {
        auto numerator = parse_video_sdp_parse_positive_u32(text);
        if (!numerator.has_value()) {
            return std::unexpected(numerator.error());
        }

        return RawVideoSdpParseExactFrameRate{
            .numerator = *numerator,
            .denominator = 1,
        };
    }

    const std::string_view numerator_text = text.substr(0, slash_pos);
    const std::string_view denominator_text = text.substr(slash_pos + 1);

    auto numerator = parse_video_sdp_parse_positive_u32(numerator_text);
    if (!numerator.has_value()) {
        return std::unexpected(numerator.error());
    }

    auto denominator = parse_video_sdp_parse_positive_u32(denominator_text);
    if (!denominator.has_value()) {
        return std::unexpected(denominator.error());
    }

    return RawVideoSdpParseExactFrameRate{
        .numerator = *numerator,
        .denominator = *denominator,
    };
}

[[nodiscard]] inline std::expected<RawVideoSdpParsePixelAspectRatio, Error>
parse_video_sdp_parse_pixel_aspect_ratio(std::string_view text) {
    text = trim_ws(text);
    if (text.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    const std::size_t colon_pos = text.find(':');
    if (colon_pos == std::string_view::npos) {
        return std::unexpected(Error::InvalidValue);
    }

    auto width = parse_video_sdp_parse_positive_u32(text.substr(0, colon_pos));
    if (!width.has_value()) {
        return std::unexpected(width.error());
    }

    auto height = parse_video_sdp_parse_positive_u32(text.substr(colon_pos + 1));
    if (!height.has_value()) {
        return std::unexpected(height.error());
    }

    return RawVideoSdpParsePixelAspectRatio{
        .width = *width,
        .height = *height,
    };
}

[[nodiscard]] inline std::expected<std::pair<std::uint16_t, bool>, Error>
parse_video_sdp_parse_depth(std::string_view text) {
    text = trim_ws(text);
    if (text.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    bool floating_point = false;
    if (text.back() == 'f') {
        floating_point = true;
        text.remove_suffix(1);
        if (text.empty()) {
            return std::unexpected(Error::InvalidValue);
        }
    }

    auto parsed = parse_sdp_numeric_value<std::uint16_t>(text);
    if (!parsed.has_value() || *parsed == 0) {
        return std::unexpected(Error::InvalidValue);
    }

    return std::pair<std::uint16_t, bool>{*parsed, floating_point};
}

[[nodiscard]] inline VideoSampling video_sampling_from_raw_value(std::string_view raw_value) {
    VideoSampling sampling{};

    if (raw_value == "YCbCr-4:2:2") {
        sampling.known = VideoSampling::Known::YCbCr422;
    } else if (raw_value == "YCbCr-4:4:4") {
        sampling.known = VideoSampling::Known::YCbCr444;
    } else if (raw_value == "YCbCr-4:2:0") {
        sampling.known = VideoSampling::Known::YCbCr420;
    } else if (raw_value == "RGB") {
        sampling.known = VideoSampling::Known::RGB;
    } else if (raw_value == "XYZ") {
        sampling.known = VideoSampling::Known::XYZ;
    } else if (raw_value == "KEY") {
        sampling.known = VideoSampling::Known::Key;
    } else if (raw_value == "CLYCbCr-4:4:4") {
        sampling.known = VideoSampling::Known::CLYCbCr444;
    } else if (raw_value == "CLYCbCr-4:2:2") {
        sampling.known = VideoSampling::Known::CLYCbCr422;
    } else if (raw_value == "CLYCbCr-4:2:0") {
        sampling.known = VideoSampling::Known::CLYCbCr420;
    } else if (raw_value == "ICtCp-4:4:4") {
        sampling.known = VideoSampling::Known::ICtCp444;
    } else if (raw_value == "ICtCp-4:2:2") {
        sampling.known = VideoSampling::Known::ICtCp422;
    } else if (raw_value == "ICtCp-4:2:0") {
        sampling.known = VideoSampling::Known::ICtCp420;
    } else {
        sampling.known = VideoSampling::Known::Other;
        sampling.raw_token = std::string(raw_value);
    }

    return sampling;
}

[[nodiscard]] inline VideoColorimetry video_colorimetry_from_raw_value(std::string_view raw_value) {
    VideoColorimetry colorimetry{};

    if (raw_value == "BT601") {
        colorimetry.known = VideoColorimetry::Known::Bt601;
    } else if (raw_value == "BT709") {
        colorimetry.known = VideoColorimetry::Known::Bt709;
    } else if (raw_value == "BT2020") {
        colorimetry.known = VideoColorimetry::Known::Bt2020;
    } else if (raw_value == "BT2100") {
        colorimetry.known = VideoColorimetry::Known::Bt2100;
    } else if (raw_value == "ST2065-1") {
        colorimetry.known = VideoColorimetry::Known::St2065_1;
    } else if (raw_value == "ST2065-3") {
        colorimetry.known = VideoColorimetry::Known::St2065_3;
    } else if (raw_value == "UNSPECIFIED") {
        colorimetry.known = VideoColorimetry::Known::Unspecified;
    } else if (raw_value == "XYZ") {
        colorimetry.known = VideoColorimetry::Known::Xyz;
    } else if (raw_value == "ALPHA") {
        colorimetry.known = VideoColorimetry::Known::Alpha;
    } else {
        colorimetry.known = VideoColorimetry::Known::Other;
        colorimetry.raw_token = std::string(raw_value);
    }

    return colorimetry;
}

[[nodiscard]] inline VideoTransferCharacteristicSystem video_tcs_from_raw_value(std::string_view raw_value) {
    VideoTransferCharacteristicSystem tcs{};

    if (raw_value == "SDR") {
        tcs.known = VideoTransferCharacteristicSystem::Known::SDR;
    } else if (raw_value == "PQ") {
        tcs.known = VideoTransferCharacteristicSystem::Known::PQ;
    } else if (raw_value == "HLG") {
        tcs.known = VideoTransferCharacteristicSystem::Known::HLG;
    } else if (raw_value == "LINEAR" || raw_value == "Linear") {
        tcs.known = VideoTransferCharacteristicSystem::Known::Linear;
    } else if (raw_value == "BT2100LINPQ") {
        tcs.known = VideoTransferCharacteristicSystem::Known::Bt2100LinPq;
    } else if (raw_value == "BT2100LINHLG") {
        tcs.known = VideoTransferCharacteristicSystem::Known::Bt2100LinHlg;
    } else if (raw_value == "ST2065-1") {
        tcs.known = VideoTransferCharacteristicSystem::Known::St2065_1;
    } else if (raw_value == "ST428-1") {
        tcs.known = VideoTransferCharacteristicSystem::Known::St428_1;
    } else if (raw_value == "DENSITY") {
        tcs.known = VideoTransferCharacteristicSystem::Known::Density;
    } else if (raw_value == "ST2115LOGS3") {
        tcs.known = VideoTransferCharacteristicSystem::Known::St2115LogS3;
    } else if (raw_value == "UNSPECIFIED") {
        tcs.known = VideoTransferCharacteristicSystem::Known::Unspecified;
    } else {
        tcs.known = VideoTransferCharacteristicSystem::Known::Other;
        tcs.raw_token = std::string(raw_value);
    }

    return tcs;
}

[[nodiscard]] inline VideoSignalStandard video_signal_standard_from_raw_value(std::string_view raw_value) {
    VideoSignalStandard ssn{};

    if (raw_value == "ST2110-20:2017") {
        ssn.known = VideoSignalStandard::Known::St2110_20_2017;
    } else if (raw_value == "ST2110-20:2022") {
        ssn.known = VideoSignalStandard::Known::St2110_20_2022;
    } else {
        ssn.known = VideoSignalStandard::Known::Other;
        ssn.raw_token = std::string(raw_value);
    }

    return ssn;
}

[[nodiscard]] inline VideoRange video_range_from_raw_value(std::string_view raw_value) {
    VideoRange range{};

    if (raw_value == "NARROW") {
        range.known = VideoRange::Known::Narrow;
    } else if (raw_value == "FULLPROTECT") {
        range.known = VideoRange::Known::FullProtect;
    } else if (raw_value == "FULL") {
        range.known = VideoRange::Known::Full;
    } else {
        range.known = VideoRange::Known::Other;
        range.raw_token = std::string(raw_value);
    }

    return range;
}

[[nodiscard]] inline std::expected<VideoPackingMode, Error>
parse_video_packing_mode_from_raw_value(std::string_view raw_value) {
    if (raw_value == "2110GPM") {
        return VideoPackingMode::Gpm;
    }

    if (raw_value == "2110BPM") {
        return VideoPackingMode::Bpm;
    }

    return std::unexpected(Error::InvalidValue);
}

[[nodiscard]] inline std::expected<VideoSenderType, Error>
parse_video_sender_type_from_raw_value(const std::string_view raw_value) {
    if (raw_value == "2110TPN") {
        return VideoSenderType::Narrow;
    }

    if (raw_value == "2110TPNL") {
        return VideoSenderType::NarrowLinear;
    }

    if (raw_value == "2110TPW") {
        return VideoSenderType::Wide;
    }

    return std::unexpected(Error::InvalidValue);
}

[[nodiscard]] inline Error apply_video_media_specific_fmtp_to_signaling(const std::vector<std::string> &parameters,
                                                                        VideoStreamSignaling &signaling) {
    bool have_sampling = false;
    bool have_width = false;
    bool have_height = false;
    bool have_exactframerate = false;
    bool have_depth = false;
    bool have_colorimetry = false;
    bool have_packing_mode = false;
    bool have_signal_standard = false;
    bool have_pixel_aspect_ratio = false;
    bool have_sender_type = false;

    bool interlace = false;
    bool segmented = false;

    for (const std::string &parameter_text : parameters) {
        auto parsed_token = parse_video_sdp_parse_fmtp_token(parameter_text);
        if (!parsed_token.has_value()) {
            return parsed_token.error();
        }

        const RawVideoSdpParseFmtpToken token = *parsed_token;

        if (token.name == "sampling") {
            if (have_sampling || !token.value.has_value()) {
                return Error::InvalidValue;
            }

            signaling.media.sampling = video_sampling_from_raw_value(*token.value);
            have_sampling = true;
            continue;
        }

        if (token.name == "width") {
            if (have_width || !token.value.has_value()) {
                return Error::InvalidValue;
            }

            auto parsed = parse_video_sdp_parse_positive_u32(*token.value);
            if (!parsed.has_value()) {
                return parsed.error();
            }

            signaling.media.width = *parsed;
            have_width = true;
            continue;
        }

        if (token.name == "height") {
            if (have_height || !token.value.has_value()) {
                return Error::InvalidValue;
            }

            auto parsed = parse_video_sdp_parse_positive_u32(*token.value);
            if (!parsed.has_value()) {
                return parsed.error();
            }

            signaling.media.height = *parsed;
            have_height = true;
            continue;
        }

        if (token.name == "exactframerate") {
            if (have_exactframerate || !token.value.has_value()) {
                return Error::InvalidValue;
            }

            auto parsed = parse_video_sdp_parse_exactframerate(*token.value);
            if (!parsed.has_value()) {
                return parsed.error();
            }

            signaling.media.fps_num = parsed->numerator;
            signaling.media.fps_den = parsed->denominator;
            have_exactframerate = true;
            continue;
        }

        if (token.name == "depth") {
            if (have_depth || !token.value.has_value()) {
                return Error::InvalidValue;
            }

            auto parsed = parse_video_sdp_parse_depth(*token.value);
            if (!parsed.has_value()) {
                return parsed.error();
            }

            if (parsed->first > std::numeric_limits<std::uint8_t>::max()) {
                return Error::InvalidValue;
            }

            signaling.media.depth.bits = static_cast<std::uint8_t>(parsed->first);
            signaling.media.depth.floating_point = parsed->second;
            have_depth = true;
            continue;
        }

        if (token.name == "colorimetry") {
            if (have_colorimetry || !token.value.has_value()) {
                return Error::InvalidValue;
            }

            signaling.media.colorimetry = video_colorimetry_from_raw_value(*token.value);
            have_colorimetry = true;
            continue;
        }

        if (token.name == "PM") {
            if (have_packing_mode || !token.value.has_value()) {
                return Error::InvalidValue;
            }

            auto parsed = parse_video_packing_mode_from_raw_value(*token.value);
            if (!parsed.has_value()) {
                return parsed.error();
            }

            signaling.packing_mode = *parsed;
            have_packing_mode = true;
            continue;
        }

        if (token.name == "SSN") {
            if (have_signal_standard || !token.value.has_value()) {
                return Error::InvalidValue;
            }

            signaling.media.signal_standard = video_signal_standard_from_raw_value(*token.value);
            have_signal_standard = true;
            continue;
        }

        if (token.name == "TCS") {
            if (signaling.media.transfer_characteristic_system.has_value() || !token.value.has_value()) {
                return Error::InvalidValue;
            }

            signaling.media.transfer_characteristic_system = video_tcs_from_raw_value(*token.value);
            continue;
        }

        if (token.name == "RANGE") {
            if (signaling.media.range.has_value() || !token.value.has_value()) {
                return Error::InvalidValue;
            }

            signaling.media.range = video_range_from_raw_value(*token.value);
            continue;
        }

        if (token.name == "PAR") {
            if (have_pixel_aspect_ratio || !token.value.has_value()) {
                return Error::InvalidValue;
            }

            auto parsed = parse_video_sdp_parse_pixel_aspect_ratio(*token.value);
            if (!parsed.has_value()) {
                return parsed.error();
            }

            signaling.media.pixel_aspect_ratio.width = parsed->width;
            signaling.media.pixel_aspect_ratio.height = parsed->height;
            have_pixel_aspect_ratio = true;
            continue;
        }

        if (token.name == "interlace") {
            if (token.value.has_value() || interlace) {
                return Error::InvalidValue;
            }

            interlace = true;
            continue;
        }

        if (token.name == "segmented") {
            if (token.value.has_value() || segmented) {
                return Error::InvalidValue;
            }

            segmented = true;
            continue;
        }

        if (token.name == "TP") {
            if (have_sender_type || !token.value.has_value()) {
                return Error::InvalidValue;
            }

            auto parsed = parse_video_sender_type_from_raw_value(*token.value);
            if (!parsed.has_value()) {
                return parsed.error();
            }

            signaling.sender_type = *parsed;
            have_sender_type = true;
            continue;
        }

        if (token.name == "TROFF") {
            if (signaling.troff_us.has_value() || !token.value.has_value()) {
                return Error::InvalidValue;
            }

            auto parsed = parse_sdp_numeric_value<std::uint32_t>(*token.value);
            if (!parsed.has_value()) {
                return parsed.error();
            }

            signaling.troff_us = *parsed;
            continue;
        }

        if (token.name == "CMAX") {
            if (signaling.cmax.has_value() || !token.value.has_value()) {
                return Error::InvalidValue;
            }

            auto parsed = parse_sdp_numeric_value<std::uint32_t>(*token.value);
            if (!parsed.has_value()) {
                return parsed.error();
            }

            signaling.cmax = *parsed;
            continue;
        }
    }

    if (!have_sampling || !have_width || !have_height || !have_exactframerate || !have_depth || !have_colorimetry ||
        !have_packing_mode || !have_signal_standard) {
        return Error::InvalidValue;
    }

    if (!interlace && !segmented) {
        signaling.scan_mode = VideoScanMode::Progressive;
    } else if (interlace && !segmented) {
        signaling.scan_mode = VideoScanMode::Interlaced;
    } else if (interlace && segmented) {
        signaling.scan_mode = VideoScanMode::PsF;
    } else {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

[[nodiscard]] inline std::expected<VideoStreamSignaling, Error>
parse_video_stream_signaling(const RawSdpDocument &raw_sdp, const std::uint8_t expected_payload_type) {
    auto selected_media = select_raw_video_sdp_media_section(raw_sdp, expected_payload_type);
    if (!selected_media.has_value()) {
        return std::unexpected(selected_media.error());
    }

    const RawSdpMediaSectionLines &media = **selected_media;

    if (media.rtpmap.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    if (media.fmtp_media_specific_parameters.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    auto raw_rtpmap = parse_video_sdp_parse_rtpmap_payload(media.rtpmap);
    if (!raw_rtpmap.has_value()) {
        return std::unexpected(raw_rtpmap.error());
    }

    auto parsed_timing = parse_stream_timing_signaling(raw_sdp.session, media, raw_rtpmap->clock_rate);
    if (!parsed_timing.has_value()) {
        return std::unexpected(parsed_timing.error());
    }

    auto parsed_transport = parse_stream_transport_signaling(raw_sdp.session, media);
    if (!parsed_transport.has_value()) {
        return std::unexpected(parsed_transport.error());
    }

    VideoStreamSignaling signaling{};

    if (const Error err = apply_video_media_specific_fmtp_to_signaling(media.fmtp_media_specific_parameters, signaling);
        err != Error::Ok) {
        return std::unexpected(err);
    }

    signaling.timing = std::move(*parsed_timing);
    signaling.transport = std::move(*parsed_transport);

    if (const Error err = validate_video_stream_signaling(signaling); err != Error::Ok) {
        return std::unexpected(err);
    }

    return signaling;
}

[[nodiscard]] inline std::expected<VideoStreamSignaling, Error>
parse_video_stream_signaling(std::string_view sdp, const std::uint8_t expected_payload_type) {
    auto raw_sdp = parse_raw_sdp_document(sdp);
    if (!raw_sdp.has_value()) {
        return std::unexpected(raw_sdp.error());
    }

    return parse_video_stream_signaling(*raw_sdp, expected_payload_type);
}

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_VIDEO_SDP_PARSE_HPP
#ifndef ST2110_OBS_PLUGIN_VIDEO_SDP_SIGNALING_ADAPTER_HPP
#define ST2110_OBS_PLUGIN_VIDEO_SDP_SIGNALING_ADAPTER_HPP

#include "video_scan_mode.hpp"
#include "error.hpp"
#include "video_packing_mode.hpp"
#include "signaling_structs.hpp"
#include "video_sdp_fmtp.hpp"
#include "video_signaling.hpp"

#include <expected>

namespace st2110 {
inline std::expected<VideoScanMode, Error>
video_scan_mode_from_raw_video_sdp_fmtp(const RawVideoSdpFmtpParameters &raw) {
    if (!raw.interlace && !raw.segmented) {
        return VideoScanMode::Progressive;
    } else if (raw.interlace && !raw.segmented) {
        return VideoScanMode::Interlaced;
    } else if (raw.interlace && raw.segmented) {
        return VideoScanMode::PsF;
    }
    return std::unexpected(Error::InvalidValue);
}

inline std::expected<VideoPackingMode, Error> video_packing_mode_from_raw_video_sdp_fmtp(std::string_view raw_pm) {
    if (raw_pm == "2110GPM") {
        return VideoPackingMode::Gpm;
    }
    if (raw_pm == "2110BPM") {
        return VideoPackingMode::Bpm;
    }
    return std::unexpected(Error::InvalidValue);
}

inline std::expected<VideoMediaDescription, Error>
video_media_description_from_raw_video_sdp_fmtp(const RawVideoSdpFmtpParameters &raw) {
    VideoMediaDescription res{};

    VideoSampling sampling{};
    if (raw.sampling == "YCbCr-4:2:2") {
        sampling.known = VideoSampling::Known::YCbCr422;
    } else if (raw.sampling == "YCbCr-4:4:4") {
        sampling.known = VideoSampling::Known::YCbCr444;
    } else if (raw.sampling == "YCbCr-4:2:0") {
        sampling.known = VideoSampling::Known::YCbCr420;
    } else if (raw.sampling == "RGB") {
        sampling.known = VideoSampling::Known::RGB;
    } else if (raw.sampling == "XYZ") {
        sampling.known = VideoSampling::Known::XYZ;
    } else if (raw.sampling == "KEY") {
        sampling.known = VideoSampling::Known::Key;
    } else if (raw.sampling == "CLYCbCr-4:4:4") {
        sampling.known = VideoSampling::Known::CLYCbCr444;
    } else if (raw.sampling == "CLYCbCr-4:2:2") {
        sampling.known = VideoSampling::Known::CLYCbCr422;
    } else if (raw.sampling == "CLYCbCr-4:2:0") {
        sampling.known = VideoSampling::Known::CLYCbCr420;
    } else if (raw.sampling == "ICtCp-4:4:4") {
        sampling.known = VideoSampling::Known::ICtCp444;
    } else if (raw.sampling == "ICtCp-4:2:2") {
        sampling.known = VideoSampling::Known::ICtCp422;
    } else if (raw.sampling == "ICtCp-4:2:0") {
        sampling.known = VideoSampling::Known::ICtCp420;
    } else {
        sampling.known = VideoSampling::Known::Other;
        sampling.raw_token = raw.sampling;
    }
    res.sampling = sampling;

    res.width = raw.width;
    res.height = raw.height;
    res.fps_num = raw.exactframerate.numerator;
    res.fps_den = raw.exactframerate.denominator;

    VideoBitDepth depth{};
    if (raw.depth == 0 || raw.depth > 255) {
        return std::unexpected(Error::InvalidValue);
    }
    depth.bits = static_cast<uint8_t>(raw.depth);
    depth.floating_point = raw.depth_floating_point;
    res.depth = depth;

    VideoColorimetry colorimetry{};
    if (raw.colorimetry == "BT601") {
        colorimetry.known = VideoColorimetry::Known::Bt601;
    } else if (raw.colorimetry == "BT709") {
        colorimetry.known = VideoColorimetry::Known::Bt709;
    } else if (raw.colorimetry == "BT2020") {
        colorimetry.known = VideoColorimetry::Known::Bt2020;
    } else if (raw.colorimetry == "BT2100") {
        colorimetry.known = VideoColorimetry::Known::Bt2100;
    } else if (raw.colorimetry == "ST2065-1") {
        colorimetry.known = VideoColorimetry::Known::St2065_1;
    } else if (raw.colorimetry == "ST2065-3") {
        colorimetry.known = VideoColorimetry::Known::St2065_3;
    } else if (raw.colorimetry == "UNSPECIFIED") {
        colorimetry.known = VideoColorimetry::Known::Unspecified;
    } else if (raw.colorimetry == "XYZ") {
        colorimetry.known = VideoColorimetry::Known::Xyz;
    } else if (raw.colorimetry == "ALPHA") {
        colorimetry.known = VideoColorimetry::Known::Alpha;
    } else {
        colorimetry.known = VideoColorimetry::Known::Other;
        colorimetry.raw_token = raw.colorimetry;
    }
    res.colorimetry = colorimetry;

    VideoTransferCharacteristicSystem transferCharacteristicSystem{};
    if (raw.transfer_characteristic_system.has_value()) {
        if (*raw.transfer_characteristic_system == "SDR") {
            transferCharacteristicSystem.known = VideoTransferCharacteristicSystem::Known::SDR;
        } else if (*raw.transfer_characteristic_system == "PQ") {
            transferCharacteristicSystem.known = VideoTransferCharacteristicSystem::Known::PQ;
        } else if (*raw.transfer_characteristic_system == "HLG") {
            transferCharacteristicSystem.known = VideoTransferCharacteristicSystem::Known::HLG;
        } else if (*raw.transfer_characteristic_system == "LINEAR" || *raw.transfer_characteristic_system == "Linear") {
            transferCharacteristicSystem.known = VideoTransferCharacteristicSystem::Known::Linear;
        } else if (*raw.transfer_characteristic_system == "BT2100LINPQ") {
            transferCharacteristicSystem.known = VideoTransferCharacteristicSystem::Known::Bt2100LinPq;
        } else if (*raw.transfer_characteristic_system == "BT2100LINHLG") {
            transferCharacteristicSystem.known = VideoTransferCharacteristicSystem::Known::Bt2100LinHlg;
        } else if (*raw.transfer_characteristic_system == "ST2065-1") {
            transferCharacteristicSystem.known = VideoTransferCharacteristicSystem::Known::St2065_1;
        } else if (*raw.transfer_characteristic_system == "ST428-1") {
            transferCharacteristicSystem.known = VideoTransferCharacteristicSystem::Known::St428_1;
        } else if (*raw.transfer_characteristic_system == "DENSITY") {
            transferCharacteristicSystem.known = VideoTransferCharacteristicSystem::Known::Density;
        } else if (*raw.transfer_characteristic_system == "ST2115LOGS3") {
            transferCharacteristicSystem.known = VideoTransferCharacteristicSystem::Known::St2115LogS3;
        } else if (*raw.transfer_characteristic_system == "UNSPECIFIED") {
            transferCharacteristicSystem.known = VideoTransferCharacteristicSystem::Known::Unspecified;
        } else {
            transferCharacteristicSystem.known = VideoTransferCharacteristicSystem::Known::Other;
            transferCharacteristicSystem.raw_token = *raw.transfer_characteristic_system;
        }

        res.transfer_characteristic_system = std::move(transferCharacteristicSystem);
    }

    VideoSignalStandard signalStandard{};
    if (raw.signal_standard == "ST2110-20:2017") {
        signalStandard.known = VideoSignalStandard::Known::St2110_20_2017;
    } else if (raw.signal_standard == "ST2110-20:2022") {
        signalStandard.known = VideoSignalStandard::Known::St2110_20_2022;
    } else {
        signalStandard.known = VideoSignalStandard::Known::Other;
        signalStandard.raw_token = raw.signal_standard;
    }
    res.signal_standard = signalStandard;

    VideoRange range{};
    if (raw.range.has_value()) {
        if (*raw.range == "NARROW") {
            range.known = VideoRange::Known::Narrow;
        } else if (*raw.range == "FULL") {
            range.known = VideoRange::Known::Full;
        } else {
            range.known = VideoRange::Known::Other;
            range.raw_token = *raw.range;
        }

        res.range = std::move(range);
    }

    return res;
}

inline std::expected<VideoStreamSignaling, Error>
video_stream_signaling_from_raw_video_sdp_fmtp(const RawVideoSdpFmtpParameters &raw) {
    VideoStreamSignaling res{};

    auto expected_media = video_media_description_from_raw_video_sdp_fmtp(raw);
    if (!expected_media.has_value()) {
        return std::unexpected(expected_media.error());
    }
    res.media = *expected_media;

    auto expected_scan_mode = video_scan_mode_from_raw_video_sdp_fmtp(raw);
    if (!expected_scan_mode.has_value()) {
        return std::unexpected(expected_scan_mode.error());
    }
    res.scan_mode = *expected_scan_mode;

    auto expected_packing_mode = video_packing_mode_from_raw_video_sdp_fmtp(raw.packing_mode);
    if (!expected_packing_mode.has_value()) {
        return std::unexpected(expected_packing_mode.error());
    }
    res.packing_mode = *expected_packing_mode;

    if (raw.max_udp_datagram_bytes.has_value()) {
        res.max_udp_datagram_bytes = *raw.max_udp_datagram_bytes;
    }

    if (Error err = validate_video_media_description(res.media); err != Error::Ok) {
        return std::unexpected(err);
    }

    if (Error err = validate_video_media_description_cross_field_constraints(res.media, res.scan_mode);
        err != Error::Ok) {
        return std::unexpected(err);
    }

    return res;
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_VIDEO_SDP_SIGNALING_ADAPTER_HPP

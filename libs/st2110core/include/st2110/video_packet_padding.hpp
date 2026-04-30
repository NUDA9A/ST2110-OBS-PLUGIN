#ifndef ST2110_OBS_PLUGIN_VIDEO_PACKET_PADDING_HPP
#define ST2110_OBS_PLUGIN_VIDEO_PACKET_PADDING_HPP

#include "error.hpp"
#include "packet_view.hpp"
#include "video_packing_mode.hpp"
#include "video_scan_mode.hpp"

namespace st2110 {
inline Error validate_psf_video_packet_trailing_padding(const PacketView &pkt_view) { return Error::Unsupported; }

inline Error validate_interlaced_video_packet_trailing_padding(const PacketView &pkt_view) {
    return Error::Unsupported;
}

inline Error validate_progressive_video_packet_trailing_padding(const PacketView &pkt_view) {
    if (pkt_view.trailing_padding.empty()) {
        return Error::Ok;
    }

    if (!pkt_view.rtp.marker) {
        return Error::InvalidValue;
    }

    for (const auto &b : pkt_view.trailing_padding) {
        if (b != uint8_t{0}) {
            return Error::InvalidValue;
        }
    }

    return Error::Ok;
}

inline Error validate_gpm_video_packet_trailing_padding(VideoScanMode scan_mode, const PacketView &pkt_view) {
    switch (scan_mode) {
    case VideoScanMode::Progressive:
        return validate_progressive_video_packet_trailing_padding(pkt_view);
    case VideoScanMode::Interlaced:
        return validate_interlaced_video_packet_trailing_padding(pkt_view);
    case VideoScanMode::PsF:
        return validate_psf_video_packet_trailing_padding(pkt_view);
    default:
        return Error::InvalidValue;
    }
}

inline Error validate_bpm_video_packet_trailing_padding(VideoScanMode scan_mode, const PacketView &pkt_view) {
    (void)scan_mode;
    (void)pkt_view;
    return Error::Unsupported;
}

inline Error validate_video_packet_trailing_padding(VideoPackingMode packing_mode, VideoScanMode scan_mode,
                                                    const PacketView &pkt_view) {
    switch (packing_mode) {
    case VideoPackingMode::Gpm:
        return validate_gpm_video_packet_trailing_padding(scan_mode, pkt_view);
    case VideoPackingMode::Bpm:
        return validate_bpm_video_packet_trailing_padding(scan_mode, pkt_view);
    default:
        return Error::InvalidValue;
    }
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_VIDEO_PACKET_PADDING_HPP

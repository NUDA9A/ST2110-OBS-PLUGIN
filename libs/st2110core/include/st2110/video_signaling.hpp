#ifndef ST2110_OBS_PLUGIN_VIDEO_SIGNALING_HPP
#define ST2110_OBS_PLUGIN_VIDEO_SIGNALING_HPP

#include "pixel_format.hpp"
#include "video_scan_mode.hpp"
#include "error.hpp"
#include "config_validation.hpp"
#include "packet_parse.hpp"

#include <cstdint>
#include <optional>


namespace st2110 {
    enum class MediaClockMode {
        Direct, Unsupported
    };

    enum class TimestampMode {
        Narrow, Linear, Unsupported
    };

    enum class ReferenceClockKind {
        LocalMac, Ptp, Unsupported
    };

    struct VideoStreamSignaling {
        PixelFormat format = PixelFormat::UYVY;
        VideoScanMode scan_mode = VideoScanMode::Progressive;

        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t fps_num = 0;
        uint32_t fps_den = 0;

        std::optional<std::size_t> max_udp_datagram_bytes{};

        MediaClockMode media_clock_mode = ...;
        TimestampMode timestamp_mode = ...;
        ReferenceClockKind reference_clock = ...;

        std::optional<uint32_t> ts_delay_sender_ticks{};
    };

    inline Error validate_video_stream_signaling(const VideoStreamSignaling& videoStreamSignaling) {
        if (Error err = validate_video_format_constraints(videoStreamSignaling.format, videoStreamSignaling.width, videoStreamSignaling.height); err != Error::Ok) {
            return err;
        }
        if (Error err = validate_video_scan_mode(videoStreamSignaling.scan_mode); err != Error::Ok) {
            return err;
        }
        if (Error err = validate_packet_parse_policy_config(PacketParsePolicy{videoStreamSignaling.max_udp_datagram_bytes}); err != Error::Ok) {
            return err;
        }
    }
}

#endif //ST2110_OBS_PLUGIN_VIDEO_SIGNALING_HPP

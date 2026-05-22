#ifndef ST2110_OBS_VIDEO_SIGNALING_TYPES_HPP
#define ST2110_OBS_VIDEO_SIGNALING_TYPES_HPP

#include <st2110/model/common_sdp_parameters.hpp>
#include <st2110/model/video/video_media_types.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace st2110 {
enum class VideoSenderType { Narrow, NarrowLinear, Wide };

struct VideoStreamSignaling {
    VideoMediaDescription media{};
    VideoScanMode scan_mode = VideoScanMode::Progressive;
    VideoPackingMode packing_mode = VideoPackingMode::Gpm;

    StreamTimingSignaling timing{};
    StreamTransportSignaling transport{};

    VideoSenderType sender_type = VideoSenderType::Narrow;
    std::optional<std::uint32_t> troff_us{};
    std::optional<std::uint32_t> cmax{};
};

inline Error validate_video_sender_signaling(const VideoSenderType sender_type,
                                             const std::optional<std::uint32_t> &troff_us,
                                             const std::optional<std::uint32_t> &cmax) {
    (void)sender_type;

    if (troff_us.has_value() && *troff_us == 0) {
        return Error::InvalidValue;
    }

    if (cmax.has_value() && *cmax == 0) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

inline Error validate_video_max_udp_datagram_bytes(const std::optional<std::size_t> &max_udp_datagram_bytes,
                                                   const VideoPackingMode packing_mode) {
    const std::size_t effective_max_udp = max_udp_datagram_bytes.value_or(1460);

    if (packing_mode == VideoPackingMode::Bpm) {
        return effective_max_udp <= 1460 ? Error::Ok : Error::InvalidValue;
    }

    return Error::Ok;
}

inline Error validate_video_stream_signaling(const VideoStreamSignaling &signaling) {
    if (const Error err =
            validate_video_media_description_cross_field_constraints(signaling.media, signaling.scan_mode);
        err != Error::Ok) {
        return err;
    }
    if (const Error err = validate_video_sender_signaling(signaling.sender_type, signaling.troff_us, signaling.cmax);
        err != Error::Ok) {
        return err;
    }

    if (const Error err = validate_stream_timing_signaling(signaling.timing); err != Error::Ok) {
        return err;
    }

    if (const Error err = validate_stream_transport_signaling(signaling.transport); err != Error::Ok) {
        return err;
    }

    if (const Error err =
            validate_video_max_udp_datagram_bytes(signaling.transport.max_udp_datagram_bytes, signaling.packing_mode);
        err != Error::Ok) {
        return err;
    }

    if (signaling.timing.rtp_clock_rate != 90000) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

} // namespace st2110

#endif // ST2110_OBS_VIDEO_SIGNALING_TYPES_HPP

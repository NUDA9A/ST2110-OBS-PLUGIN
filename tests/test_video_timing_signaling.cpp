#include <cassert>
#include <cstdint>
#include <optional>

#include <st2110/video_signaling.hpp>

static st2110::VideoStreamSignaling make_base_signaling() {
    st2110::VideoStreamSignaling s{};

    s.media.sampling = st2110::VideoSampling{st2110::VideoSampling::Known::YCbCr422, std::nullopt};
    s.media.width = 1920;
    s.media.height = 1080;
    s.media.fps_num = 30000;
    s.media.fps_den = 1001;
    s.media.depth = st2110::VideoBitDepth{8, false};
    s.media.colorimetry = st2110::VideoColorimetry{st2110::VideoColorimetry::Known::Bt709, std::nullopt};

    s.scan_mode = st2110::VideoScanMode::Progressive;
    s.packing_mode = st2110::VideoPackingMode::Gpm;
    s.max_udp_datagram_bytes = st2110::standardUdpDatagramSizeLimitBytes;

    s.media_clock_mode = st2110::MediaClockMode::Direct;
    s.timestamp_mode = st2110::TimestampMode::New;

    s.reference_clock.kind = st2110::ReferenceClockKind::Ptp;
    s.reference_clock.ptp = st2110::PtpReferenceClock{};

    s.ts_delay_sender_ticks = 0;

    s.sender_type = st2110::VideoSenderType::Narrow;
    s.troff_us = std::nullopt;
    s.cmax = std::nullopt;

    return s;
}

static void test_video_stream_signaling_rejects_invalid_media_clock_mode() {
    st2110::VideoStreamSignaling s = make_base_signaling();
    s.media_clock_mode = static_cast<st2110::MediaClockMode>(999);

    assert(st2110::validate_video_stream_signaling(s) == st2110::Error::InvalidValue);
}

static void test_video_stream_signaling_rejects_invalid_timestamp_mode() {
    st2110::VideoStreamSignaling s = make_base_signaling();
    s.timestamp_mode = static_cast<st2110::TimestampMode>(999);

    assert(st2110::validate_video_stream_signaling(s) == st2110::Error::InvalidValue);
}

int main() {
    test_video_stream_signaling_rejects_invalid_media_clock_mode();
    test_video_stream_signaling_rejects_invalid_timestamp_mode();
    return 0;
}
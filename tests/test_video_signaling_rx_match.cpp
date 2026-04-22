#include <cassert>
#include <cstdint>

#include <st2110/video_signaling.hpp>
#include <st2110/rx_config.hpp>

static st2110::VideoStreamSignaling make_signaling() {
    st2110::VideoStreamSignaling s{};
    s.format = st2110::PixelFormat::UYVY;
    s.scan_mode = st2110::VideoScanMode::Progressive;
    s.width = 1920;
    s.height = 1080;
    s.fps_num = 30000;
    s.fps_den = 1001;

    s.packing_mode = st2110::VideoPackingMode::Gpm;
    s.media_clock_mode = st2110::MediaClockMode::Direct;
    s.timestamp_mode = st2110::TimestampMode::New;

    s.reference_clock.kind = st2110::ReferenceClockKind::Ptp;
    s.reference_clock.ptp = st2110::PtpReferenceClock{};

    s.ts_delay_sender_ticks = 0;
    s.sender_type = st2110::VideoSenderType::Narrow;

    return s;
}

static st2110::RxVideoConfig make_rx_config() {
    return st2110::RxVideoConfig{
            .width = 1920,
            .height = 1080,
            .fps_num = 30000,
            .fps_den = 1001,
            .udp_port = 5004,
            .payload_type = 112,
            .local_ip = "0.0.0.0",
            .dest_ip = "239.1.1.1",
            .format = st2110::PixelFormat::UYVY,
            .scan_mode = st2110::VideoScanMode::Progressive
    };
}

static void test_matching_signaling_and_rx_config_is_ok() {
    const st2110::VideoStreamSignaling s = make_signaling();
    const st2110::RxVideoConfig cfg = make_rx_config();

    assert(st2110::validate_video_stream_signaling_against_rx_video_config(s, cfg)
           == st2110::Error::Ok);
}

static void test_width_mismatch_is_invalid() {
    st2110::VideoStreamSignaling s = make_signaling();
    st2110::RxVideoConfig cfg = make_rx_config();

    cfg.width = 1280;

    assert(st2110::validate_video_stream_signaling_against_rx_video_config(s, cfg)
           == st2110::Error::InvalidValue);
}

static void test_scan_mode_mismatch_is_invalid() {
    st2110::VideoStreamSignaling s = make_signaling();
    st2110::RxVideoConfig cfg = make_rx_config();

    cfg.scan_mode = st2110::VideoScanMode::Interlaced;

    assert(st2110::validate_video_stream_signaling_against_rx_video_config(s, cfg)
           == st2110::Error::InvalidValue);
}

static void test_frame_rate_mismatch_is_invalid() {
    st2110::VideoStreamSignaling s = make_signaling();
    st2110::RxVideoConfig cfg = make_rx_config();

    cfg.fps_num = 25;
    cfg.fps_den = 1;

    assert(st2110::validate_video_stream_signaling_against_rx_video_config(s, cfg)
           == st2110::Error::InvalidValue);
}

static void test_height_mismatch_is_invalid() {
    st2110::VideoStreamSignaling s = make_signaling();
    st2110::RxVideoConfig cfg = make_rx_config();

    cfg.height = 720;

    assert(st2110::validate_video_stream_signaling_against_rx_video_config(s, cfg)
           == st2110::Error::InvalidValue);
}

int main() {
    test_matching_signaling_and_rx_config_is_ok();
    test_width_mismatch_is_invalid();
    test_scan_mode_mismatch_is_invalid();
    test_frame_rate_mismatch_is_invalid();
    test_height_mismatch_is_invalid();
    return 0;
}
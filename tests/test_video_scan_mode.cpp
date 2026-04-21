#include <cassert>
#include <cstdint>

#include <st2110/depacketizer.hpp>
#include <st2110/error.hpp>
#include <st2110/rx_config.hpp>
#include <st2110/video_scan_mode.hpp>

static st2110::RxVideoConfig make_base_rx_config() {
    st2110::RxVideoConfig cfg{};
    cfg.width = 1920;
    cfg.height = 1080;
    cfg.fps_num = 25;
    cfg.fps_den = 1;
    cfg.udp_port = 5004;
    cfg.payload_type = 112;
    cfg.local_ip = "";
    cfg.dest_ip = "239.1.1.1";
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.scan_mode = st2110::VideoScanMode::Progressive;
    return cfg;
}

static void test_video_scan_mode_values_are_distinct() {
    assert(st2110::VideoScanMode::Progressive != st2110::VideoScanMode::Interlaced);
    assert(st2110::VideoScanMode::Progressive != st2110::VideoScanMode::PsF);
    assert(st2110::VideoScanMode::Interlaced != st2110::VideoScanMode::PsF);
}

static void test_rx_video_config_accepts_progressive_mode() {
    auto cfg = make_base_rx_config();
    cfg.scan_mode = st2110::VideoScanMode::Progressive;

    assert(st2110::validate_rx_video_config(cfg) == st2110::Error::Ok);
}

static void test_rx_video_config_accepts_interlaced_mode_as_model_value() {
    auto cfg = make_base_rx_config();
    cfg.scan_mode = st2110::VideoScanMode::Interlaced;

    assert(st2110::validate_rx_video_config(cfg) == st2110::Error::Ok);
}

static void test_rx_video_config_accepts_psf_mode_as_model_value() {
    auto cfg = make_base_rx_config();
    cfg.scan_mode = st2110::VideoScanMode::PsF;

    assert(st2110::validate_rx_video_config(cfg) == st2110::Error::Ok);
}

static void test_rx_video_config_rejects_unknown_scan_mode_value() {
    auto cfg = make_base_rx_config();
    cfg.scan_mode = static_cast<st2110::VideoScanMode>(999);

    assert(st2110::validate_rx_video_config(cfg) == st2110::Error::InvalidValue);
}

static void test_depacketizer_config_defaults_to_progressive() {
    st2110::DepacketizerConfig cfg{};
    assert(cfg.scan_mode == st2110::VideoScanMode::Progressive);
}

static void test_depacketizer_exposes_configured_scan_mode() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 640;
    cfg.height = 480;
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.scan_mode = st2110::VideoScanMode::PsF;

    st2110::Depacketizer dep(cfg);
    assert(dep.scan_mode() == st2110::VideoScanMode::PsF);
}

static void test_scan_mode_is_independent_from_pixel_format() {
    auto cfg = make_base_rx_config();
    cfg.format = st2110::PixelFormat::UYVY;

    cfg.scan_mode = st2110::VideoScanMode::Progressive;
    assert(st2110::validate_rx_video_config(cfg) == st2110::Error::Ok);

    cfg.scan_mode = st2110::VideoScanMode::Interlaced;
    assert(st2110::validate_rx_video_config(cfg) == st2110::Error::Ok);

    cfg.scan_mode = st2110::VideoScanMode::PsF;
    assert(st2110::validate_rx_video_config(cfg) == st2110::Error::Ok);
}

int main() {
    test_video_scan_mode_values_are_distinct();
    test_rx_video_config_accepts_progressive_mode();
    test_rx_video_config_accepts_interlaced_mode_as_model_value();
    test_rx_video_config_accepts_psf_mode_as_model_value();
    test_rx_video_config_rejects_unknown_scan_mode_value();
    test_depacketizer_config_defaults_to_progressive();
    test_depacketizer_exposes_configured_scan_mode();
    test_scan_mode_is_independent_from_pixel_format();
    return 0;
}
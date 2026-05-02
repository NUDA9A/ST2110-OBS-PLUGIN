#include <cassert>
#include <cstdint>
#include <optional>
#include <string>

#include <st2110/rx_config.hpp>
#include <st2110/video_signaling.hpp>

static st2110::PtpReferenceClock make_valid_ptp_reference_clock() {
    st2110::PtpReferenceClock ptp{};
    ptp.clock_identity = {0x39, 0xA7, 0x94, 0xFF, 0xFE, 0x07, 0xCB, 0xD0};
    ptp.domain_number = 127;
    ptp.traceable = false;
    return ptp;
}

static st2110::VideoSignalStandard make_signal_standard(st2110::VideoSignalStandard::Known known) {
    return st2110::VideoSignalStandard{known, std::nullopt};
}

static st2110::VideoStreamSignaling make_base_signaling() {
    st2110::VideoStreamSignaling s{};

    s.media.sampling = st2110::VideoSampling{st2110::VideoSampling::Known::YCbCr422, std::nullopt};
    s.media.width = 1920;
    s.media.height = 1080;
    s.media.fps_num = 30000;
    s.media.fps_den = 1001;
    s.media.depth = st2110::VideoBitDepth{8, false};
    s.media.colorimetry = st2110::VideoColorimetry{st2110::VideoColorimetry::Known::Bt709, std::nullopt};
    s.media.signal_standard = make_signal_standard(st2110::VideoSignalStandard::Known::St2110_20_2017);

    s.scan_mode = st2110::VideoScanMode::Progressive;

    s.packing_mode = st2110::VideoPackingMode::Gpm;
    s.max_udp_datagram_bytes = st2110::standardUdpDatagramSizeLimitBytes;

    s.media_clock_mode = st2110::MediaClockMode::Direct;
    s.timestamp_mode = st2110::TimestampMode::New;
    s.reference_clock.kind = st2110::ReferenceClockKind::Ptp;
    s.reference_clock.ptp = make_valid_ptp_reference_clock();

    s.ts_delay_sender_ticks = 0;

    s.sender_type = st2110::VideoSenderType::Narrow;
    s.troff_us = std::nullopt;
    s.cmax = std::nullopt;

    return s;
}

static void test_valid_signaling_projects_to_rx_config() {
    const st2110::VideoStreamSignaling s = make_base_signaling();

    auto cfg = st2110::rx_video_config_from_video_stream_signaling(s, 5004, 112, "0.0.0.0", "239.1.1.1");

    assert(cfg.has_value());
    assert(cfg->width == 1920);
    assert(cfg->height == 1080);
    assert(cfg->fps_num == 30000);
    assert(cfg->fps_den == 1001);
    assert(cfg->udp_port == 5004);
    assert(cfg->payload_type == 112);
    assert(cfg->local_ip == "0.0.0.0");
    assert(cfg->dest_ip == "239.1.1.1");
    assert(cfg->format == st2110::PixelFormat::UYVY);
    assert(cfg->scan_mode == st2110::VideoScanMode::Progressive);
    assert(cfg->packing_mode == st2110::VideoPackingMode::Gpm);
}

static void test_invalid_signaling_is_rejected_before_projection() {
    st2110::VideoStreamSignaling s = make_base_signaling();
    s.reference_clock.ptp = std::nullopt;

    auto cfg = st2110::rx_video_config_from_video_stream_signaling(s, 5004, 112, "0.0.0.0", "239.1.1.1");

    assert(!cfg.has_value());
    assert(cfg.error() == st2110::Error::InvalidValue);
}

static void test_missing_ssn_is_rejected_before_projection() {
    st2110::VideoStreamSignaling s = make_base_signaling();
    s.media.signal_standard = std::nullopt;

    auto cfg = st2110::rx_video_config_from_video_stream_signaling(s, 5004, 112, "0.0.0.0", "239.1.1.1");

    assert(!cfg.has_value());
    assert(cfg.error() == st2110::Error::InvalidValue);
}

static void test_invalid_transport_args_are_rejected_after_projection() {
    const st2110::VideoStreamSignaling s = make_base_signaling();

    auto cfg = st2110::rx_video_config_from_video_stream_signaling(s, 0, 112, "0.0.0.0", "239.1.1.1");

    assert(!cfg.has_value());
    assert(cfg.error() == st2110::Error::InvalidValue);
}

static void test_projection_preserves_scan_mode_without_reinterpreting_it() {
    st2110::VideoStreamSignaling s = make_base_signaling();
    s.scan_mode = st2110::VideoScanMode::Interlaced;

    auto cfg = st2110::rx_video_config_from_video_stream_signaling(s, 5004, 112, "0.0.0.0", "239.1.1.1");

    assert(cfg.has_value());
    assert(cfg->scan_mode == st2110::VideoScanMode::Interlaced);
    assert(cfg->packing_mode == st2110::VideoPackingMode::Gpm);
}

static void test_projection_rejects_invalid_payload_type() {
    const st2110::VideoStreamSignaling s = make_base_signaling();

    auto cfg = st2110::rx_video_config_from_video_stream_signaling(s, 5004, 34, "0.0.0.0", "239.1.1.1");

    assert(!cfg.has_value());
    assert(cfg.error() == st2110::Error::InvalidValue);
}

static void test_projection_rejects_unsupported_pixel_format_mapping() {
    st2110::VideoStreamSignaling s = make_base_signaling();
    s.media.sampling = st2110::VideoSampling{st2110::VideoSampling::Known::RGB, std::nullopt};

    auto cfg = st2110::rx_video_config_from_video_stream_signaling(s, 5004, 112, "0.0.0.0", "239.1.1.1");

    assert(!cfg.has_value());
    assert(cfg.error() == st2110::Error::Unsupported);
}

int main() {
    test_valid_signaling_projects_to_rx_config();
    test_invalid_signaling_is_rejected_before_projection();
    test_missing_ssn_is_rejected_before_projection();
    test_invalid_transport_args_are_rejected_after_projection();
    test_projection_preserves_scan_mode_without_reinterpreting_it();
    test_projection_rejects_invalid_payload_type();
    test_projection_rejects_unsupported_pixel_format_mapping();
    return 0;
}
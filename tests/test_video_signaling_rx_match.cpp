#include <cassert>
#include <cstdint>
#include <optional>

#include <st2110/rx_config.hpp>
#include <st2110/video_signaling.hpp>

static st2110::PtpReferenceClock make_valid_ptp_reference_clock() {
    st2110::PtpReferenceClock ptp{};
    ptp.clock_identity = {0x39, 0xA7, 0x94, 0xFF, 0xFE, 0x07, 0xCB, 0xD0};
    ptp.domain_number = 127;
    ptp.traceable = false;
    return ptp;
}

static st2110::VideoRange make_range(st2110::VideoRange::Known known) {
    return st2110::VideoRange{known, std::nullopt};
}

static st2110::VideoSignalStandard make_signal_standard(st2110::VideoSignalStandard::Known known) {
    return st2110::VideoSignalStandard{known, std::nullopt};
}

static st2110::VideoStreamSignaling make_signaling() {
    st2110::VideoStreamSignaling s{};

    s.media.sampling = st2110::VideoSampling{st2110::VideoSampling::Known::YCbCr422, std::nullopt};
    s.media.width = 1920;
    s.media.height = 1080;
    s.media.fps_num = 30000;
    s.media.fps_den = 1001;
    s.media.depth = st2110::VideoBitDepth{8, false};
    s.media.colorimetry = st2110::VideoColorimetry{st2110::VideoColorimetry::Known::Bt709, std::nullopt};
    s.media.signal_standard = make_signal_standard(st2110::VideoSignalStandard::Known::St2110_20_2017);
    s.media.range = make_range(st2110::VideoRange::Known::Narrow);

    s.scan_mode = st2110::VideoScanMode::Progressive;
    s.packing_mode = st2110::VideoPackingMode::Gpm;
    s.media_clock_mode = st2110::MediaClockMode::Direct;
    s.timestamp_mode = st2110::TimestampMode::New;

    s.reference_clock.kind = st2110::ReferenceClockKind::Ptp;
    s.reference_clock.ptp = make_valid_ptp_reference_clock();

    s.ts_delay_sender_ticks = 0;
    s.sender_type = st2110::VideoSenderType::Narrow;

    return s;
}

static st2110::VideoStreamSignaling make_rgb8_signaling() {
    st2110::VideoStreamSignaling s = make_signaling();
    s.media.sampling = st2110::VideoSampling{st2110::VideoSampling::Known::RGB, std::nullopt};
    s.media.depth = st2110::VideoBitDepth{8, false};
    return s;
}

static st2110::RxVideoConfig make_project_field_fallback_rx_config() {
    return st2110::RxVideoConfig{.width = 1920,
                                 .height = 1080,
                                 .fps_num = 30000,
                                 .fps_den = 1001,
                                 .udp_port = 5004,
                                 .payload_type = 112,
                                 .local_ip = "0.0.0.0",
                                 .dest_ip = "239.1.1.1",
                                 .format = st2110::PixelFormat::UYVY,
                                 .scan_mode = st2110::VideoScanMode::Progressive,
                                 .packing_mode = st2110::VideoPackingMode::Gpm};
}

static st2110::RxVideoConfig make_rx_config_from_signaling(const st2110::VideoStreamSignaling &s) {
    auto cfg = st2110::rx_video_config_from_video_stream_signaling(s, 5004, 112, "0.0.0.0", "239.1.1.1");

    assert(cfg.has_value());
    return *cfg;
}

static void test_matching_signaling_and_project_field_fallback_rx_config_is_ok() {
    const st2110::VideoStreamSignaling s = make_signaling();
    const st2110::RxVideoConfig cfg = make_project_field_fallback_rx_config();

    assert(!cfg.receive_capability.has_value());
    assert(st2110::validate_video_stream_signaling_against_rx_video_config(s, cfg) == st2110::Error::Ok);
}

static void test_matching_signaling_and_explicit_receive_capability_rx_config_is_ok() {
    const st2110::VideoStreamSignaling s = make_signaling();
    const st2110::RxVideoConfig cfg = make_rx_config_from_signaling(s);

    assert(cfg.receive_capability.has_value());
    assert(st2110::validate_video_stream_signaling_against_rx_video_config(s, cfg) == st2110::Error::Ok);
}

static void test_width_mismatch_is_invalid() {
    st2110::VideoStreamSignaling s = make_signaling();
    st2110::RxVideoConfig cfg = make_rx_config_from_signaling(s);

    cfg.width = 1280;
    cfg.receive_capability->media.width = 1280;

    assert(st2110::validate_rx_video_config(cfg) == st2110::Error::Ok);
    assert(st2110::validate_video_stream_signaling_against_rx_video_config(s, cfg) == st2110::Error::InvalidValue);
}

static void test_height_mismatch_is_invalid() {
    st2110::VideoStreamSignaling s = make_signaling();
    st2110::RxVideoConfig cfg = make_rx_config_from_signaling(s);

    cfg.height = 720;
    cfg.receive_capability->media.height = 720;

    assert(st2110::validate_rx_video_config(cfg) == st2110::Error::Ok);
    assert(st2110::validate_video_stream_signaling_against_rx_video_config(s, cfg) == st2110::Error::InvalidValue);
}

static void test_frame_rate_mismatch_is_invalid() {
    st2110::VideoStreamSignaling s = make_signaling();
    st2110::RxVideoConfig cfg = make_rx_config_from_signaling(s);

    cfg.fps_num = 25;
    cfg.fps_den = 1;
    cfg.receive_capability->media.fps_num = 25;
    cfg.receive_capability->media.fps_den = 1;

    assert(st2110::validate_rx_video_config(cfg) == st2110::Error::Ok);
    assert(st2110::validate_video_stream_signaling_against_rx_video_config(s, cfg) == st2110::Error::InvalidValue);
}

static void test_scan_mode_mismatch_is_invalid() {
    st2110::VideoStreamSignaling s = make_signaling();
    st2110::RxVideoConfig cfg = make_rx_config_from_signaling(s);

    cfg.scan_mode = st2110::VideoScanMode::Interlaced;
    cfg.receive_capability->scan_mode = st2110::VideoScanMode::Interlaced;

    assert(st2110::validate_rx_video_config(cfg) == st2110::Error::Ok);
    assert(st2110::validate_video_stream_signaling_against_rx_video_config(s, cfg) == st2110::Error::InvalidValue);
}

static void test_packing_mode_mismatch_is_invalid_without_runtime_support_check() {
    st2110::VideoStreamSignaling s = make_signaling();
    st2110::RxVideoConfig cfg = make_rx_config_from_signaling(s);

    cfg.packing_mode = st2110::VideoPackingMode::Bpm;
    cfg.receive_capability->packing_mode = st2110::VideoPackingMode::Bpm;

    assert(st2110::validate_rx_video_config(cfg) == st2110::Error::Ok);
    assert(st2110::validate_video_stream_signaling_against_rx_video_config(s, cfg) == st2110::Error::InvalidValue);
}

static void test_explicit_receive_capability_must_match_rx_project_common_fields() {
    st2110::VideoStreamSignaling s = make_signaling();
    st2110::RxVideoConfig cfg = make_rx_config_from_signaling(s);

    cfg.width = 1280;

    assert(st2110::validate_rx_video_config(cfg) == st2110::Error::InvalidValue);
    assert(st2110::validate_video_stream_signaling_against_rx_video_config(s, cfg) == st2110::Error::InvalidValue);
}

static void test_generic_rfc4175_transport_marker_matches_signaling() {
    const st2110::VideoStreamSignaling s = make_signaling();
    st2110::RxVideoConfig cfg = make_rx_config_from_signaling(s);

    cfg.receive_capability->transport_format = st2110::VideoTransportPayloadFormat::Rfc4175;

    assert(st2110::validate_rx_video_config(cfg) == st2110::Error::Ok);
    assert(st2110::validate_video_stream_signaling_against_rx_video_config(s, cfg) == st2110::Error::Ok);
}

static void test_common_rgb8_receive_capability_matches_rgb8_signaling_before_runtime_support_check() {
    const st2110::VideoStreamSignaling s = make_rgb8_signaling();

    st2110::VideoReceiveCapabilityProjectionOptions options{};
    options.handoff_format = st2110::VideoFrameHandoffFormat::Rgb8;

    auto capability = st2110::video_receive_capability_from_video_stream_signaling(s, options);
    assert(capability.has_value());

    st2110::RxVideoConfig cfg{.width = s.media.width,
                              .height = s.media.height,
                              .fps_num = s.media.fps_num,
                              .fps_den = s.media.fps_den,
                              .udp_port = 5004,
                              .payload_type = 112,
                              .local_ip = "0.0.0.0",
                              .dest_ip = "239.1.1.1",
                              .format = st2110::PixelFormat::UYVY,
                              .scan_mode = s.scan_mode,
                              .packing_mode = s.packing_mode,
                              .receive_capability = *capability};

    assert(st2110::validate_rx_video_config(cfg) == st2110::Error::Ok);
    assert(st2110::validate_video_stream_signaling_against_rx_video_config(s, cfg) == st2110::Error::Ok);

    assert(st2110::validate_rx_video_config_against_runtime_support(
               cfg, st2110::default_video_rx_runtime_support_policy()) == st2110::Error::Unsupported);
}

static void test_missing_ssn_in_signaling_is_invalid_before_rx_match() {
    st2110::VideoStreamSignaling s = make_signaling();
    st2110::RxVideoConfig cfg = make_rx_config_from_signaling(s);

    s.media.signal_standard = std::nullopt;

    assert(st2110::validate_video_stream_signaling_against_rx_video_config(s, cfg) == st2110::Error::InvalidValue);
}

int main() {
    test_matching_signaling_and_project_field_fallback_rx_config_is_ok();
    test_matching_signaling_and_explicit_receive_capability_rx_config_is_ok();
    test_width_mismatch_is_invalid();
    test_height_mismatch_is_invalid();
    test_frame_rate_mismatch_is_invalid();
    test_scan_mode_mismatch_is_invalid();
    test_packing_mode_mismatch_is_invalid_without_runtime_support_check();
    test_explicit_receive_capability_must_match_rx_project_common_fields();
    test_generic_rfc4175_transport_marker_matches_signaling();
    test_common_rgb8_receive_capability_matches_rgb8_signaling_before_runtime_support_check();
    test_missing_ssn_in_signaling_is_invalid_before_rx_match();
    return 0;
}
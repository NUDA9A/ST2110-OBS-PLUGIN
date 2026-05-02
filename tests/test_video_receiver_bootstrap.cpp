#include <cassert>
#include <optional>
#include <string>

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

static void test_valid_bootstrap_composes_all_receiver_inputs() {
    const st2110::VideoStreamSignaling signaling = make_base_signaling();

    auto bootstrap = st2110::video_receiver_bootstrap_config_from_video_stream_signaling(
        signaling, 5004, 112, "0.0.0.0", "239.1.1.1", st2110::PartialFramePolicy::EmitWithFlag);

    assert(bootstrap.has_value());

    assert(bootstrap->packet_parse_policy.max_udp_datagram_bytes.has_value());
    assert(*bootstrap->packet_parse_policy.max_udp_datagram_bytes == st2110::standardUdpDatagramSizeLimitBytes);

    assert(bootstrap->rx_config.width == 1920);
    assert(bootstrap->rx_config.height == 1080);
    assert(bootstrap->rx_config.fps_num == 30000);
    assert(bootstrap->rx_config.fps_den == 1001);
    assert(bootstrap->rx_config.udp_port == 5004);
    assert(bootstrap->rx_config.payload_type == 112);
    assert(bootstrap->rx_config.local_ip == "0.0.0.0");
    assert(bootstrap->rx_config.dest_ip == "239.1.1.1");
    assert(bootstrap->rx_config.format == st2110::PixelFormat::UYVY);
    assert(bootstrap->rx_config.scan_mode == st2110::VideoScanMode::Progressive);
    assert(bootstrap->rx_config.packing_mode == st2110::VideoPackingMode::Gpm);

    assert(bootstrap->receive_pipeline_config.depacketizer.width == 1920);
    assert(bootstrap->receive_pipeline_config.depacketizer.height == 1080);
    assert(bootstrap->receive_pipeline_config.depacketizer.format == st2110::PixelFormat::UYVY);
    assert(bootstrap->receive_pipeline_config.depacketizer.scan_mode == st2110::VideoScanMode::Progressive);
    assert(bootstrap->receive_pipeline_config.depacketizer.packing_mode == st2110::VideoPackingMode::Gpm);
    assert(bootstrap->receive_pipeline_config.depacketizer.partial_frame_policy ==
           st2110::PartialFramePolicy::EmitWithFlag);

    assert(bootstrap->receive_pipeline_config.reconstructor.format == st2110::PixelFormat::UYVY);
    assert(bootstrap->receive_pipeline_config.reconstructor.scan_mode == st2110::VideoScanMode::Progressive);
}

static void test_bootstrap_preserves_absent_maxudp_override() {
    st2110::VideoStreamSignaling signaling = make_base_signaling();
    signaling.max_udp_datagram_bytes = std::nullopt;

    auto bootstrap = st2110::video_receiver_bootstrap_config_from_video_stream_signaling(
        signaling, 5004, 112, "0.0.0.0", "239.1.1.1", st2110::PartialFramePolicy::Drop);

    assert(bootstrap.has_value());
    assert(!bootstrap->packet_parse_policy.max_udp_datagram_bytes.has_value());
    assert(bootstrap->rx_config.packing_mode == st2110::VideoPackingMode::Gpm);
    assert(bootstrap->receive_pipeline_config.depacketizer.partial_frame_policy == st2110::PartialFramePolicy::Drop);
    assert(bootstrap->receive_pipeline_config.depacketizer.packing_mode == st2110::VideoPackingMode::Gpm);
}

static void test_bootstrap_rejects_invalid_signaling_before_projection() {
    st2110::VideoStreamSignaling signaling = make_base_signaling();
    signaling.reference_clock.ptp = std::nullopt;

    auto bootstrap = st2110::video_receiver_bootstrap_config_from_video_stream_signaling(
        signaling, 5004, 112, "0.0.0.0", "239.1.1.1", st2110::PartialFramePolicy::EmitWithFlag);

    assert(!bootstrap.has_value());
    assert(bootstrap.error() == st2110::Error::InvalidValue);
}

static void test_bootstrap_rejects_missing_ssn_before_projection() {
    st2110::VideoStreamSignaling signaling = make_base_signaling();
    signaling.media.signal_standard = std::nullopt;

    auto bootstrap = st2110::video_receiver_bootstrap_config_from_video_stream_signaling(
        signaling, 5004, 112, "0.0.0.0", "239.1.1.1", st2110::PartialFramePolicy::EmitWithFlag);

    assert(!bootstrap.has_value());
    assert(bootstrap.error() == st2110::Error::InvalidValue);
}

static void test_bootstrap_rejects_invalid_transport_inputs_after_signaling_projection() {
    const st2110::VideoStreamSignaling signaling = make_base_signaling();

    auto bootstrap = st2110::video_receiver_bootstrap_config_from_video_stream_signaling(
        signaling, 0, 112, "0.0.0.0", "239.1.1.1", st2110::PartialFramePolicy::EmitWithFlag);

    assert(!bootstrap.has_value());
    assert(bootstrap.error() == st2110::Error::InvalidValue);
}

static void test_bootstrap_returns_unsupported_for_structurally_valid_but_runtime_unmapped_signaling() {
    st2110::VideoStreamSignaling signaling = make_base_signaling();
    signaling.media.depth = st2110::VideoBitDepth{10, false};

    assert(st2110::validate_video_stream_signaling(signaling) == st2110::Error::Ok);

    auto bootstrap = st2110::video_receiver_bootstrap_config_from_video_stream_signaling(
        signaling, 5004, 112, "0.0.0.0", "239.1.1.1", st2110::PartialFramePolicy::EmitWithFlag);

    assert(!bootstrap.has_value());
    assert(bootstrap.error() == st2110::Error::Unsupported);
}

int main() {
    test_valid_bootstrap_composes_all_receiver_inputs();
    test_bootstrap_preserves_absent_maxudp_override();
    test_bootstrap_rejects_invalid_signaling_before_projection();
    test_bootstrap_rejects_missing_ssn_before_projection();
    test_bootstrap_rejects_invalid_transport_inputs_after_signaling_projection();
    test_bootstrap_returns_unsupported_for_structurally_valid_but_runtime_unmapped_signaling();
    return 0;
}
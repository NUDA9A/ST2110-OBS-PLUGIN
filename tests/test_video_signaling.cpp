#include <cassert>
#include <cstdint>
#include <optional>

#include <st2110/backends/mtl/mtl_rx_video_backend.hpp>
#include <st2110/backends/socket/socket_rx_video_backend.hpp>
#include <st2110/rx_config.hpp>
#include <st2110/video_signaling.hpp>

static st2110::PtpReferenceClock make_valid_ptp_reference_clock() {
    st2110::PtpReferenceClock ptp{};
    ptp.clock_identity = {0x39, 0xA7, 0x94, 0xFF, 0xFE, 0x07, 0xCB, 0xD0};
    ptp.domain_number = 127;
    ptp.traceable = false;
    return ptp;
}

static st2110::LocalMacReferenceClock make_valid_localmac_reference_clock() {
    st2110::LocalMacReferenceClock local_mac{};
    local_mac.mac = {0x7C, 0xE9, 0xD3, 0x1B, 0x9A, 0xAF};
    return local_mac;
}

static st2110::VideoSignalStandard make_signal_standard(st2110::VideoSignalStandard::Known known) {
    return st2110::VideoSignalStandard{known, std::nullopt};
}

static st2110::VideoTransferCharacteristicSystem make_tcs(st2110::VideoTransferCharacteristicSystem::Known known) {
    return st2110::VideoTransferCharacteristicSystem{known, std::nullopt};
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

static st2110::RxVideoConfig make_interlaced_gpm_rx_config_supported_by_mtl_but_not_socket() {
    st2110::VideoReceiveCapability capability{};
    capability.media.sampling = st2110::VideoSampling{st2110::VideoSampling::Known::YCbCr422, std::nullopt};
    capability.media.width = 1920;
    capability.media.height = 1080;
    capability.media.fps_num = 30000;
    capability.media.fps_den = 1001;
    capability.media.depth = st2110::VideoBitDepth{8, false};
    capability.media.colorimetry = st2110::VideoColorimetry{st2110::VideoColorimetry::Known::Bt709, std::nullopt};
    capability.media.signal_standard =
        make_signal_standard(st2110::VideoSignalStandard::Known::St2110_20_2017);
    capability.media.range = st2110::VideoRange{st2110::VideoRange::Known::Narrow, std::nullopt};

    capability.scan_mode = st2110::VideoScanMode::Interlaced;
    capability.packing_mode = st2110::VideoPackingMode::Gpm;
    capability.transport_format = st2110::VideoTransportPayloadFormat::Rfc4175Ycbcr422_8Bit;
    capability.handoff_format = st2110::VideoFrameHandoffFormat::Uyvy;
    capability.rtp_clock = st2110::VideoReceiveRtpClock{};
    capability.topology = st2110::VideoReceiveTopology{};

    st2110::RxVideoConfig cfg{};
    cfg.width = capability.media.width;
    cfg.height = capability.media.height;
    cfg.fps_num = capability.media.fps_num;
    cfg.fps_den = capability.media.fps_den;
    cfg.udp_port = 5004;
    cfg.payload_type = 112;
    cfg.local_ip = "0.0.0.0";
    cfg.dest_ip = "239.1.1.1";
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.scan_mode = capability.scan_mode;
    cfg.packing_mode = capability.packing_mode;
    cfg.receive_capability = capability;

    return cfg;
}

static void test_valid_progressive_gpm_signaling_is_accepted() {
    st2110::VideoStreamSignaling s = make_base_signaling();

    assert(st2110::validate_video_stream_signaling(s) == st2110::Error::Ok);
}

static void test_missing_ssn_is_rejected_by_standards_clean_validation() {
    st2110::VideoStreamSignaling s = make_base_signaling();
    s.media.signal_standard = std::nullopt;

    assert(st2110::validate_video_stream_signaling(s) == st2110::Error::InvalidValue);
}

static void test_valid_bpm_signaling_is_accepted() {
    st2110::VideoStreamSignaling s = make_base_signaling();
    s.media.width = 1280;
    s.media.height = 720;
    s.media.fps_num = 60000;
    s.media.fps_den = 1001;

    s.packing_mode = st2110::VideoPackingMode::Bpm;
    s.reference_clock.kind = st2110::ReferenceClockKind::LocalMac;
    s.reference_clock.ptp = std::nullopt;
    s.reference_clock.local_mac = make_valid_localmac_reference_clock();
    s.sender_type = st2110::VideoSenderType::NarrowLinear;

    assert(st2110::validate_video_stream_signaling(s) == st2110::Error::Ok);
}

static void test_min_signaled_dimensions_are_accepted_structurally() {
    st2110::VideoStreamSignaling s = make_base_signaling();
    s.media.width = 1;
    s.media.height = 1;
    s.media.fps_num = 25;
    s.media.fps_den = 1;

    assert(st2110::validate_video_stream_signaling(s) == st2110::Error::Ok);
}

static void test_max_signaled_dimensions_are_accepted_structurally() {
    st2110::VideoStreamSignaling s = make_base_signaling();
    s.media.width = 32767;
    s.media.height = 32767;
    s.media.fps_num = 25;
    s.media.fps_den = 1;

    assert(st2110::validate_video_stream_signaling(s) == st2110::Error::Ok);
}

static void test_zero_dimensions_are_rejected_structurally() {
    {
        st2110::VideoStreamSignaling s = make_base_signaling();
        s.media.width = 0;
        s.media.height = 1080;
        s.media.fps_num = 25;
        s.media.fps_den = 1;

        assert(st2110::validate_video_stream_signaling(s) == st2110::Error::InvalidValue);
    }

    {
        st2110::VideoStreamSignaling s = make_base_signaling();
        s.media.width = 1920;
        s.media.height = 0;
        s.media.fps_num = 25;
        s.media.fps_den = 1;

        assert(st2110::validate_video_stream_signaling(s) == st2110::Error::InvalidValue);
    }
}

static void test_overflow_dimensions_are_rejected_structurally() {
    {
        st2110::VideoStreamSignaling s = make_base_signaling();
        s.media.width = 32768;
        s.media.height = 1080;
        s.media.fps_num = 25;
        s.media.fps_den = 1;

        assert(st2110::validate_video_stream_signaling(s) == st2110::Error::InvalidValue);
    }

    {
        st2110::VideoStreamSignaling s = make_base_signaling();
        s.media.width = 1920;
        s.media.height = 32768;
        s.media.fps_num = 25;
        s.media.fps_den = 1;

        assert(st2110::validate_video_stream_signaling(s) == st2110::Error::InvalidValue);
    }
}

static void test_invalid_frame_rate_is_rejected() {
    st2110::VideoStreamSignaling s = make_base_signaling();
    s.media.width = 1920;
    s.media.height = 1080;
    s.media.fps_num = 0;
    s.media.fps_den = 1;

    assert(st2110::validate_video_stream_signaling(s) == st2110::Error::InvalidValue);
}

static void test_odd_width_is_structurally_valid_but_project_rx_config_projection_is_rejected() {
    st2110::VideoStreamSignaling s = make_base_signaling();
    s.media.width = 1919;
    s.media.height = 1080;
    s.media.fps_num = 25;
    s.media.fps_den = 1;

    assert(st2110::validate_video_stream_signaling(s) == st2110::Error::Ok);

    auto cfg = st2110::rx_video_config_from_video_stream_signaling(s, 5004, 112, "0.0.0.0", "239.1.1.1");

    assert(!cfg.has_value());
    assert(cfg.error() == st2110::Error::InvalidValue);
}

static void test_interlaced_gpm_rx_config_passes_common_validation_fails_socket_support_and_passes_mtl_support() {
    st2110::RxVideoConfig cfg = make_interlaced_gpm_rx_config_supported_by_mtl_but_not_socket();

    assert(st2110::validate_rx_video_config(cfg) == st2110::Error::Ok);
    assert(st2110::validate_rx_video_config_against_project_delivery_support(
               cfg, st2110::default_video_project_delivery_support_policy()) == st2110::Error::Ok);

    assert(st2110::validate_socket_rx_video_config_support(
               cfg, st2110::default_socket_rx_video_support_policy()) == st2110::Error::Unsupported);

    assert(st2110::validate_mtl_rx_video_config_support(
               cfg, st2110::default_mtl_rx_video_support_policy()) == st2110::Error::Ok);
}

static void test_standard_sized_maxudp_is_accepted() {
    st2110::VideoStreamSignaling s = make_base_signaling();
    s.media.width = 1920;
    s.media.height = 1080;
    s.media.fps_num = 25;
    s.media.fps_den = 1;
    s.max_udp_datagram_bytes = st2110::standardUdpDatagramSizeLimitBytes;

    assert(st2110::validate_video_stream_signaling(s) == st2110::Error::Ok);
}

static void test_extended_sized_maxudp_is_accepted() {
    st2110::VideoStreamSignaling s = make_base_signaling();
    s.media.width = 1920;
    s.media.height = 1080;
    s.media.fps_num = 25;
    s.media.fps_den = 1;
    s.max_udp_datagram_bytes = st2110::extendedUdpDatagramSizeLimitBytes;

    assert(st2110::validate_video_stream_signaling(s) == st2110::Error::Ok);
}

static void test_non_boundary_maxudp_is_rejected() {
    st2110::VideoStreamSignaling s = make_base_signaling();
    s.media.width = 1920;
    s.media.height = 1080;
    s.media.fps_num = 25;
    s.media.fps_den = 1;
    s.max_udp_datagram_bytes = 4096;

    assert(st2110::validate_video_stream_signaling(s) == st2110::Error::InvalidValue);
}

static void test_above_extended_maxudp_is_rejected() {
    st2110::VideoStreamSignaling s = make_base_signaling();
    s.media.width = 1920;
    s.media.height = 1080;
    s.media.fps_num = 25;
    s.media.fps_den = 1;
    s.max_udp_datagram_bytes = st2110::extendedUdpDatagramSizeLimitBytes + 1;

    assert(st2110::validate_video_stream_signaling(s) == st2110::Error::InvalidValue);
}

static void test_standard_maxudp_policy_is_derived_from_signaling() {
    st2110::VideoStreamSignaling s{};
    s.max_udp_datagram_bytes = st2110::standardUdpDatagramSizeLimitBytes;

    st2110::PacketParsePolicy p = st2110::packet_parse_policy_from_video_stream_signaling(s);

    assert(p.max_udp_datagram_bytes.has_value());
    assert(*p.max_udp_datagram_bytes == st2110::standardUdpDatagramSizeLimitBytes);
}

static void test_extended_maxudp_policy_is_derived_from_signaling() {
    st2110::VideoStreamSignaling s{};
    s.max_udp_datagram_bytes = st2110::extendedUdpDatagramSizeLimitBytes;

    st2110::PacketParsePolicy p = st2110::packet_parse_policy_from_video_stream_signaling(s);

    assert(p.max_udp_datagram_bytes.has_value());
    assert(*p.max_udp_datagram_bytes == st2110::extendedUdpDatagramSizeLimitBytes);
}

static void test_absent_maxudp_produces_empty_policy_override() {
    st2110::VideoStreamSignaling s{};

    st2110::PacketParsePolicy p = st2110::packet_parse_policy_from_video_stream_signaling(s);

    assert(!p.max_udp_datagram_bytes.has_value());
}

static void test_unsupported_sampling_is_structurally_valid_but_not_runtime_mappable() {
    st2110::VideoStreamSignaling s = make_base_signaling();
    s.media.sampling = st2110::VideoSampling{st2110::VideoSampling::Known::RGB, std::nullopt};

    assert(st2110::validate_video_stream_signaling(s) == st2110::Error::Ok);

    auto projected = st2110::pixel_format_from_video_stream_signaling(s);
    assert(!projected.has_value());
    assert(projected.error() == st2110::Error::Unsupported);
}

static void test_rgb8_capability_is_structurally_valid_but_project_rx_config_projection_is_unsupported() {
    st2110::VideoStreamSignaling s = make_base_signaling();
    s.media.sampling = st2110::VideoSampling{st2110::VideoSampling::Known::RGB, std::nullopt};
    s.media.depth = st2110::VideoBitDepth{8, false};

    st2110::VideoReceiveCapabilityProjectionOptions options{};
    options.handoff_format = st2110::VideoFrameHandoffFormat::Rgb8;

    auto capability = st2110::video_receive_capability_from_video_stream_signaling(s, options);
    assert(capability.has_value());
    assert(capability->media.sampling.known == st2110::VideoSampling::Known::RGB);
    assert(capability->handoff_format == st2110::VideoFrameHandoffFormat::Rgb8);
    assert(capability->transport_format == st2110::VideoTransportPayloadFormat::Rfc4175Rgb_8Bit);

    auto cfg = st2110::rx_video_config_from_video_stream_signaling(
        s, 5004, 112, "0.0.0.0", "239.1.1.1", options);

    assert(!cfg.has_value());
    assert(cfg.error() == st2110::Error::Unsupported);
}

static void test_bt709_sdr_with_st2110_20_2017_is_accepted() {
    st2110::VideoStreamSignaling s = make_base_signaling();
    s.media.transfer_characteristic_system = make_tcs(st2110::VideoTransferCharacteristicSystem::Known::SDR);
    s.media.signal_standard = make_signal_standard(st2110::VideoSignalStandard::Known::St2110_20_2017);

    assert(st2110::validate_video_stream_signaling(s) == st2110::Error::Ok);
}

static void test_bt709_sdr_with_st2110_20_2022_is_accepted() {
    st2110::VideoStreamSignaling s = make_base_signaling();
    s.media.transfer_characteristic_system = make_tcs(st2110::VideoTransferCharacteristicSystem::Known::SDR);
    s.media.signal_standard = make_signal_standard(st2110::VideoSignalStandard::Known::St2110_20_2022);

    assert(st2110::validate_video_stream_signaling(s) == st2110::Error::Ok);
}

static void test_alpha_requires_st2110_20_2022_and_runtime_projection_remains_localized() {
    {
        st2110::VideoStreamSignaling s = make_base_signaling();
        s.media.sampling = st2110::VideoSampling{st2110::VideoSampling::Known::Key, std::nullopt};
        s.media.colorimetry = st2110::VideoColorimetry{st2110::VideoColorimetry::Known::Alpha, std::nullopt};
        s.media.transfer_characteristic_system = std::nullopt;
        s.media.signal_standard = make_signal_standard(st2110::VideoSignalStandard::Known::St2110_20_2017);

        assert(st2110::validate_video_stream_signaling(s) == st2110::Error::InvalidValue);
    }

    {
        st2110::VideoStreamSignaling s = make_base_signaling();
        s.media.sampling = st2110::VideoSampling{st2110::VideoSampling::Known::Key, std::nullopt};
        s.media.colorimetry = st2110::VideoColorimetry{st2110::VideoColorimetry::Known::Alpha, std::nullopt};
        s.media.transfer_characteristic_system = std::nullopt;
        s.media.signal_standard = make_signal_standard(st2110::VideoSignalStandard::Known::St2110_20_2022);

        assert(st2110::validate_video_stream_signaling(s) == st2110::Error::Ok);

        auto projected = st2110::pixel_format_from_video_stream_signaling(s);
        assert(!projected.has_value());
        assert(projected.error() == st2110::Error::Unsupported);
    }
}

static void test_st2115logs3_requires_st2110_20_2022() {
    {
        st2110::VideoStreamSignaling s = make_base_signaling();
        s.media.transfer_characteristic_system =
            make_tcs(st2110::VideoTransferCharacteristicSystem::Known::St2115LogS3);
        s.media.signal_standard = make_signal_standard(st2110::VideoSignalStandard::Known::St2110_20_2017);

        assert(st2110::validate_video_stream_signaling(s) == st2110::Error::InvalidValue);
    }

    {
        st2110::VideoStreamSignaling s = make_base_signaling();
        s.media.transfer_characteristic_system =
            make_tcs(st2110::VideoTransferCharacteristicSystem::Known::St2115LogS3);
        s.media.signal_standard = make_signal_standard(st2110::VideoSignalStandard::Known::St2110_20_2022);

        assert(st2110::validate_video_stream_signaling(s) == st2110::Error::Ok);
    }
}

int main() {
    test_valid_progressive_gpm_signaling_is_accepted();
    test_missing_ssn_is_rejected_by_standards_clean_validation();
    test_valid_bpm_signaling_is_accepted();
    test_min_signaled_dimensions_are_accepted_structurally();
    test_max_signaled_dimensions_are_accepted_structurally();
    test_zero_dimensions_are_rejected_structurally();
    test_overflow_dimensions_are_rejected_structurally();
    test_invalid_frame_rate_is_rejected();
    test_odd_width_is_structurally_valid_but_project_rx_config_projection_is_rejected();
    test_interlaced_gpm_rx_config_passes_common_validation_fails_socket_support_and_passes_mtl_support();

    test_standard_sized_maxudp_is_accepted();
    test_extended_sized_maxudp_is_accepted();
    test_non_boundary_maxudp_is_rejected();
    test_above_extended_maxudp_is_rejected();
    test_standard_maxudp_policy_is_derived_from_signaling();
    test_extended_maxudp_policy_is_derived_from_signaling();
    test_absent_maxudp_produces_empty_policy_override();

    test_unsupported_sampling_is_structurally_valid_but_not_runtime_mappable();
    test_rgb8_capability_is_structurally_valid_but_project_rx_config_projection_is_unsupported();

    test_bt709_sdr_with_st2110_20_2017_is_accepted();
    test_bt709_sdr_with_st2110_20_2022_is_accepted();
    test_alpha_requires_st2110_20_2022_and_runtime_projection_remains_localized();
    test_st2115logs3_requires_st2110_20_2022();

    return 0;
}
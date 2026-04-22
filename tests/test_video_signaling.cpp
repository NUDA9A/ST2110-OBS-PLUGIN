#include <cassert>
#include <cstdint>

#include <st2110/video_signaling.hpp>

static void test_valid_progressive_gpm_signaling_is_accepted() {
    st2110::VideoStreamSignaling s{};
    s.format = st2110::PixelFormat::UYVY;
    s.scan_mode = st2110::VideoScanMode::Progressive;
    s.width = 1920;
    s.height = 1080;
    s.fps_num = 30000;
    s.fps_den = 1001;

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

    assert(st2110::validate_video_stream_signaling(s) == st2110::Error::Ok);
}

static void test_valid_bpm_signaling_is_accepted() {
    st2110::VideoStreamSignaling s{};
    s.format = st2110::PixelFormat::UYVY;
    s.scan_mode = st2110::VideoScanMode::Progressive;
    s.width = 1280;
    s.height = 720;
    s.fps_num = 60000;
    s.fps_den = 1001;

    s.packing_mode = st2110::VideoPackingMode::Bpm;
    s.max_udp_datagram_bytes = st2110::standardUdpDatagramSizeLimitBytes;

    s.media_clock_mode = st2110::MediaClockMode::Direct;
    s.timestamp_mode = st2110::TimestampMode::New;
    s.reference_clock.kind = st2110::ReferenceClockKind::LocalMac;
    s.reference_clock.local_mac = st2110::LocalMacReferenceClock{};
    s.ts_delay_sender_ticks = 0;

    s.sender_type = st2110::VideoSenderType::NarrowLinear;

    assert(st2110::validate_video_stream_signaling(s) == st2110::Error::Ok);
}

static void test_invalid_dimensions_are_rejected() {
    st2110::VideoStreamSignaling s{};
    s.format = st2110::PixelFormat::UYVY;
    s.scan_mode = st2110::VideoScanMode::Progressive;
    s.width = 0;
    s.height = 1080;
    s.fps_num = 25;
    s.fps_den = 1;

    s.reference_clock.kind = st2110::ReferenceClockKind::Ptp;
    s.reference_clock.ptp = st2110::PtpReferenceClock{};

    assert(st2110::validate_video_stream_signaling(s) == st2110::Error::InvalidValue);
}

static void test_invalid_frame_rate_is_rejected() {
    st2110::VideoStreamSignaling s{};
    s.format = st2110::PixelFormat::UYVY;
    s.scan_mode = st2110::VideoScanMode::Progressive;
    s.width = 1920;
    s.height = 1080;
    s.fps_num = 0;
    s.fps_den = 1;

    s.reference_clock.kind = st2110::ReferenceClockKind::Ptp;
    s.reference_clock.ptp = st2110::PtpReferenceClock{};

    assert(st2110::validate_video_stream_signaling(s) == st2110::Error::InvalidValue);
}

static void test_uyvy_odd_width_is_rejected() {
    st2110::VideoStreamSignaling s{};
    s.format = st2110::PixelFormat::UYVY;
    s.scan_mode = st2110::VideoScanMode::Progressive;
    s.width = 1919;
    s.height = 1080;
    s.fps_num = 25;
    s.fps_den = 1;

    s.reference_clock.kind = st2110::ReferenceClockKind::Ptp;
    s.reference_clock.ptp = st2110::PtpReferenceClock{};

    assert(st2110::validate_video_stream_signaling(s) == st2110::Error::InvalidValue);
}

static void test_invalid_maxudp_config_is_rejected() {
    st2110::VideoStreamSignaling s{};
    s.format = st2110::PixelFormat::UYVY;
    s.scan_mode = st2110::VideoScanMode::Progressive;
    s.width = 1920;
    s.height = 1080;
    s.fps_num = 25;
    s.fps_den = 1;

    s.reference_clock.kind = st2110::ReferenceClockKind::Ptp;
    s.reference_clock.ptp = st2110::PtpReferenceClock{};

    s.max_udp_datagram_bytes = 8; // smaller than min parsable datagram

    assert(st2110::validate_video_stream_signaling(s) == st2110::Error::InvalidValue);
}

static void test_packet_parse_policy_is_derived_from_signaling() {
    st2110::VideoStreamSignaling s{};
    s.max_udp_datagram_bytes = 4096;

    st2110::PacketParsePolicy p =
            st2110::packet_parse_policy_from_video_stream_signaling(s);

    assert(p.max_udp_datagram_bytes.has_value());
    assert(*p.max_udp_datagram_bytes == 4096u);
}

static void test_absent_maxudp_produces_empty_policy_override() {
    st2110::VideoStreamSignaling s{};

    st2110::PacketParsePolicy p =
            st2110::packet_parse_policy_from_video_stream_signaling(s);

    assert(!p.max_udp_datagram_bytes.has_value());
}

int main() {
    test_valid_progressive_gpm_signaling_is_accepted();
    test_valid_bpm_signaling_is_accepted();
    test_invalid_dimensions_are_rejected();
    test_invalid_frame_rate_is_rejected();
    test_uyvy_odd_width_is_rejected();
    test_invalid_maxudp_config_is_rejected();
    test_packet_parse_policy_is_derived_from_signaling();
    test_absent_maxudp_produces_empty_policy_override();
    return 0;
}
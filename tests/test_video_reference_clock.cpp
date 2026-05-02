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

static st2110::PtpReferenceClock make_traceable_ptp_reference_clock() {
    st2110::PtpReferenceClock ptp{};
    ptp.clock_identity = {};
    ptp.domain_number = 0;
    ptp.traceable = true;
    return ptp;
}

static st2110::LocalMacReferenceClock make_valid_localmac_reference_clock() {
    st2110::LocalMacReferenceClock local_mac{};
    local_mac.mac = {0x7C, 0xE9, 0xD3, 0x1B, 0x9A, 0xAF};
    return local_mac;
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

    s.scan_mode = st2110::VideoScanMode::Progressive;
    s.packing_mode = st2110::VideoPackingMode::Gpm;
    s.media_clock_mode = st2110::MediaClockMode::Direct;
    s.timestamp_mode = st2110::TimestampMode::New;
    s.ts_delay_sender_ticks = 0;
    s.sender_type = st2110::VideoSenderType::Narrow;

    return s;
}

static void test_validate_reference_clock_ptp_ok() {
    st2110::ReferenceClock clock{};
    clock.kind = st2110::ReferenceClockKind::Ptp;
    clock.ptp = make_valid_ptp_reference_clock();
    assert(st2110::validate_reference_clock(clock) == st2110::Error::Ok);
}

static void test_validate_reference_clock_ptp_traceable_ok() {
    st2110::ReferenceClock clock{};
    clock.kind = st2110::ReferenceClockKind::Ptp;
    clock.ptp = make_traceable_ptp_reference_clock();
    assert(st2110::validate_reference_clock(clock) == st2110::Error::Ok);
}

static void test_validate_reference_clock_ptp_missing_payload_is_invalid() {
    st2110::ReferenceClock clock{};
    clock.kind = st2110::ReferenceClockKind::Ptp;
    assert(st2110::validate_reference_clock(clock) == st2110::Error::InvalidValue);
}

static void test_validate_reference_clock_ptp_zero_identity_without_traceable_is_invalid() {
    st2110::ReferenceClock clock{};
    clock.kind = st2110::ReferenceClockKind::Ptp;
    clock.ptp = st2110::PtpReferenceClock{};
    assert(st2110::validate_reference_clock(clock) == st2110::Error::InvalidValue);
}

static void test_validate_reference_clock_ptp_with_localmac_is_invalid() {
    st2110::ReferenceClock clock{};
    clock.kind = st2110::ReferenceClockKind::Ptp;
    clock.ptp = make_valid_ptp_reference_clock();
    clock.local_mac = make_valid_localmac_reference_clock();
    assert(st2110::validate_reference_clock(clock) == st2110::Error::InvalidValue);
}

static void test_validate_reference_clock_ptp_with_raw_token_is_invalid() {
    st2110::ReferenceClock clock{};
    clock.kind = st2110::ReferenceClockKind::Ptp;
    clock.ptp = make_valid_ptp_reference_clock();
    clock.raw_token = std::string("ts-refclk:ptp=IEEE1588-2008:...");
    assert(st2110::validate_reference_clock(clock) == st2110::Error::InvalidValue);
}

static void test_validate_reference_clock_localmac_ok() {
    st2110::ReferenceClock clock{};
    clock.kind = st2110::ReferenceClockKind::LocalMac;
    clock.local_mac = make_valid_localmac_reference_clock();
    assert(st2110::validate_reference_clock(clock) == st2110::Error::Ok);
}

static void test_validate_reference_clock_localmac_missing_payload_is_invalid() {
    st2110::ReferenceClock clock{};
    clock.kind = st2110::ReferenceClockKind::LocalMac;
    assert(st2110::validate_reference_clock(clock) == st2110::Error::InvalidValue);
}

static void test_validate_reference_clock_localmac_zero_mac_is_invalid() {
    st2110::ReferenceClock clock{};
    clock.kind = st2110::ReferenceClockKind::LocalMac;
    clock.local_mac = st2110::LocalMacReferenceClock{};
    assert(st2110::validate_reference_clock(clock) == st2110::Error::InvalidValue);
}

static void test_validate_reference_clock_localmac_with_ptp_is_invalid() {
    st2110::ReferenceClock clock{};
    clock.kind = st2110::ReferenceClockKind::LocalMac;
    clock.local_mac = make_valid_localmac_reference_clock();
    clock.ptp = make_valid_ptp_reference_clock();
    assert(st2110::validate_reference_clock(clock) == st2110::Error::InvalidValue);
}

static void test_validate_reference_clock_other_ok() {
    st2110::ReferenceClock clock{};
    clock.kind = st2110::ReferenceClockKind::Other;
    clock.raw_token = std::string("clk:some-future-format");
    assert(st2110::validate_reference_clock(clock) == st2110::Error::Ok);
}

static void test_validate_reference_clock_other_missing_token_is_invalid() {
    st2110::ReferenceClock clock{};
    clock.kind = st2110::ReferenceClockKind::Other;
    assert(st2110::validate_reference_clock(clock) == st2110::Error::InvalidValue);
}

static void test_validate_reference_clock_other_empty_token_is_invalid() {
    st2110::ReferenceClock clock{};
    clock.kind = st2110::ReferenceClockKind::Other;
    clock.raw_token = std::string("");
    assert(st2110::validate_reference_clock(clock) == st2110::Error::InvalidValue);
}

static void test_validate_reference_clock_other_with_ptp_is_invalid() {
    st2110::ReferenceClock clock{};
    clock.kind = st2110::ReferenceClockKind::Other;
    clock.raw_token = std::string("clk:some-future-format");
    clock.ptp = make_valid_ptp_reference_clock();
    assert(st2110::validate_reference_clock(clock) == st2110::Error::InvalidValue);
}

static void test_video_stream_signaling_uses_reference_clock_validation() {
    st2110::VideoStreamSignaling s = make_base_signaling();
    s.reference_clock.kind = st2110::ReferenceClockKind::Ptp;
    s.reference_clock.ptp = make_valid_ptp_reference_clock();

    assert(st2110::validate_video_stream_signaling(s) == st2110::Error::Ok);
}

static void test_video_stream_signaling_accepts_traceable_reference_clock() {
    st2110::VideoStreamSignaling s = make_base_signaling();
    s.reference_clock.kind = st2110::ReferenceClockKind::Ptp;
    s.reference_clock.ptp = make_traceable_ptp_reference_clock();

    assert(st2110::validate_video_stream_signaling(s) == st2110::Error::Ok);
}

static void test_video_stream_signaling_rejects_invalid_reference_clock() {
    st2110::VideoStreamSignaling s = make_base_signaling();
    s.reference_clock.kind = st2110::ReferenceClockKind::Ptp;
    s.reference_clock.ptp = std::nullopt;
    s.reference_clock.local_mac = make_valid_localmac_reference_clock();

    assert(st2110::validate_video_stream_signaling(s) == st2110::Error::InvalidValue);
}

int main() {
    test_validate_reference_clock_ptp_ok();
    test_validate_reference_clock_ptp_traceable_ok();
    test_validate_reference_clock_ptp_missing_payload_is_invalid();
    test_validate_reference_clock_ptp_zero_identity_without_traceable_is_invalid();
    test_validate_reference_clock_ptp_with_localmac_is_invalid();
    test_validate_reference_clock_ptp_with_raw_token_is_invalid();

    test_validate_reference_clock_localmac_ok();
    test_validate_reference_clock_localmac_missing_payload_is_invalid();
    test_validate_reference_clock_localmac_zero_mac_is_invalid();
    test_validate_reference_clock_localmac_with_ptp_is_invalid();

    test_validate_reference_clock_other_ok();
    test_validate_reference_clock_other_missing_token_is_invalid();
    test_validate_reference_clock_other_empty_token_is_invalid();
    test_validate_reference_clock_other_with_ptp_is_invalid();

    test_video_stream_signaling_uses_reference_clock_validation();
    test_video_stream_signaling_accepts_traceable_reference_clock();
    test_video_stream_signaling_rejects_invalid_reference_clock();
    return 0;
}
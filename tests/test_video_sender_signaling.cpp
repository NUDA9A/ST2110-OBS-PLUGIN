#include <cassert>
#include <optional>

#include <st2110/video_signaling.hpp>

static st2110::VideoStreamSignaling make_base_signaling() {
    st2110::VideoStreamSignaling s{};

    s.media.sampling = st2110::VideoSampling{
            st2110::VideoSampling::Known::YCbCr422,
            std::nullopt};
    s.media.width = 1920;
    s.media.height = 1080;
    s.media.fps_num = 30000;
    s.media.fps_den = 1001;
    s.media.depth = st2110::VideoBitDepth{8, false};
    s.media.colorimetry = st2110::VideoColorimetry{
            st2110::VideoColorimetry::Known::Bt709,
            std::nullopt};

    s.scan_mode = st2110::VideoScanMode::Progressive;
    s.packing_mode = st2110::VideoPackingMode::Gpm;

    s.media_clock_mode = st2110::MediaClockMode::Direct;
    s.timestamp_mode = st2110::TimestampMode::New;

    s.reference_clock.kind = st2110::ReferenceClockKind::Ptp;
    s.reference_clock.ptp = st2110::PtpReferenceClock{};

    s.ts_delay_sender_ticks = 0;

    return s;
}

static void test_validate_video_sender_signaling_narrow_ok() {
    assert(st2110::validate_video_sender_signaling(
            st2110::VideoSenderType::Narrow,
            std::nullopt,
            std::nullopt) == st2110::Error::Ok);
}

static void test_validate_video_sender_signaling_narrow_rejects_troff() {
    assert(st2110::validate_video_sender_signaling(
            st2110::VideoSenderType::Narrow,
            10u,
            std::nullopt) == st2110::Error::InvalidValue);
}

static void test_validate_video_sender_signaling_narrow_rejects_cmax() {
    assert(st2110::validate_video_sender_signaling(
            st2110::VideoSenderType::Narrow,
            std::nullopt,
            1u) == st2110::Error::InvalidValue);
}

static void test_validate_video_sender_signaling_narrow_linear_ok() {
    assert(st2110::validate_video_sender_signaling(
            st2110::VideoSenderType::NarrowLinear,
            std::nullopt,
            std::nullopt) == st2110::Error::Ok);
}

static void test_validate_video_sender_signaling_narrow_linear_rejects_troff() {
    assert(st2110::validate_video_sender_signaling(
            st2110::VideoSenderType::NarrowLinear,
            10u,
            std::nullopt) == st2110::Error::InvalidValue);
}

static void test_validate_video_sender_signaling_narrow_linear_rejects_cmax() {
    assert(st2110::validate_video_sender_signaling(
            st2110::VideoSenderType::NarrowLinear,
            std::nullopt,
            1u) == st2110::Error::InvalidValue);
}

static void test_validate_video_sender_signaling_wide_requires_cmax() {
    assert(st2110::validate_video_sender_signaling(
            st2110::VideoSenderType::Wide,
            std::nullopt,
            std::nullopt) == st2110::Error::InvalidValue);
}

static void test_validate_video_sender_signaling_wide_accepts_positive_cmax() {
    assert(st2110::validate_video_sender_signaling(
            st2110::VideoSenderType::Wide,
            std::nullopt,
            4u) == st2110::Error::Ok);
}

static void test_validate_video_sender_signaling_wide_rejects_zero_cmax() {
    assert(st2110::validate_video_sender_signaling(
            st2110::VideoSenderType::Wide,
            std::nullopt,
            0u) == st2110::Error::InvalidValue);
}

static void test_validate_video_sender_signaling_wide_rejects_troff() {
    assert(st2110::validate_video_sender_signaling(
            st2110::VideoSenderType::Wide,
            10u,
            4u) == st2110::Error::InvalidValue);
}

static void test_validate_video_stream_signaling_accepts_valid_sender_signaling() {
    st2110::VideoStreamSignaling s = make_base_signaling();
    s.sender_type = st2110::VideoSenderType::Wide;
    s.cmax = 4u;
    s.troff_us = std::nullopt;

    assert(st2110::validate_video_stream_signaling(s) == st2110::Error::Ok);
}

static void test_validate_video_stream_signaling_rejects_invalid_sender_signaling() {
    st2110::VideoStreamSignaling s = make_base_signaling();
    s.sender_type = st2110::VideoSenderType::Wide;
    s.cmax = std::nullopt;
    s.troff_us = std::nullopt;

    assert(st2110::validate_video_stream_signaling(s) == st2110::Error::InvalidValue);
}

int main() {
    test_validate_video_sender_signaling_narrow_ok();
    test_validate_video_sender_signaling_narrow_rejects_troff();
    test_validate_video_sender_signaling_narrow_rejects_cmax();

    test_validate_video_sender_signaling_narrow_linear_ok();
    test_validate_video_sender_signaling_narrow_linear_rejects_troff();
    test_validate_video_sender_signaling_narrow_linear_rejects_cmax();

    test_validate_video_sender_signaling_wide_requires_cmax();
    test_validate_video_sender_signaling_wide_accepts_positive_cmax();
    test_validate_video_sender_signaling_wide_rejects_zero_cmax();
    test_validate_video_sender_signaling_wide_rejects_troff();

    test_validate_video_stream_signaling_accepts_valid_sender_signaling();
    test_validate_video_stream_signaling_rejects_invalid_sender_signaling();
    return 0;
}
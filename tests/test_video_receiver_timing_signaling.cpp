#include <cassert>
#include <cstdint>

#include <st2110/video_receiver_timing_signaling.hpp>

static st2110::ReferenceClock make_valid_ptp_reference_clock() {
    st2110::ReferenceClock clock{};
    clock.kind = st2110::ReferenceClockKind::Ptp;

    st2110::PtpReferenceClock ptp{};
    ptp.clock_identity = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
    ptp.domain_number = 127;
    ptp.traceable = true;

    clock.ptp = ptp;
    clock.local_mac.reset();
    clock.raw_token.reset();
    return clock;
}

static st2110::VideoStreamSignaling make_valid_signaling() {
    st2110::VideoStreamSignaling signaling{};

    signaling.media.sampling.known = st2110::VideoSampling::Known::YCbCr422;
    signaling.media.width = 1920;
    signaling.media.height = 1080;
    signaling.media.fps_num = 25;
    signaling.media.fps_den = 1;
    signaling.media.depth.bits = 8;
    signaling.media.depth.floating_point = false;
    signaling.media.colorimetry.known = st2110::VideoColorimetry::Known::Bt709;

    signaling.scan_mode = st2110::VideoScanMode::Progressive;
    signaling.packing_mode = st2110::VideoPackingMode::Gpm;

    signaling.media_clock_mode = st2110::MediaClockMode::Direct;
    signaling.timestamp_mode = st2110::TimestampMode::New;
    signaling.reference_clock = make_valid_ptp_reference_clock();
    signaling.ts_delay_sender_ticks = 0;

    signaling.sender_type = st2110::VideoSenderType::Narrow;
    signaling.troff_us.reset();
    signaling.cmax.reset();

    return signaling;
}

static st2110::VideoReceiverTimingConfig make_valid_timing_config() { return st2110::VideoReceiverTimingConfig{}; }

static void test_valid_consistency_check_accepts_supported_sender() {
    const auto cfg = make_valid_timing_config();
    const auto signaling = make_valid_signaling();

    assert(st2110::validate_video_receiver_timing_against_video_stream_signaling(cfg, signaling) == st2110::Error::Ok);
}

static void test_invalid_receiver_timing_config_is_rejected_first() {
    st2110::VideoReceiverTimingConfig cfg{};
    cfg.capability.supports_type_n = false;
    cfg.capability.supports_type_nl = false;
    cfg.capability.supports_type_w = false;

    const auto signaling = make_valid_signaling();

    assert(st2110::validate_video_receiver_timing_against_video_stream_signaling(cfg, signaling) ==
           st2110::Error::InvalidValue);
}

static void test_unsupported_sender_type_is_rejected() {
    auto cfg = make_valid_timing_config();
    cfg.capability.supports_type_w = false;

    auto signaling = make_valid_signaling();
    signaling.sender_type = st2110::VideoSenderType::Wide;
    signaling.cmax = 16;

    assert(st2110::validate_video_receiver_timing_against_video_stream_signaling(cfg, signaling) ==
           st2110::Error::Unsupported);
}

static void test_nonzero_ts_delay_requires_consumer_support() {
    auto cfg = make_valid_timing_config();
    cfg.requirements.consume_ts_delay = false;

    auto signaling = make_valid_signaling();
    signaling.ts_delay_sender_ticks = 1234;

    assert(st2110::validate_video_receiver_timing_against_video_stream_signaling(cfg, signaling) ==
           st2110::Error::Unsupported);
}

static void test_wide_sender_cmax_requires_consumer_support() {
    auto cfg = make_valid_timing_config();
    cfg.requirements.consume_sender_cmax = false;

    auto signaling = make_valid_signaling();
    signaling.sender_type = st2110::VideoSenderType::Wide;
    signaling.cmax = 32;

    assert(st2110::validate_video_receiver_timing_against_video_stream_signaling(cfg, signaling) ==
           st2110::Error::Unsupported);
}

static void test_invalid_reference_clock_is_checked_when_required() {
    auto cfg = make_valid_timing_config();

    auto signaling = make_valid_signaling();
    signaling.reference_clock = st2110::ReferenceClock{};
    signaling.reference_clock.kind = st2110::ReferenceClockKind::Ptp;
    signaling.reference_clock.ptp.reset();
    signaling.reference_clock.local_mac.reset();
    signaling.reference_clock.raw_token.reset();

    assert(st2110::validate_video_receiver_timing_against_video_stream_signaling(cfg, signaling) ==
           st2110::Error::InvalidValue);
}

static void test_invalid_reference_clock_may_be_skipped_when_not_required() {
    auto cfg = make_valid_timing_config();
    cfg.requirements.require_reference_clock = false;

    auto signaling = make_valid_signaling();
    signaling.reference_clock = st2110::ReferenceClock{};
    signaling.reference_clock.kind = st2110::ReferenceClockKind::Ptp;
    signaling.reference_clock.ptp.reset();
    signaling.reference_clock.local_mac.reset();
    signaling.reference_clock.raw_token.reset();

    assert(st2110::validate_video_receiver_timing_against_video_stream_signaling(cfg, signaling) == st2110::Error::Ok);
}

int main() {
    test_valid_consistency_check_accepts_supported_sender();
    test_invalid_receiver_timing_config_is_rejected_first();
    test_unsupported_sender_type_is_rejected();
    test_nonzero_ts_delay_requires_consumer_support();
    test_wide_sender_cmax_requires_consumer_support();
    test_invalid_reference_clock_is_checked_when_required();
    test_invalid_reference_clock_may_be_skipped_when_not_required();
    return 0;
}
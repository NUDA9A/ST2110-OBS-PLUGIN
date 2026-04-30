#include <cassert>

#include <st2110/error.hpp>
#include <st2110/video_receiver_timing.hpp>

static void test_default_capability_has_supported_sender_types() {
    const st2110::VideoReceiverTimingCapability capability{};

    assert(st2110::has_any_supported_video_sender_type(capability));
    assert(st2110::validate_video_receiver_timing_capability(capability) == st2110::Error::Ok);
}

static void test_capability_with_no_supported_sender_types_is_invalid() {
    st2110::VideoReceiverTimingCapability capability{};
    capability.supports_type_n = false;
    capability.supports_type_nl = false;
    capability.supports_type_w = false;

    assert(!st2110::has_any_supported_video_sender_type(capability));
    assert(st2110::validate_video_receiver_timing_capability(capability) == st2110::Error::InvalidValue);
}

static void test_config_validation_delegates_to_capability_validation() {
    st2110::VideoReceiverTimingConfig cfg{};
    cfg.capability.supports_type_n = false;
    cfg.capability.supports_type_nl = false;
    cfg.capability.supports_type_w = false;

    assert(st2110::validate_video_receiver_timing_config(cfg) == st2110::Error::InvalidValue);
}

static void test_config_can_carry_explicit_timing_requirements() {
    st2110::VideoReceiverTimingConfig cfg{};
    cfg.capability.supports_type_n = true;
    cfg.capability.supports_type_nl = false;
    cfg.capability.supports_type_w = false;

    cfg.requirements.require_reference_clock = true;
    cfg.requirements.require_media_clock = true;
    cfg.requirements.require_timestamp_mode = true;
    cfg.requirements.consume_ts_delay = true;
    cfg.requirements.consume_sender_troff = true;
    cfg.requirements.consume_sender_cmax = true;

    assert(st2110::validate_video_receiver_timing_config(cfg) == st2110::Error::Ok);
}

static void test_config_can_also_carry_relaxed_requirements_without_becoming_invalid() {
    st2110::VideoReceiverTimingConfig cfg{};
    cfg.capability.supports_type_n = false;
    cfg.capability.supports_type_nl = true;
    cfg.capability.supports_type_w = false;

    cfg.requirements.require_reference_clock = false;
    cfg.requirements.require_media_clock = false;
    cfg.requirements.require_timestamp_mode = false;
    cfg.requirements.consume_ts_delay = false;
    cfg.requirements.consume_sender_troff = false;
    cfg.requirements.consume_sender_cmax = false;

    assert(st2110::validate_video_receiver_timing_config(cfg) == st2110::Error::Ok);
}

int main() {
    test_default_capability_has_supported_sender_types();
    test_capability_with_no_supported_sender_types_is_invalid();
    test_config_validation_delegates_to_capability_validation();
    test_config_can_carry_explicit_timing_requirements();
    test_config_can_also_carry_relaxed_requirements_without_becoming_invalid();
    return 0;
}
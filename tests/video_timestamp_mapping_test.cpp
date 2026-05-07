#include "st2110/receive/video/video_timestamp_mapping.hpp"

#include <cassert>
#include <cstdint>

using namespace st2110;

namespace {

void rtp_mapper_default_mode_maps_first_observed_to_local_zero() {
    VideoRtpTimestampMapperConfig cfg{};
    cfg.rtp_clock_rate = 90000;

    VideoRtpTimestampMapper mapper{cfg};

    auto first = mapper.map(12345);

    assert(first.has_value());
    assert(*first == 0);

    auto second = mapper.map(12345 + 90000);

    assert(second.has_value());
    assert(*second == 1000000000ULL);
}

void rtp_mapper_maps_anchor_to_anchor_timestamp_in_configured_reference_mode() {
    VideoRtpTimestampMapperConfig cfg{};
    cfg.rtp_clock_rate = 90000;
    cfg.initial_anchor_mode = RtpTimestampInitialAnchorMode::ConfiguredReference;
    cfg.anchor_rtp_timestamp = 12345;
    cfg.anchor_timestamp_ns = 777;

    VideoRtpTimestampMapper mapper{cfg};

    auto mapped = mapper.map(12345);

    assert(mapped.has_value());
    assert(*mapped == 777);
}

void rtp_mapper_maps_90000_ticks_to_one_second_in_configured_reference_mode() {
    VideoRtpTimestampMapperConfig cfg{};
    cfg.rtp_clock_rate = 90000;
    cfg.initial_anchor_mode = RtpTimestampInitialAnchorMode::ConfiguredReference;
    cfg.anchor_rtp_timestamp = 1000;
    cfg.anchor_timestamp_ns = 5000000000ULL;

    VideoRtpTimestampMapper mapper{cfg};

    auto mapped = mapper.map(91000);

    assert(mapped.has_value());
    assert(*mapped == 6000000000ULL);
}

void rtp_mapper_maps_progressive_25fps_tick_step_from_first_observed_zero() {
    VideoRtpTimestampMapperConfig cfg{};
    cfg.rtp_clock_rate = 90000;

    VideoRtpTimestampMapper mapper{cfg};

    auto first = mapper.map(50000);
    assert(first.has_value());
    assert(*first == 0);

    // 90000 / 25 = 3600 RTP ticks.
    auto second = mapper.map(50000 + 3600);

    assert(second.has_value());
    assert(*second == 40000000ULL);
}

void rtp_mapper_handles_32bit_wraparound_in_configured_reference_mode() {
    VideoRtpTimestampMapperConfig cfg{};
    cfg.rtp_clock_rate = 90000;
    cfg.initial_anchor_mode = RtpTimestampInitialAnchorMode::ConfiguredReference;
    cfg.anchor_rtp_timestamp = 0xFFFFFF00U;
    cfg.anchor_timestamp_ns = 10000;

    VideoRtpTimestampMapper mapper{cfg};

    auto anchor = mapper.map(0xFFFFFF00U);
    assert(anchor.has_value());
    assert(*anchor == 10000);

    // From 0xFFFFFF00 to 0x0000002C is 300 RTP ticks.
    auto wrapped = mapper.map(0x0000002CU);

    assert(wrapped.has_value());
    assert(*wrapped == 10000 + 3333333ULL);
}

void rtp_mapper_can_continue_after_wraparound_in_configured_reference_mode() {
    VideoRtpTimestampMapperConfig cfg{};
    cfg.rtp_clock_rate = 90000;
    cfg.initial_anchor_mode = RtpTimestampInitialAnchorMode::ConfiguredReference;
    cfg.anchor_rtp_timestamp = 0xFFFFFFFEU;
    cfg.anchor_timestamp_ns = 0;

    VideoRtpTimestampMapper mapper{cfg};

    auto a = mapper.map(0xFFFFFFFEU);
    assert(a.has_value());
    assert(*a == 0);

    auto b = mapper.map(0x00000000U);
    assert(b.has_value());
    assert(*b == 22222ULL);

    auto c = mapper.map(0x00000002U);
    assert(c.has_value());
    assert(*c == 44444ULL);
}

void rtp_mapper_rejects_backward_timestamp() {
    VideoRtpTimestampMapperConfig cfg{};
    cfg.rtp_clock_rate = 90000;
    cfg.initial_anchor_mode = RtpTimestampInitialAnchorMode::ConfiguredReference;
    cfg.anchor_rtp_timestamp = 1000;
    cfg.anchor_timestamp_ns = 0;

    VideoRtpTimestampMapper mapper{cfg};

    auto first = mapper.map(2000);
    assert(first.has_value());

    auto backward = mapper.map(1500);
    assert(!backward.has_value());
    assert(backward.error() == Error::InvalidValue);
}

void rtp_mapper_rejects_zero_clock_rate() {
    VideoRtpTimestampMapperConfig cfg{};
    cfg.rtp_clock_rate = 0;
    cfg.initial_anchor_mode = RtpTimestampInitialAnchorMode::ConfiguredReference;
    cfg.anchor_rtp_timestamp = 0;
    cfg.anchor_timestamp_ns = 0;

    assert(validate_video_rtp_timestamp_mapper_config(cfg) == Error::InvalidValue);

    VideoRtpTimestampMapper mapper{cfg};

    auto mapped = mapper.map(0);
    assert(!mapped.has_value());
    assert(mapped.error() == Error::InvalidValue);
}

void rtp_mapper_rejects_nonzero_anchor_fields_for_first_observed_zero_mode() {
    VideoRtpTimestampMapperConfig cfg{};
    cfg.rtp_clock_rate = 90000;
    cfg.initial_anchor_mode = RtpTimestampInitialAnchorMode::FirstObservedBecomesLocalZero;
    cfg.anchor_rtp_timestamp = 12345;
    cfg.anchor_timestamp_ns = 777;

    assert(validate_video_rtp_timestamp_mapper_config(cfg) == Error::InvalidValue);
}

void synthetic_mapper_maps_unit_index_by_frame_cadence() {
    SyntheticVideoTimestampMapperConfig cfg{};
    cfg.fps_num = 25;
    cfg.fps_den = 1;
    cfg.anchor_timestamp_ns = 1000;

    SyntheticVideoTimestampMapper mapper{cfg};

    auto first = mapper.map_unit_index(0);
    assert(first.has_value());
    assert(*first == 1000);

    auto second = mapper.map_unit_index(1);
    assert(second.has_value());
    assert(*second == 40001000ULL);

    auto third = mapper.map_unit_index(2);
    assert(third.has_value());
    assert(*third == 80001000ULL);
}

void synthetic_mapper_rejects_invalid_frame_rate() {
    SyntheticVideoTimestampMapperConfig cfg{};
    cfg.fps_num = 0;
    cfg.fps_den = 1;
    cfg.anchor_timestamp_ns = 0;

    assert(validate_synthetic_video_timestamp_mapper_config(cfg) == Error::InvalidValue);

    SyntheticVideoTimestampMapper mapper{cfg};

    auto mapped = mapper.map_unit_index(0);
    assert(!mapped.has_value());
    assert(mapped.error() == Error::InvalidValue);
}

} // namespace

int main() {
    rtp_mapper_default_mode_maps_first_observed_to_local_zero();
    rtp_mapper_maps_anchor_to_anchor_timestamp_in_configured_reference_mode();
    rtp_mapper_maps_90000_ticks_to_one_second_in_configured_reference_mode();
    rtp_mapper_maps_progressive_25fps_tick_step_from_first_observed_zero();
    rtp_mapper_handles_32bit_wraparound_in_configured_reference_mode();
    rtp_mapper_can_continue_after_wraparound_in_configured_reference_mode();
    rtp_mapper_rejects_backward_timestamp();
    rtp_mapper_rejects_zero_clock_rate();
    rtp_mapper_rejects_nonzero_anchor_fields_for_first_observed_zero_mode();

    synthetic_mapper_maps_unit_index_by_frame_cadence();
    synthetic_mapper_rejects_invalid_frame_rate();

    return 0;
}
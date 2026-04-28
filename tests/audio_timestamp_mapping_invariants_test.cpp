#include <st2110/audio_timestamp_mapping.hpp>
#include <st2110/error.hpp>
#include <st2110/timestamp.hpp>

#include <cassert>
#include <cstdint>
#include <expected>
#include <limits>

namespace {
constexpr uint32_t kAudioClock48k = 48'000;
constexpr uint64_t kNsPerSecond = 1'000'000'000ull;
constexpr st2110::TimestampNs kOneMillisecondNs = 1'000'000ull;

st2110::TimestampNs expected_ns_from_ticks(uint64_t ticks, uint32_t rtp_clock_rate,
                                           st2110::TimestampNs anchor_timestamp_ns = 0) {
  const uint64_t whole_seconds = ticks / rtp_clock_rate;
  const uint64_t remainder_ticks = ticks % rtp_clock_rate;

  const uint64_t whole_ns = whole_seconds * kNsPerSecond;
  const uint64_t remainder_ns = (remainder_ticks * kNsPerSecond) / rtp_clock_rate;

  return anchor_timestamp_ns + whole_ns + remainder_ns;
}

void expect_timestamp(const std::expected<st2110::TimestampNs, st2110::Error> &result, st2110::TimestampNs expected) {
  assert(result.has_value());
  assert(*result == expected);
}

void expect_error(const std::expected<st2110::TimestampNs, st2110::Error> &result, st2110::Error expected) {
  assert(!result.has_value());
  assert(result.error() == expected);
}

void regular_48k_1ms_cadence_is_exact_and_monotonic() {
  const st2110::TimestampNs anchor_ns = 10'000'000'000ull;
  st2110::AudioRtpTimestampMapper mapper(
      {.rtp_clock_rate = kAudioClock48k, .anchor_rtp_timestamp = 90'000, .anchor_timestamp_ns = anchor_ns});

  st2110::TimestampNs previous = 0;

  for (uint32_t packet_index = 0; packet_index != 256; ++packet_index) {
    const uint32_t rtp_timestamp = static_cast<uint32_t>(90'000 + packet_index * 48u);
    const st2110::TimestampNs expected = anchor_ns + packet_index * kOneMillisecondNs;

    const auto mapped = mapper.map(rtp_timestamp);
    expect_timestamp(mapped, expected);

    if (packet_index != 0) {
      assert(*mapped > previous);
      assert(*mapped - previous == kOneMillisecondNs);
    }

    previous = *mapped;
  }
}

void non_default_audio_clock_rate_uses_explicit_clock_rate() {
  constexpr uint32_t clock_rate = 96'000;
  constexpr uint32_t ticks_per_1ms_packet = 96;

  st2110::AudioRtpTimestampMapper mapper(
      {.rtp_clock_rate = clock_rate, .anchor_rtp_timestamp = 1'000, .anchor_timestamp_ns = 0});

  auto first = mapper.map(1'000);
  expect_timestamp(first, 0);

  auto second = mapper.map(1'000 + ticks_per_1ms_packet);
  expect_timestamp(second, kOneMillisecondNs);

  auto third = mapper.map(1'000 + 2 * ticks_per_1ms_packet);
  expect_timestamp(third, 2 * kOneMillisecondNs);
}

void wraparound_preserves_monotonic_48k_cadence() {
  constexpr uint32_t anchor_rtp = 0xfffffff0u;

  st2110::AudioRtpTimestampMapper mapper(
      {.rtp_clock_rate = kAudioClock48k, .anchor_rtp_timestamp = anchor_rtp, .anchor_timestamp_ns = 0});

  st2110::TimestampNs previous = 0;

  for (uint32_t packet_index = 0; packet_index != 16; ++packet_index) {
    const uint32_t rtp_timestamp = static_cast<uint32_t>(anchor_rtp + packet_index * 48u);
    const st2110::TimestampNs expected = packet_index * kOneMillisecondNs;

    const auto mapped = mapper.map(rtp_timestamp);
    expect_timestamp(mapped, expected);

    if (packet_index != 0) {
      assert(*mapped > previous);
      assert(*mapped - previous == kOneMillisecondNs);
    }

    previous = *mapped;
  }
}

void long_running_stream_continues_beyond_one_32bit_rtp_epoch() {
  st2110::AudioRtpTimestampMapper mapper(
      {.rtp_clock_rate = kAudioClock48k, .anchor_rtp_timestamp = 0, .anchor_timestamp_ns = 0});

  expect_timestamp(mapper.map(0), 0);

  constexpr uint64_t large_forward_delta = 0x7ffffff0ull;
  constexpr uint32_t first_rtp = static_cast<uint32_t>(large_forward_delta);
  constexpr uint32_t second_rtp = static_cast<uint32_t>(large_forward_delta * 2ull);
  constexpr uint32_t third_rtp = static_cast<uint32_t>(large_forward_delta * 2ull + 96ull);

  const uint64_t first_accumulated_ticks = large_forward_delta;
  const uint64_t second_accumulated_ticks = large_forward_delta * 2ull;
  const uint64_t third_accumulated_ticks = large_forward_delta * 2ull + 96ull;

  expect_timestamp(mapper.map(first_rtp), expected_ns_from_ticks(first_accumulated_ticks, kAudioClock48k));

  expect_timestamp(mapper.map(second_rtp), expected_ns_from_ticks(second_accumulated_ticks, kAudioClock48k));

  const auto third = mapper.map(third_rtp);
  expect_timestamp(third, expected_ns_from_ticks(third_accumulated_ticks, kAudioClock48k));

  assert(*third > expected_ns_from_ticks(0xffffffffull, kAudioClock48k));
}

void backward_or_ambiguous_delta_is_rejected_without_advancing_state() {
  st2110::AudioRtpTimestampMapper mapper(
      {.rtp_clock_rate = kAudioClock48k, .anchor_rtp_timestamp = 10'000, .anchor_timestamp_ns = 0});

  expect_timestamp(mapper.map(10'000), 0);

  expect_error(mapper.map(9'999), st2110::Error::InvalidValue);

  // The failed backward packet must not advance the mapper state.
  expect_timestamp(mapper.map(10'048), kOneMillisecondNs);
}

void exact_half_range_delta_is_rejected_as_ambiguous() {
  st2110::AudioRtpTimestampMapper mapper(
      {.rtp_clock_rate = kAudioClock48k, .anchor_rtp_timestamp = 0, .anchor_timestamp_ns = 0});

  expect_timestamp(mapper.map(0), 0);
  expect_error(mapper.map(0x80000000u), st2110::Error::InvalidValue);
}

void reset_reanchors_mapping_and_discards_previous_cadence_state() {
  st2110::AudioRtpTimestampMapper mapper(
      {.rtp_clock_rate = kAudioClock48k, .anchor_rtp_timestamp = 1'000, .anchor_timestamp_ns = 0});

  expect_timestamp(mapper.map(1'000), 0);
  expect_timestamp(mapper.map(1'048), kOneMillisecondNs);

  const auto reset_error = mapper.reset(
      {.rtp_clock_rate = kAudioClock48k, .anchor_rtp_timestamp = 50'000, .anchor_timestamp_ns = 5'000'000'000ull});
  assert(reset_error == st2110::Error::Ok);

  expect_timestamp(mapper.map(50'000), 5'000'000'000ull);
  expect_timestamp(mapper.map(50'048), 5'001'000'000ull);
}

void playout_timing_preserves_media_cadence_with_constant_delay() {
  const st2110::AudioReceiverPlayoutTimingConfig cfg{.playout_delay_ns = 2'000'000ull};

  const auto first = st2110::audio_block_timing(100, 10'000'000ull, cfg);
  assert(first.has_value());
  assert(first->rtp_timestamp == 100);
  assert(first->media_timestamp_ns == 10'000'000ull);
  assert(first->playout_timestamp_ns == 12'000'000ull);

  const auto second = st2110::audio_block_timing(148, 11'000'000ull, cfg);
  assert(second.has_value());
  assert(second->rtp_timestamp == 148);
  assert(second->media_timestamp_ns == 11'000'000ull);
  assert(second->playout_timestamp_ns == 13'000'000ull);

  assert(second->media_timestamp_ns - first->media_timestamp_ns == kOneMillisecondNs);
  assert(second->playout_timestamp_ns - first->playout_timestamp_ns == kOneMillisecondNs);
}
} // namespace

int main() {
  regular_48k_1ms_cadence_is_exact_and_monotonic();
  non_default_audio_clock_rate_uses_explicit_clock_rate();
  wraparound_preserves_monotonic_48k_cadence();
  long_running_stream_continues_beyond_one_32bit_rtp_epoch();
  backward_or_ambiguous_delta_is_rejected_without_advancing_state();
  exact_half_range_delta_is_rejected_as_ambiguous();
  reset_reanchors_mapping_and_discards_previous_cadence_state();
  playout_timing_preserves_media_cadence_with_constant_delay();

  return 0;
}
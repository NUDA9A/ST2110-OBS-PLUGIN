#include <st2110/video_timestamp_mapping.hpp>
#include <st2110/video_playout_timing.hpp>
#include <st2110/video_frame.hpp>
#include <st2110/pixel_format.hpp>
#include <st2110/error.hpp>

#include <cassert>
#include <cstdint>
#include <limits>

using namespace st2110;

static void rtp_mapper_maps_90khz_ticks_to_monotonic_ns() {
  VideoRtpTimestampMapper mapper({
      .rtp_clock_rate = 90000,
      .anchor_rtp_timestamp = 1000,
      .anchor_timestamp_ns = 5000000000ULL,
  });

  auto t0 = mapper.map(1000);
  assert(t0.has_value());
  assert(*t0 == 5000000000ULL);

  auto t1 = mapper.map(4000); // +3000 ticks = 1/30 s
  assert(t1.has_value());
  assert(*t1 == 5033333333ULL);

  auto t2 = mapper.map(7000); // +6000 ticks = 2/30 s
  assert(t2.has_value());
  assert(*t2 == 5066666666ULL);

  assert(*t0 < *t1);
  assert(*t1 < *t2);
}

static void rtp_mapper_preserves_continuity_across_32bit_wraparound() {
  VideoRtpTimestampMapper mapper({
      .rtp_clock_rate = 90000,
      .anchor_rtp_timestamp = 0xFFFFFF00U,
      .anchor_timestamp_ns = 1000000000ULL,
  });

  auto t0 = mapper.map(0xFFFFFF00U);
  assert(t0.has_value());
  assert(*t0 == 1000000000ULL);

  auto t1 = mapper.map(0xFFFFFFB4U); // +180 ticks = 2 ms
  assert(t1.has_value());
  assert(*t1 == 1002000000ULL);

  auto t2 = mapper.map(44U); // wraparound, total +300 ticks
  assert(t2.has_value());
  assert(*t2 == 1003333333ULL);

  assert(*t0 < *t1);
  assert(*t1 < *t2);
}

static void rtp_mapper_rejects_backward_or_ambiguous_delta() {
  {
    VideoRtpTimestampMapper mapper({
        .rtp_clock_rate = 90000,
        .anchor_rtp_timestamp = 1000,
        .anchor_timestamp_ns = 0,
    });

    auto t0 = mapper.map(1000);
    assert(t0.has_value());

    auto backward = mapper.map(900);
    assert(!backward.has_value());
    assert(backward.error() == Error::InvalidValue);
  }

  {
    VideoRtpTimestampMapper mapper({
        .rtp_clock_rate = 90000,
        .anchor_rtp_timestamp = 0,
        .anchor_timestamp_ns = 0,
    });

    auto ambiguous_half_range = mapper.map(0x80000000U);
    assert(!ambiguous_half_range.has_value());
    assert(ambiguous_half_range.error() == Error::InvalidValue);
  }
}

static void rtp_mapper_rejects_invalid_clock_rate() {
  VideoRtpTimestampMapper mapper({
      .rtp_clock_rate = 0,
      .anchor_rtp_timestamp = 0,
      .anchor_timestamp_ns = 0,
  });

  auto mapped = mapper.map(0);
  assert(!mapped.has_value());
  assert(mapped.error() == Error::InvalidValue);
}

static void rtp_mapper_rejects_nanosecond_overflow() {
  VideoRtpTimestampMapper mapper({
      .rtp_clock_rate = 90000,
      .anchor_rtp_timestamp = 0,
      .anchor_timestamp_ns = std::numeric_limits<TimestampNs>::max() - 10,
  });

  auto mapped = mapper.map(90000); // +1 second cannot fit
  assert(!mapped.has_value());
  assert(mapped.error() == Error::InvalidValue);
}

static void synthetic_mapper_uses_explicit_fps_cadence_without_becoming_rtp_mapping() {
  SyntheticVideoTimestampMapper mapper({
      .fps_num = 30000,
      .fps_den = 1001,
      .anchor_timestamp_ns = 10,
  });

  auto t0 = mapper.map_unit_index(0);
  assert(t0.has_value());
  assert(*t0 == 10ULL);

  auto t1 = mapper.map_unit_index(1);
  assert(t1.has_value());
  assert(*t1 == 33366676ULL);

  auto t2 = mapper.map_unit_index(2);
  assert(t2.has_value());
  assert(*t2 == 66733343ULL);

  assert(*t0 < *t1);
  assert(*t1 < *t2);
}

static void synthetic_mapper_rejects_invalid_fps_config() {
  SyntheticVideoTimestampMapper mapper({
      .fps_num = 0,
      .fps_den = 1,
      .anchor_timestamp_ns = 0,
  });

  auto mapped = mapper.map_unit_index(0);
  assert(!mapped.has_value());
  assert(mapped.error() == Error::InvalidValue);
}

static void reconstructed_frame_timing_uses_mapped_media_timestamp_plus_playout_offset() {
  VideoRtpTimestampMapper timestamp_mapper({
      .rtp_clock_rate = 90000,
      .anchor_rtp_timestamp = 100,
      .anchor_timestamp_ns = 1000000000ULL,
  });

  auto media_timestamp = timestamp_mapper.map(3100); // +3000 ticks = 1/30 s
  assert(media_timestamp.has_value());
  assert(*media_timestamp == 1033333333ULL);

  ReconstructedVideoFrame frame{
      .frame = VideoFrame(2, 1, PixelFormat::UYVY),
      .rtp_timestamp = 3100,
      .complete = true,
  };

  VideoReceiverPlayoutTimingConfig playout_cfg{
      .link_offset_delay_ns = 5000000ULL,
  };

  auto timing = video_reconstructed_frame_timing(frame, *media_timestamp, playout_cfg);

  assert(timing.has_value());
  assert(timing->rtp_timestamp == 3100);
  assert(timing->media_timestamp_ns == 1033333333ULL);
  assert(timing->reconstruction_timestamp_ns == 1038333333ULL);
}

int main() {
  rtp_mapper_maps_90khz_ticks_to_monotonic_ns();
  rtp_mapper_preserves_continuity_across_32bit_wraparound();
  rtp_mapper_rejects_backward_or_ambiguous_delta();
  rtp_mapper_rejects_invalid_clock_rate();
  rtp_mapper_rejects_nanosecond_overflow();

  synthetic_mapper_uses_explicit_fps_cadence_without_becoming_rtp_mapping();
  synthetic_mapper_rejects_invalid_fps_config();

  reconstructed_frame_timing_uses_mapped_media_timestamp_plus_playout_offset();

  return 0;
}
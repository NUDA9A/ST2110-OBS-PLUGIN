#include <cassert>
#include <cstdint>
#include <optional>

#include <st2110/video_signaling.hpp>

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
  s.max_udp_datagram_bytes = st2110::standardUdpDatagramSizeLimitBytes;

  s.media_clock_mode = st2110::MediaClockMode::Direct;
  s.timestamp_mode = st2110::TimestampMode::New;
  s.reference_clock.kind = st2110::ReferenceClockKind::Ptp;
  s.reference_clock.ptp = st2110::PtpReferenceClock{};
  s.ts_delay_sender_ticks = 0;

  s.sender_type = st2110::VideoSenderType::Narrow;
  s.troff_us = std::nullopt;
  s.cmax = std::nullopt;

  return s;
}

static void test_valid_progressive_gpm_signaling_is_accepted() {
  st2110::VideoStreamSignaling s = make_base_signaling();

  assert(st2110::validate_video_stream_signaling(s) == st2110::Error::Ok);
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
  s.reference_clock.local_mac = st2110::LocalMacReferenceClock{};
  s.sender_type = st2110::VideoSenderType::NarrowLinear;

  assert(st2110::validate_video_stream_signaling(s) == st2110::Error::Ok);
}

static void test_invalid_dimensions_are_rejected() {
  st2110::VideoStreamSignaling s = make_base_signaling();
  s.media.width = 0;
  s.media.height = 1080;
  s.media.fps_num = 25;
  s.media.fps_den = 1;

  assert(st2110::validate_video_stream_signaling(s) == st2110::Error::InvalidValue);
}

static void test_invalid_frame_rate_is_rejected() {
  st2110::VideoStreamSignaling s = make_base_signaling();
  s.media.width = 1920;
  s.media.height = 1080;
  s.media.fps_num = 0;
  s.media.fps_den = 1;

  assert(st2110::validate_video_stream_signaling(s) == st2110::Error::InvalidValue);
}

static void test_odd_width_is_structurally_valid_but_runtime_projection_rejects_it() {
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

static void test_invalid_maxudp_config_is_rejected() {
  st2110::VideoStreamSignaling s = make_base_signaling();
  s.media.width = 1920;
  s.media.height = 1080;
  s.media.fps_num = 25;
  s.media.fps_den = 1;
  s.max_udp_datagram_bytes = 8; // smaller than min parsable datagram

  assert(st2110::validate_video_stream_signaling(s) == st2110::Error::InvalidValue);
}

static void test_packet_parse_policy_is_derived_from_signaling() {
  st2110::VideoStreamSignaling s{};
  s.max_udp_datagram_bytes = 4096;

  st2110::PacketParsePolicy p = st2110::packet_parse_policy_from_video_stream_signaling(s);

  assert(p.max_udp_datagram_bytes.has_value());
  assert(*p.max_udp_datagram_bytes == 4096u);
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

int main() {
  test_valid_progressive_gpm_signaling_is_accepted();
  test_valid_bpm_signaling_is_accepted();
  test_invalid_dimensions_are_rejected();
  test_invalid_frame_rate_is_rejected();
  test_odd_width_is_structurally_valid_but_runtime_projection_rejects_it();
  test_invalid_maxudp_config_is_rejected();
  test_packet_parse_policy_is_derived_from_signaling();
  test_absent_maxudp_produces_empty_policy_override();
  test_unsupported_sampling_is_structurally_valid_but_not_runtime_mappable();
  return 0;
}
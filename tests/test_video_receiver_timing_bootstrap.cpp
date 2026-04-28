#include <cassert>
#include <cstdint>
#include <optional>
#include <string>

#include <st2110/video_receiver_timing_signaling.hpp>

static st2110::VideoStreamSignaling make_valid_signaling() {
  st2110::VideoStreamSignaling s{};

  s.media.sampling.known = st2110::VideoSampling::Known::YCbCr422;
  s.media.width = 1920;
  s.media.height = 1080;
  s.media.fps_num = 25;
  s.media.fps_den = 1;
  s.media.depth.bits = 8;
  s.media.depth.floating_point = false;
  s.media.colorimetry.known = st2110::VideoColorimetry::Known::Bt709;

  s.scan_mode = st2110::VideoScanMode::Progressive;
  s.packing_mode = st2110::VideoPackingMode::Gpm;
  s.max_udp_datagram_bytes = st2110::standardUdpDatagramSizeLimitBytes;

  s.media_clock_mode = st2110::MediaClockMode::Direct;
  s.timestamp_mode = st2110::TimestampMode::New;

  s.reference_clock.kind = st2110::ReferenceClockKind::Ptp;
  s.reference_clock.ptp = st2110::PtpReferenceClock{};
  s.reference_clock.local_mac.reset();
  s.reference_clock.raw_token.reset();

  s.ts_delay_sender_ticks = 0;

  s.sender_type = st2110::VideoSenderType::Narrow;
  s.troff_us.reset();
  s.cmax.reset();

  return s;
}

static st2110::VideoReceiverTimingConfig make_valid_timing_config() {
  st2110::VideoReceiverTimingConfig cfg{};
  cfg.capability.supports_type_n = true;
  cfg.capability.supports_type_nl = true;
  cfg.capability.supports_type_w = true;

  cfg.requirements.require_reference_clock = true;
  cfg.requirements.require_media_clock = true;
  cfg.requirements.require_timestamp_mode = true;
  cfg.requirements.consume_ts_delay = true;
  cfg.requirements.consume_sender_troff = true;
  cfg.requirements.consume_sender_cmax = true;

  return cfg;
}

static void test_timing_aware_bootstrap_config_is_composed_successfully() {
  const auto signaling = make_valid_signaling();
  const auto timing_cfg = make_valid_timing_config();

  auto res = st2110::video_receiver_bootstrap_config_from_video_stream_signaling(
      signaling, timing_cfg, 5004, 112, "127.0.0.1", "239.0.0.1", st2110::PartialFramePolicy::EmitWithFlag);

  assert(res.has_value());

  const auto &cfg = *res;

  assert(cfg.packet_parse_policy.max_udp_datagram_bytes == signaling.max_udp_datagram_bytes);

  assert(cfg.rx_config.width == signaling.media.width);
  assert(cfg.rx_config.height == signaling.media.height);
  assert(cfg.rx_config.fps_num == signaling.media.fps_num);
  assert(cfg.rx_config.fps_den == signaling.media.fps_den);
  assert(cfg.rx_config.format == st2110::PixelFormat::UYVY);
  assert(cfg.rx_config.scan_mode == signaling.scan_mode);
  assert(cfg.rx_config.udp_port == 5004);
  assert(cfg.rx_config.payload_type == 112);
  assert(cfg.rx_config.local_ip == "127.0.0.1");
  assert(cfg.rx_config.dest_ip == "239.0.0.1");

  assert(cfg.receive_pipeline_config.depacketizer.width == signaling.media.width);
  assert(cfg.receive_pipeline_config.depacketizer.height == signaling.media.height);
  assert(cfg.receive_pipeline_config.depacketizer.format == st2110::PixelFormat::UYVY);
  assert(cfg.receive_pipeline_config.depacketizer.scan_mode == signaling.scan_mode);
  assert(cfg.receive_pipeline_config.depacketizer.packing_mode == signaling.packing_mode);
  assert(cfg.receive_pipeline_config.depacketizer.partial_frame_policy == st2110::PartialFramePolicy::EmitWithFlag);

  assert(cfg.receive_pipeline_config.reconstructor.format == st2110::PixelFormat::UYVY);
  assert(cfg.receive_pipeline_config.reconstructor.scan_mode == signaling.scan_mode);

  assert(cfg.timing_config.capability.supports_type_n == timing_cfg.capability.supports_type_n);
  assert(cfg.timing_config.capability.supports_type_nl == timing_cfg.capability.supports_type_nl);
  assert(cfg.timing_config.capability.supports_type_w == timing_cfg.capability.supports_type_w);

  assert(cfg.timing_config.requirements.require_reference_clock == timing_cfg.requirements.require_reference_clock);
  assert(cfg.timing_config.requirements.require_media_clock == timing_cfg.requirements.require_media_clock);
  assert(cfg.timing_config.requirements.require_timestamp_mode == timing_cfg.requirements.require_timestamp_mode);
  assert(cfg.timing_config.requirements.consume_ts_delay == timing_cfg.requirements.consume_ts_delay);
  assert(cfg.timing_config.requirements.consume_sender_troff == timing_cfg.requirements.consume_sender_troff);
  assert(cfg.timing_config.requirements.consume_sender_cmax == timing_cfg.requirements.consume_sender_cmax);
}

static void test_timing_aware_bootstrap_rejects_unsupported_sender_type() {
  auto signaling = make_valid_signaling();
  signaling.sender_type = st2110::VideoSenderType::Wide;
  signaling.cmax = 12;

  auto timing_cfg = make_valid_timing_config();
  timing_cfg.capability.supports_type_w = false;

  auto res = st2110::video_receiver_bootstrap_config_from_video_stream_signaling(
      signaling, timing_cfg, 5004, 112, "127.0.0.1", "239.0.0.1", st2110::PartialFramePolicy::EmitWithFlag);

  assert(!res.has_value());
  assert(res.error() == st2110::Error::Unsupported);
}

static void test_timing_aware_bootstrap_rejects_unconsumed_ts_delay() {
  auto signaling = make_valid_signaling();
  signaling.ts_delay_sender_ticks = 900;

  auto timing_cfg = make_valid_timing_config();
  timing_cfg.requirements.consume_ts_delay = false;

  auto res = st2110::video_receiver_bootstrap_config_from_video_stream_signaling(
      signaling, timing_cfg, 5004, 112, "127.0.0.1", "239.0.0.1", st2110::PartialFramePolicy::EmitWithFlag);

  assert(!res.has_value());
  assert(res.error() == st2110::Error::Unsupported);
}

int main() {
  test_timing_aware_bootstrap_config_is_composed_successfully();
  test_timing_aware_bootstrap_rejects_unsupported_sender_type();
  test_timing_aware_bootstrap_rejects_unconsumed_ts_delay();
  return 0;
}
#include "st2110/video_sdp_fmtp.hpp"
#include "st2110/video_sdp_ingestion.hpp"
#include "st2110/video_signaling.hpp"

#include <cassert>
#include <string>
#include <string_view>

using namespace st2110;

static bool fmtp_has_unknown_named(const RawVideoSdpFmtpParameters &fmtp, std::string_view name) {
  for (const auto &unknown : fmtp.unknown_parameters) {
    if (unknown.name == name) {
      return true;
    }
  }

  return false;
}

static std::string make_fmtp_payload_with_timing_parameters() {
  return "sampling=YCbCr-4:2:2; "
         "width=1920; "
         "height=1080; "
         "exactframerate=60000/1001; "
         "depth=10; "
         "colorimetry=BT709; "
         "PM=2110GPM; "
         "SSN=ST2110-20:2022; "
         "TCS=SDR; "
         "RANGE=FULL; "
         "TP=2110TPW; "
         "TROFF=11; "
         "CMAX=2; "
         "TSMODE=SAMP; "
         "TSDELAY=37; "
         "X-FUTURE-PARAM=future";
}

static std::string make_video_sdp_with_fmtp_timing_parameters() {
  return "v=0\r\n"
         "o=- 0 0 IN IP4 127.0.0.1\r\n"
         "s=ST2110 test\r\n"
         "t=0 0\r\n"
         "m=video 5004 RTP/AVP 96\r\n"
         "a=rtpmap:96 raw/90000\r\n"
         "a=fmtp:96 sampling=YCbCr-4:2:2; width=1920; height=1080; exactframerate=60000/1001; depth=10; "
         "colorimetry=BT709; PM=2110GPM; SSN=ST2110-20:2022; TCS=SDR; RANGE=FULL; TP=2110TPW; TROFF=11; CMAX=2; "
         "TSMODE=SAMP; TSDELAY=37\r\n"
         "a=ts-refclk:ptp=IEEE1588-2008:39-A7-94-FF-FE-07-CB-D0:127\r\n"
         "a=mediaclk:direct=0\r\n";
}

static std::string make_video_sdp_with_standalone_timing_attributes() {
  return "v=0\r\n"
         "o=- 0 0 IN IP4 127.0.0.1\r\n"
         "s=ST2110 test\r\n"
         "t=0 0\r\n"
         "m=video 5004 RTP/AVP 96\r\n"
         "a=rtpmap:96 raw/90000\r\n"
         "a=fmtp:96 sampling=YCbCr-4:2:2; width=1920; height=1080; exactframerate=60000/1001; depth=10; "
         "colorimetry=BT709; PM=2110GPM; SSN=ST2110-20:2022; TCS=SDR; RANGE=FULL\r\n"
         "a=ts-refclk:ptp=IEEE1588-2008:39-A7-94-FF-FE-07-CB-D0:127\r\n"
         "a=mediaclk:direct=0\r\n"
         "a=tsmode:SAMP\r\n"
         "a=tsdelay:37\r\n"
         "a=tp:2110TPW\r\n"
         "a=troff:11\r\n"
         "a=cmax:2\r\n";
}

static void test_fmtp_parser_extracts_known_timing_parameters() {
  auto parsed = parse_video_sdp_fmtp_payload(make_fmtp_payload_with_timing_parameters());
  assert(parsed.has_value());

  const auto &fmtp = *parsed;

  assert(fmtp.sender_type.has_value());
  assert(*fmtp.sender_type == "2110TPW");

  assert(fmtp.troff_us.has_value());
  assert(*fmtp.troff_us == 11);

  assert(fmtp.cmax.has_value());
  assert(*fmtp.cmax == 2);

  assert(fmtp.timestamp_mode.has_value());
  assert(*fmtp.timestamp_mode == "SAMP");

  assert(fmtp.ts_delay_sender_ticks.has_value());
  assert(*fmtp.ts_delay_sender_ticks == 37);

  assert(!fmtp_has_unknown_named(fmtp, "TP"));
  assert(!fmtp_has_unknown_named(fmtp, "TROFF"));
  assert(!fmtp_has_unknown_named(fmtp, "CMAX"));
  assert(!fmtp_has_unknown_named(fmtp, "TSMODE"));
  assert(!fmtp_has_unknown_named(fmtp, "TSDELAY"));

  assert(fmtp_has_unknown_named(fmtp, "X-FUTURE-PARAM"));
}

static void test_fmtp_parser_rejects_duplicate_known_timing_parameter() {
  std::string payload = make_fmtp_payload_with_timing_parameters();
  payload += "; TP=2110TPN";

  auto parsed = parse_video_sdp_fmtp_payload(payload);
  assert(!parsed.has_value());
  assert(parsed.error() == Error::InvalidValue);
}

static void test_fmtp_parser_rejects_malformed_numeric_timing_parameter() {
  std::string payload = "sampling=YCbCr-4:2:2; "
                        "width=1920; "
                        "height=1080; "
                        "exactframerate=60000/1001; "
                        "depth=10; "
                        "colorimetry=BT709; "
                        "PM=2110GPM; "
                        "SSN=ST2110-20:2022; "
                        "TROFF=not-a-number";

  auto parsed = parse_video_sdp_fmtp_payload(payload);
  assert(!parsed.has_value());
  assert(parsed.error() == Error::InvalidValue);
}

static void test_final_ingestion_maps_fmtp_timing_parameters_to_signaling() {
  const std::string sdp = make_video_sdp_with_fmtp_timing_parameters();

  auto signaling_expected = parse_video_stream_signaling_from_sdp(sdp, 96);
  assert(signaling_expected.has_value());

  const auto &signaling = *signaling_expected;

  assert(signaling.sender_type == VideoSenderType::Wide);

  assert(signaling.troff_us.has_value());
  assert(*signaling.troff_us == 11);

  assert(signaling.cmax.has_value());
  assert(*signaling.cmax == 2);

  assert(signaling.timestamp_mode == TimestampMode::Samp);
  assert(signaling.ts_delay_sender_ticks == 37);

  assert(signaling.reference_clock.kind == ReferenceClockKind::Ptp);
  assert(signaling.media_clock_mode == MediaClockMode::Direct);

  assert(validate_video_stream_signaling(signaling) == Error::Ok);
}

static void test_standalone_timing_attributes_remain_supported_as_compatibility_path() {
  const std::string sdp = make_video_sdp_with_standalone_timing_attributes();

  auto signaling_expected = parse_video_stream_signaling_from_sdp(sdp, 96);
  assert(signaling_expected.has_value());

  const auto &signaling = *signaling_expected;

  assert(signaling.sender_type == VideoSenderType::Wide);

  assert(signaling.troff_us.has_value());
  assert(*signaling.troff_us == 11);

  assert(signaling.cmax.has_value());
  assert(*signaling.cmax == 2);

  assert(signaling.timestamp_mode == TimestampMode::Samp);
  assert(signaling.ts_delay_sender_ticks == 37);

  assert(validate_video_stream_signaling(signaling) == Error::Ok);
}

static void test_final_ingestion_rejects_conflict_between_fmtp_and_standalone_timing_parameter() {
  std::string sdp = make_video_sdp_with_fmtp_timing_parameters();

  sdp += "a=tp:2110TPN\r\n";

  auto signaling_expected = parse_video_stream_signaling_from_sdp(sdp, 96);
  assert(!signaling_expected.has_value());
  assert(signaling_expected.error() == Error::InvalidValue);
}

int main() {
  test_fmtp_parser_extracts_known_timing_parameters();
  test_fmtp_parser_rejects_duplicate_known_timing_parameter();
  test_fmtp_parser_rejects_malformed_numeric_timing_parameter();
  test_final_ingestion_maps_fmtp_timing_parameters_to_signaling();
  test_standalone_timing_attributes_remain_supported_as_compatibility_path();
  test_final_ingestion_rejects_conflict_between_fmtp_and_standalone_timing_parameter();
  return 0;
}
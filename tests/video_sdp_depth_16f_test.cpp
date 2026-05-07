// tests/video_sdp_depth_16f_test.cpp

#include "st2110/ingress/video/video_sdp_fmtp.hpp"
#include "st2110/ingress/video/video_sdp_signaling_adapter.hpp"
#include "st2110/ingress/video/video_sdp_ingestion.hpp"
#include "st2110/video_signaling.hpp"

#include <cassert>
#include <string>
#include <string_view>

using namespace st2110;

static std::string make_fmtp_payload_with_depth(std::string_view depth_token) {
    std::string payload = "sampling=YCbCr-4:2:2; "
                          "width=1920; "
                          "height=1080; "
                          "exactframerate=60000/1001; "
                          "depth=";

    payload += depth_token;

    payload += "; colorimetry=BT709; "
               "PM=2110GPM; "
               "SSN=ST2110-20:2017; "
               "TCS=SDR; "
               "RANGE=FULL";

    return payload;
}

static std::string make_video_sdp_with_depth(std::string_view depth_token) {
    std::string sdp = "v=0\r\n"
                      "o=- 0 0 IN IP4 127.0.0.1\r\n"
                      "s=ST2110 depth test\r\n"
                      "t=0 0\r\n"
                      "m=video 5004 RTP/AVP 96\r\n"
                      "a=rtpmap:96 raw/90000\r\n"
                      "a=fmtp:96 sampling=YCbCr-4:2:2; width=1920; height=1080; exactframerate=60000/1001; depth=";

    sdp += depth_token;

    sdp += "; colorimetry=BT709; PM=2110GPM; SSN=ST2110-20:2017; TCS=SDR; RANGE=FULL; TP=2110TPN\r\n"
           "a=ts-refclk:ptp=IEEE1588-2008:39-A7-94-FF-FE-07-CB-D0:127\r\n"
           "a=mediaclk:direct=0\r\n";

    return sdp;
}

static void assert_integer_depth_maps(std::string_view depth_token, uint16_t expected_bits) {
    auto parsed = parse_video_sdp_fmtp_payload(make_fmtp_payload_with_depth(depth_token));
    assert(parsed.has_value());

    assert(parsed->depth == expected_bits);
    assert(!parsed->depth_floating_point);

    auto media = video_media_description_from_raw_video_sdp_fmtp(*parsed);
    assert(media.has_value());

    assert(media->depth.bits == expected_bits);
    assert(!media->depth.floating_point);
    assert(validate_video_media_description(*media) == Error::Ok);
}

static void test_fmtp_parser_keeps_integer_depths_working() {
    assert_integer_depth_maps("8", 8);
    assert_integer_depth_maps("10", 10);
    assert_integer_depth_maps("12", 12);
    assert_integer_depth_maps("16", 16);
}

static void test_fmtp_parser_accepts_depth_16f() {
    auto parsed = parse_video_sdp_fmtp_payload(make_fmtp_payload_with_depth("16f"));
    assert(parsed.has_value());

    assert(parsed->depth == 16);
    assert(parsed->depth_floating_point);

    auto media = video_media_description_from_raw_video_sdp_fmtp(*parsed);
    assert(media.has_value());

    assert(media->depth.bits == 16);
    assert(media->depth.floating_point);
    assert(validate_video_media_description(*media) == Error::Ok);
}

static void assert_depth_token_rejected(std::string_view depth_token) {
    auto parsed = parse_video_sdp_fmtp_payload(make_fmtp_payload_with_depth(depth_token));
    assert(!parsed.has_value());
    assert(parsed.error() == Error::InvalidValue);
}

static void test_fmtp_parser_rejects_malformed_depth_tokens() {
    assert_depth_token_rejected("");
    assert_depth_token_rejected("0");
    assert_depth_token_rejected("abc");
    assert_depth_token_rejected("16ff");
    assert_depth_token_rejected("f");
    assert_depth_token_rejected("8f");
    assert_depth_token_rejected("10f");
    assert_depth_token_rejected("12f");
    assert_depth_token_rejected("65536");
}

static void test_sdp_ingestion_accepts_depth_16f_as_signaling() {
    const std::string sdp = make_video_sdp_with_depth("16f");

    auto signaling_expected = parse_video_stream_signaling_from_sdp(sdp, 96);
    assert(signaling_expected.has_value());

    const auto &signaling = *signaling_expected;

    assert(signaling.media.depth.bits == 16);
    assert(signaling.media.depth.floating_point);

    assert(validate_video_stream_signaling(signaling) == Error::Ok);
}

static void test_depth_16f_runtime_projection_remains_unsupported_for_mvp() {
    const std::string sdp = make_video_sdp_with_depth("16f");

    auto signaling_expected = parse_video_stream_signaling_from_sdp(sdp, 96);
    assert(signaling_expected.has_value());

    const auto &signaling = *signaling_expected;

    auto pixel_format = pixel_format_from_video_stream_signaling(signaling);
    assert(!pixel_format.has_value());
    assert(pixel_format.error() == Error::Unsupported);

    auto depacketizer_config = depacketizer_config_from_video_stream_signaling(signaling, PartialFramePolicy::Drop);
    assert(!depacketizer_config.has_value());
    assert(depacketizer_config.error() == Error::Unsupported);
}

int main() {
    test_fmtp_parser_keeps_integer_depths_working();
    test_fmtp_parser_accepts_depth_16f();
    test_fmtp_parser_rejects_malformed_depth_tokens();
    test_sdp_ingestion_accepts_depth_16f_as_signaling();
    test_depth_16f_runtime_projection_remains_unsupported_for_mvp();

    return 0;
}
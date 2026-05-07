#include "st2110/ingress/video/video_sdp_ingestion.hpp"
#include "st2110/ingress/video/video_sdp_media_section.hpp"
#include "st2110/video_signaling.hpp"

#include <cassert>
#include <string>

using namespace st2110;

static std::string make_valid_video_sdp() {
    return "v=0\r\n"
           "o=- 0 0 IN IP4 127.0.0.1\r\n"
           "s=ST2110 test\r\n"
           "t=0 0\r\n"
           "m=audio 5006 RTP/AVP 97\r\n"
           "a=rtpmap:97 L24/48000/2\r\n"
           "m=video 5004 RTP/AVP 96\r\n"
           "a=rtpmap:96 raw/90000\r\n"
           "a=fmtp:96 sampling=YCbCr-4:2:2; width=1920; height=1080; exactframerate=60000/1001; depth=10; "
           "colorimetry=BT709; PM=2110GPM; SSN=ST2110-20:2017; TCS=SDR; RANGE=FULL; TP=2110TPN\r\n"
           "a=ts-refclk:ptp=IEEE1588-2008:39-A7-94-FF-FE-07-CB-D0:127\r\n"
           "a=mediaclk:direct=0\r\n"
           "a=tsmode:SAMP\r\n"
           "a=tsdelay:37\r\n";
}

static std::string make_video_sdp_with_par(std::string_view par_token) {
    return std::string{"v=0\r\n"
                       "o=- 0 0 IN IP4 127.0.0.1\r\n"
                       "s=ST2110 test\r\n"
                       "t=0 0\r\n"
                       "m=video 5004 RTP/AVP 96\r\n"
                       "a=rtpmap:96 raw/90000\r\n"
                       "a=fmtp:96 sampling=YCbCr-4:2:2; width=1920; height=1080; exactframerate=60000/1001; depth=10; "
                       "colorimetry=BT709; PM=2110GPM; SSN=ST2110-20:2017; TCS=SDR; RANGE=FULL; TP=2110TPN; PAR="} +
           std::string{par_token} +
           "\r\n"
           "a=ts-refclk:ptp=IEEE1588-2008:39-A7-94-FF-FE-07-CB-D0:127\r\n"
           "a=mediaclk:direct=0\r\n";
}

static void test_parses_full_valid_video_sdp_into_signaling() {
    const std::string sdp = make_valid_video_sdp();

    auto signaling_expected = parse_video_stream_signaling_from_sdp(sdp, 96);
    assert(signaling_expected.has_value());

    const auto &signaling = *signaling_expected;

    assert(signaling.media.width == 1920);
    assert(signaling.media.height == 1080);
    assert(signaling.media.fps_num == 60000);
    assert(signaling.media.fps_den == 1001);

    assert(signaling.scan_mode == VideoScanMode::Progressive);
    assert(signaling.packing_mode == VideoPackingMode::Gpm);

    assert(signaling.media.sampling.known == VideoSampling::Known::YCbCr422);
    assert(signaling.media.depth.bits == 10);
    assert(signaling.media.colorimetry.known == VideoColorimetry::Known::Bt709);
    assert(signaling.media.pixel_aspect_ratio.width == 1);
    assert(signaling.media.pixel_aspect_ratio.height == 1);

    assert(signaling.reference_clock.kind == ReferenceClockKind::Ptp);
    assert(signaling.reference_clock.ptp.has_value());
    assert(signaling.reference_clock.ptp->domain_number == 127);
    assert(!signaling.reference_clock.ptp->traceable);

    assert(signaling.media_clock_mode == MediaClockMode::Direct);
    assert(signaling.timestamp_mode == TimestampMode::Samp);
    assert(signaling.sender_type == VideoSenderType::Narrow);

    assert(signaling.ts_delay_sender_ticks == 37);

    assert(!signaling.troff_us.has_value());
    assert(!signaling.cmax.has_value());

    assert(validate_video_stream_signaling(signaling) == Error::Ok);
}

static void test_raw_media_section_composition_matches_full_sdp_entry_point() {
    const std::string sdp = make_valid_video_sdp();

    auto raw_expected = select_raw_video_sdp_media_section(sdp, 96);
    assert(raw_expected.has_value());

    auto from_raw_expected = video_stream_signaling_from_raw_video_sdp_media_section(*raw_expected);
    assert(from_raw_expected.has_value());

    auto from_sdp_expected = parse_video_stream_signaling_from_sdp(sdp, 96);
    assert(from_sdp_expected.has_value());

    const auto &from_raw = *from_raw_expected;
    const auto &from_sdp = *from_sdp_expected;

    assert(from_raw.media.width == from_sdp.media.width);
    assert(from_raw.media.height == from_sdp.media.height);
    assert(from_raw.media.fps_num == from_sdp.media.fps_num);
    assert(from_raw.media.fps_den == from_sdp.media.fps_den);
    assert(from_raw.media.pixel_aspect_ratio.width == from_sdp.media.pixel_aspect_ratio.width);
    assert(from_raw.media.pixel_aspect_ratio.height == from_sdp.media.pixel_aspect_ratio.height);

    assert(from_raw.scan_mode == from_sdp.scan_mode);
    assert(from_raw.packing_mode == from_sdp.packing_mode);

    assert(from_raw.reference_clock.kind == from_sdp.reference_clock.kind);
    assert(from_raw.media_clock_mode == from_sdp.media_clock_mode);
    assert(from_raw.timestamp_mode == from_sdp.timestamp_mode);
    assert(from_raw.sender_type == from_sdp.sender_type);

    assert(from_raw.ts_delay_sender_ticks == from_sdp.ts_delay_sender_ticks);
    assert(from_raw.troff_us == from_sdp.troff_us);
    assert(from_raw.cmax == from_sdp.cmax);
}

static void test_final_ingestion_maps_non_square_par_into_signaling() {
    const std::string sdp = make_video_sdp_with_par("12:11");

    auto signaling_expected = parse_video_stream_signaling_from_sdp(sdp, 96);
    assert(signaling_expected.has_value());

    const auto &signaling = *signaling_expected;
    assert(signaling.media.pixel_aspect_ratio.width == 12);
    assert(signaling.media.pixel_aspect_ratio.height == 11);
    assert(validate_video_stream_signaling(signaling) == Error::Ok);
}

static void test_final_ingestion_accepts_explicit_square_par() {
    const std::string sdp = make_video_sdp_with_par("1:1");

    auto signaling_expected = parse_video_stream_signaling_from_sdp(sdp, 96);
    assert(signaling_expected.has_value());

    const auto &signaling = *signaling_expected;
    assert(signaling.media.pixel_aspect_ratio.width == 1);
    assert(signaling.media.pixel_aspect_ratio.height == 1);
}

static void test_final_ingestion_rejects_malformed_par() {
    const std::string sdp = make_video_sdp_with_par("1:");

    auto signaling_expected = parse_video_stream_signaling_from_sdp(sdp, 96);
    assert(!signaling_expected.has_value());
    assert(signaling_expected.error() == Error::InvalidValue);
}

static void test_rejects_invalid_rtpmap_clock_rate_in_final_ingestion() {
    std::string sdp = make_valid_video_sdp();

    const std::string good = "a=rtpmap:96 raw/90000\r\n";
    const std::string bad = "a=rtpmap:96 raw/48000\r\n";

    const std::size_t pos = sdp.find(good);
    assert(pos != std::string::npos);
    sdp.replace(pos, good.size(), bad);

    auto signaling_expected = parse_video_stream_signaling_from_sdp(sdp, 96);
    assert(!signaling_expected.has_value());
    assert(signaling_expected.error() == Error::InvalidValue);
}

static void test_rejects_invalid_rtpmap_encoding_name_in_final_ingestion() {
    std::string sdp = make_valid_video_sdp();

    const std::string good = "a=rtpmap:96 raw/90000\r\n";
    const std::string bad = "a=rtpmap:96 L16/90000\r\n";

    const std::size_t pos = sdp.find(good);
    assert(pos != std::string::npos);
    sdp.replace(pos, good.size(), bad);

    auto signaling_expected = parse_video_stream_signaling_from_sdp(sdp, 96);
    assert(!signaling_expected.has_value());
    assert(signaling_expected.error() == Error::InvalidValue);
}

static void test_propagates_invalid_timing_attribute_error_from_final_ingestion() {
    std::string sdp = make_valid_video_sdp();

    const std::string good = "a=mediaclk:direct=0\r\n";
    const std::string bad = "a=mediaclk:direct=abc\r\n";

    const std::size_t pos = sdp.find(good);
    assert(pos != std::string::npos);
    sdp.replace(pos, good.size(), bad);

    auto signaling_expected = parse_video_stream_signaling_from_sdp(sdp, 96);
    assert(!signaling_expected.has_value());
    assert(signaling_expected.error() == Error::InvalidValue);
}

static void test_rejects_non_zero_direct_media_clock_offset() {
    std::string sdp = make_valid_video_sdp();

    const std::string good = "a=mediaclk:direct=0\r\n";
    const std::string bad = "a=mediaclk:direct=42\r\n";

    const std::size_t pos = sdp.find(good);
    assert(pos != std::string::npos);
    sdp.replace(pos, good.size(), bad);

    auto signaling_expected = parse_video_stream_signaling_from_sdp(sdp, 96);
    assert(!signaling_expected.has_value());
    assert(signaling_expected.error() == Error::InvalidValue);
}

static void test_rejects_missing_tp_in_final_ingestion() {
    std::string sdp = make_valid_video_sdp();

    const std::string good = "TP=2110TPN";
    const std::size_t pos = sdp.find(good);
    assert(pos != std::string::npos);
    sdp.erase(pos, good.size());

    auto signaling_expected = parse_video_stream_signaling_from_sdp(sdp, 96);
    assert(!signaling_expected.has_value());
    assert(signaling_expected.error() == Error::InvalidValue);
}

static void test_accepts_narrow_sender_troff_and_cmax_in_final_ingestion() {
    std::string sdp = make_valid_video_sdp();

    const std::string good = "TP=2110TPN";
    const std::string replacement = "TP=2110TPN; TROFF=11; CMAX=2";

    const std::size_t pos = sdp.find(good);
    assert(pos != std::string::npos);
    sdp.replace(pos, good.size(), replacement);

    auto signaling_expected = parse_video_stream_signaling_from_sdp(sdp, 96);
    assert(signaling_expected.has_value());
    assert(signaling_expected->sender_type == VideoSenderType::Narrow);
    assert(signaling_expected->troff_us.has_value());
    assert(*signaling_expected->troff_us == 11);
    assert(signaling_expected->cmax.has_value());
    assert(*signaling_expected->cmax == 2);
}

static void test_rejects_zero_troff_in_final_ingestion() {
    std::string sdp = make_valid_video_sdp();

    const std::string good = "TP=2110TPN";
    const std::string replacement = "TP=2110TPN; TROFF=0";

    const std::size_t pos = sdp.find(good);
    assert(pos != std::string::npos);
    sdp.replace(pos, good.size(), replacement);

    auto signaling_expected = parse_video_stream_signaling_from_sdp(sdp, 96);
    assert(!signaling_expected.has_value());
    assert(signaling_expected.error() == Error::InvalidValue);
}

int main() {
    test_parses_full_valid_video_sdp_into_signaling();
    test_raw_media_section_composition_matches_full_sdp_entry_point();
    test_final_ingestion_maps_non_square_par_into_signaling();
    test_final_ingestion_accepts_explicit_square_par();
    test_final_ingestion_rejects_malformed_par();
    test_rejects_invalid_rtpmap_clock_rate_in_final_ingestion();
    test_rejects_invalid_rtpmap_encoding_name_in_final_ingestion();
    test_propagates_invalid_timing_attribute_error_from_final_ingestion();
    test_rejects_non_zero_direct_media_clock_offset();
    test_rejects_missing_tp_in_final_ingestion();
    test_accepts_narrow_sender_troff_and_cmax_in_final_ingestion();
    test_rejects_zero_troff_in_final_ingestion();
    return 0;
}
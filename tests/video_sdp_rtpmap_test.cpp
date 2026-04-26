#include "st2110/video_sdp_rtpmap.hpp"

#include <cassert>
#include <string>
#include <vector>

using namespace st2110;

static void test_parse_video_sdp_rtpmap_payload_parses_basic_form() {
    auto parsed = parse_video_sdp_rtpmap_payload("raw/90000");
    assert(parsed.has_value());

    assert(parsed->encoding_name == "raw");
    assert(parsed->clock_rate == 90000);
    assert(!parsed->encoding_parameters.has_value());
}

static void test_parse_video_sdp_rtpmap_payload_parses_optional_encoding_parameters() {
    auto parsed = parse_video_sdp_rtpmap_payload("raw/90000/2");
    assert(parsed.has_value());

    assert(parsed->encoding_name == "raw");
    assert(parsed->clock_rate == 90000);
    assert(parsed->encoding_parameters.has_value());
    assert(*parsed->encoding_parameters == "2");
}

static void test_parse_video_sdp_rtpmap_payload_rejects_missing_clock_rate() {
    auto parsed = parse_video_sdp_rtpmap_payload("raw");
    assert(!parsed.has_value());
    assert(parsed.error() == Error::InvalidValue);
}

static void test_parse_video_sdp_rtpmap_payload_rejects_empty_encoding_name() {
    auto parsed = parse_video_sdp_rtpmap_payload("/90000");
    assert(!parsed.has_value());
    assert(parsed.error() == Error::InvalidValue);
}

static void test_parse_video_sdp_rtpmap_payload_rejects_zero_clock_rate() {
    auto parsed = parse_video_sdp_rtpmap_payload("raw/0");
    assert(!parsed.has_value());
    assert(parsed.error() == Error::InvalidValue);
}

static void test_parse_video_sdp_rtpmap_payload_rejects_empty_encoding_parameters() {
    auto parsed = parse_video_sdp_rtpmap_payload("raw/90000/");
    assert(!parsed.has_value());
    assert(parsed.error() == Error::InvalidValue);
}

static void test_parse_video_sdp_rtpmap_payload_rejects_too_many_components() {
    auto parsed = parse_video_sdp_rtpmap_payload("raw/90000/2/extra");
    assert(!parsed.has_value());
    assert(parsed.error() == Error::InvalidValue);
}

static void test_parse_video_sdp_rtpmap_attribute_parses_matching_payload_type() {
    auto parsed = parse_video_sdp_rtpmap_attribute("a=rtpmap:112 raw/90000", 112);
    assert(parsed.has_value());
    assert(parsed->has_value());

    const auto& rtpmap = **parsed;
    assert(rtpmap.encoding_name == "raw");
    assert(rtpmap.clock_rate == 90000);
    assert(!rtpmap.encoding_parameters.has_value());
}

static void test_parse_video_sdp_rtpmap_attribute_returns_nullopt_on_payload_type_mismatch() {
    auto parsed = parse_video_sdp_rtpmap_attribute("a=rtpmap:111 raw/90000", 112);
    assert(parsed.has_value());
    assert(!parsed->has_value());
}

static void test_parse_video_sdp_rtpmap_attribute_rejects_invalid_syntax() {
    auto parsed = parse_video_sdp_rtpmap_attribute("a=rtpmap:112 raw", 112);
    assert(!parsed.has_value());
    assert(parsed.error() == Error::InvalidValue);
}

static void test_parse_video_sdp_rtpmap_attribute_rejects_invalid_payload_type_token() {
    auto parsed = parse_video_sdp_rtpmap_attribute("a=rtpmap:abc raw/90000", 112);
    assert(!parsed.has_value());
    assert(parsed.error() == Error::InvalidValue);
}

static void test_parse_video_sdp_rtpmap_from_media_section_parses_bound_rtpmap() {
    RawVideoSdpMediaSection raw{};
    raw.media_line = "m=video 50000 RTP/AVP 112";
    raw.payload_type = 112;
    raw.media_payload_types = {112};
    raw.rtpmap = "raw/90000";
    raw.fmtp = "sampling=YCbCr-4:2:2; width=1920; height=1080; exactframerate=60000/1001; depth=10; colorimetry=BT709; PM=2110GPM; SSN=ST2110-20:2022";

    auto parsed = parse_video_sdp_rtpmap_from_media_section(raw);
    assert(parsed.has_value());

    assert(parsed->encoding_name == "raw");
    assert(parsed->clock_rate == 90000);
    assert(!parsed->encoding_parameters.has_value());
}

static void test_parse_video_sdp_rtpmap_from_media_section_rejects_missing_rtpmap_binding() {
    RawVideoSdpMediaSection raw{};
    raw.media_line = "m=video 50000 RTP/AVP 112";
    raw.payload_type = 112;
    raw.media_payload_types = {112};
    raw.rtpmap = "";
    raw.fmtp = "anything";

    auto parsed = parse_video_sdp_rtpmap_from_media_section(raw);
    assert(!parsed.has_value());
    assert(parsed.error() == Error::InvalidValue);
}

static void test_parse_video_sdp_rtpmap_from_media_section_rejects_payload_type_not_in_media_payload_types() {
    RawVideoSdpMediaSection raw{};
    raw.media_line = "m=video 50000 RTP/AVP 113";
    raw.payload_type = 112;
    raw.media_payload_types = {113};
    raw.rtpmap = "raw/90000";
    raw.fmtp = "anything";

    auto parsed = parse_video_sdp_rtpmap_from_media_section(raw);
    assert(!parsed.has_value());
    assert(parsed.error() == Error::InvalidValue);
}

static void test_parse_video_sdp_rtpmap_from_media_section_rejects_broken_bound_rtpmap_syntax() {
    RawVideoSdpMediaSection raw{};
    raw.media_line = "m=video 50000 RTP/AVP 112";
    raw.payload_type = 112;
    raw.media_payload_types = {112};
    raw.rtpmap = "raw";
    raw.fmtp = "anything";

    auto parsed = parse_video_sdp_rtpmap_from_media_section(raw);
    assert(!parsed.has_value());
    assert(parsed.error() == Error::InvalidValue);
}

int main() {
    test_parse_video_sdp_rtpmap_payload_parses_basic_form();
    test_parse_video_sdp_rtpmap_payload_parses_optional_encoding_parameters();
    test_parse_video_sdp_rtpmap_payload_rejects_missing_clock_rate();
    test_parse_video_sdp_rtpmap_payload_rejects_empty_encoding_name();
    test_parse_video_sdp_rtpmap_payload_rejects_zero_clock_rate();
    test_parse_video_sdp_rtpmap_payload_rejects_empty_encoding_parameters();
    test_parse_video_sdp_rtpmap_payload_rejects_too_many_components();

    test_parse_video_sdp_rtpmap_attribute_parses_matching_payload_type();
    test_parse_video_sdp_rtpmap_attribute_returns_nullopt_on_payload_type_mismatch();
    test_parse_video_sdp_rtpmap_attribute_rejects_invalid_syntax();
    test_parse_video_sdp_rtpmap_attribute_rejects_invalid_payload_type_token();

    test_parse_video_sdp_rtpmap_from_media_section_parses_bound_rtpmap();
    test_parse_video_sdp_rtpmap_from_media_section_rejects_missing_rtpmap_binding();
    test_parse_video_sdp_rtpmap_from_media_section_rejects_payload_type_not_in_media_payload_types();
    test_parse_video_sdp_rtpmap_from_media_section_rejects_broken_bound_rtpmap_syntax();

    return 0;
}
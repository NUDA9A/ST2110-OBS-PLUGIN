#include "st2110/foundation/error.hpp"
#include "st2110/ingress/video/video_sdp_media_section.hpp"

#include <cassert>
#include <expected>
#include <string_view>

using namespace st2110;

static void expect_invalid(const std::expected<RawVideoSdpMediaSection, Error> &result) {
    assert(!result.has_value());
    assert(result.error() == Error::InvalidValue);
}

static void test_selects_matching_video_media_section() {
    constexpr std::string_view sdp = "v=0\r\n"
                                     "o=- 1 1 IN IP4 127.0.0.1\r\n"
                                     "s=test\r\n"
                                     "t=0 0\r\n"
                                     "m=audio 50002 RTP/AVP 96\r\n"
                                     "a=rtpmap:96 L24/48000/2\r\n"
                                     "m=video 50000 RTP/AVP 112\r\n"
                                     "a=rtpmap:112 raw/90000\r\n"
                                     "a=fmtp:112 sampling=YCbCr-4:2:2; width=1920; height=1080; exactframerate=25; "
                                     "depth=8; PM=2110GPM; SSN=ST2110-20:2022\r\n"
                                     "a=ts-refclk:ptp=IEEE1588-2008:39-A7-94-FF-FE-07-CB-D0:37\r\n"
                                     "a=mediaclk:direct=0\r\n"
                                     "a=tsmode:NEW\r\n"
                                     "a=tsdelay:180\r\n"
                                     "a=TP:2110TPN\r\n"
                                     "a=TROFF:12\r\n"
                                     "a=CMAX:4\r\n"
                                     "a=x-extra:one\r\n"
                                     "m=video 50004 RTP/AVP 113\r\n"
                                     "a=rtpmap:113 raw/90000\r\n"
                                     "a=fmtp:113 sampling=YCbCr-4:4:4; width=1280; height=720; exactframerate=50; "
                                     "depth=10; PM=2110BPM; SSN=ST2110-20:2022\r\n";

    auto result = select_raw_video_sdp_media_section(sdp, 112);
    assert(result.has_value());

    const auto &section = *result;
    assert(section.media_line == "m=video 50000 RTP/AVP 112");
    assert(section.payload_type == 112);
    assert(section.media_payload_types.size() == 1);
    assert(section.media_payload_types[0] == 112);

    assert(section.rtpmap == "raw/90000");
    assert(section.fmtp ==
           "sampling=YCbCr-4:2:2; width=1920; height=1080; exactframerate=25; depth=8; PM=2110GPM; SSN=ST2110-20:2022");

    assert(section.ts_refclk.has_value());
    assert(*section.ts_refclk == "ptp=IEEE1588-2008:39-A7-94-FF-FE-07-CB-D0:37");

    assert(section.mediaclk.has_value());
    assert(*section.mediaclk == "direct=0");

    assert(section.tsmode.has_value());
    assert(*section.tsmode == "NEW");

    assert(section.tsdelay.has_value());
    assert(*section.tsdelay == "180");

    assert(section.tp.has_value());
    assert(*section.tp == "2110TPN");

    assert(section.troff.has_value());
    assert(*section.troff == "12");

    assert(section.cmax.has_value());
    assert(*section.cmax == "4");

    assert(section.unknown_attributes.size() == 1);
    assert(section.unknown_attributes[0].name == "x-extra");
    assert(section.unknown_attributes[0].value == "one");
}

static void test_rejects_payload_type_mismatch() {
    constexpr std::string_view sdp = "v=0\r\n"
                                     "m=video 50000 RTP/AVP 113\r\n"
                                     "a=rtpmap:113 raw/90000\r\n"
                                     "a=fmtp:113 sampling=YCbCr-4:2:2; width=1920; height=1080; exactframerate=25; "
                                     "depth=8; PM=2110GPM; SSN=ST2110-20:2022\r\n";

    expect_invalid(select_raw_video_sdp_media_section(sdp, 112));
}

static void test_rejects_missing_required_rtpmap_association() {
    constexpr std::string_view sdp = "v=0\r\n"
                                     "m=video 50000 RTP/AVP 112\r\n"
                                     "a=rtpmap:113 raw/90000\r\n"
                                     "a=fmtp:112 sampling=YCbCr-4:2:2; width=1920; height=1080; exactframerate=25; "
                                     "depth=8; PM=2110GPM; SSN=ST2110-20:2022\r\n";

    expect_invalid(select_raw_video_sdp_media_section(sdp, 112));
}

static void test_rejects_duplicate_relevant_attributes() {
    constexpr std::string_view duplicate_tsmode_sdp = "v=0\r\n"
                                                      "m=video 50000 RTP/AVP 112\r\n"
                                                      "a=rtpmap:112 raw/90000\r\n"
                                                      "a=fmtp:112 sampling=YCbCr-4:2:2; width=1920; height=1080; "
                                                      "exactframerate=25; depth=8; PM=2110GPM; SSN=ST2110-20:2022\r\n"
                                                      "a=tsmode:NEW\r\n"
                                                      "a=tsmode:PRES\r\n";

    expect_invalid(select_raw_video_sdp_media_section(duplicate_tsmode_sdp, 112));

    constexpr std::string_view duplicate_tsdelay_sdp = "v=0\r\n"
                                                       "m=video 50000 RTP/AVP 112\r\n"
                                                       "a=rtpmap:112 raw/90000\r\n"
                                                       "a=fmtp:112 sampling=YCbCr-4:2:2; width=1920; height=1080; "
                                                       "exactframerate=25; depth=8; PM=2110GPM; SSN=ST2110-20:2022\r\n"
                                                       "a=tsdelay:100\r\n"
                                                       "a=tsdelay:200\r\n";

    expect_invalid(select_raw_video_sdp_media_section(duplicate_tsdelay_sdp, 112));

    constexpr std::string_view duplicate_tp_sdp = "v=0\r\n"
                                                  "m=video 50000 RTP/AVP 112\r\n"
                                                  "a=rtpmap:112 raw/90000\r\n"
                                                  "a=fmtp:112 sampling=YCbCr-4:2:2; width=1920; height=1080; "
                                                  "exactframerate=25; depth=8; PM=2110GPM; SSN=ST2110-20:2022\r\n"
                                                  "a=TP:2110TPN\r\n"
                                                  "a=TP:2110TPNL\r\n";

    expect_invalid(select_raw_video_sdp_media_section(duplicate_tp_sdp, 112));

    constexpr std::string_view duplicate_troff_sdp = "v=0\r\n"
                                                     "m=video 50000 RTP/AVP 112\r\n"
                                                     "a=rtpmap:112 raw/90000\r\n"
                                                     "a=fmtp:112 sampling=YCbCr-4:2:2; width=1920; height=1080; "
                                                     "exactframerate=25; depth=8; PM=2110GPM; SSN=ST2110-20:2022\r\n"
                                                     "a=TROFF:10\r\n"
                                                     "a=TROFF:20\r\n";

    expect_invalid(select_raw_video_sdp_media_section(duplicate_troff_sdp, 112));

    constexpr std::string_view duplicate_cmax_sdp = "v=0\r\n"
                                                    "m=video 50000 RTP/AVP 112\r\n"
                                                    "a=rtpmap:112 raw/90000\r\n"
                                                    "a=fmtp:112 sampling=YCbCr-4:2:2; width=1920; height=1080; "
                                                    "exactframerate=25; depth=8; PM=2110GPM; SSN=ST2110-20:2022\r\n"
                                                    "a=CMAX:4\r\n"
                                                    "a=CMAX:8\r\n";

    expect_invalid(select_raw_video_sdp_media_section(duplicate_cmax_sdp, 112));
}

static void test_preserves_unknown_attributes() {
    constexpr std::string_view sdp = "v=0\r\n"
                                     "m=video 50000 RTP/AVP 112\r\n"
                                     "a=rtpmap:112 raw/90000\r\n"
                                     "a=fmtp:112 sampling=YCbCr-4:2:2; width=1920; height=1080; exactframerate=25; "
                                     "depth=8; PM=2110GPM; SSN=ST2110-20:2022\r\n"
                                     "a=recvonly\r\n"
                                     "a=x-test:value\r\n";

    auto result = select_raw_video_sdp_media_section(sdp, 112);
    assert(result.has_value());

    const auto &section = *result;
    assert(section.unknown_attributes.size() == 2);

    assert(section.unknown_attributes[0].name == "recvonly");
    assert(section.unknown_attributes[0].value.empty());

    assert(section.unknown_attributes[1].name == "x-test");
    assert(section.unknown_attributes[1].value == "value");
}

static void test_rejects_non_dynamic_payload_type_for_video_media_line() {
    constexpr std::string_view sdp = "v=0\r\n"
                                     "m=video 50000 RTP/AVP 34\r\n"
                                     "a=rtpmap:34 raw/90000\r\n"
                                     "a=fmtp:34 sampling=YCbCr-4:2:2; width=1920; height=1080; exactframerate=25; "
                                     "depth=8; PM=2110GPM; SSN=ST2110-20:2022\r\n";

    expect_invalid(select_raw_video_sdp_media_section(sdp, 34));
}

static void test_rejects_malformed_video_media_line_port_token() {
    constexpr std::string_view bad_alpha_port = "v=0\r\n"
                                                "m=video abc RTP/AVP 112\r\n"
                                                "a=rtpmap:112 raw/90000\r\n"
                                                "a=fmtp:112 sampling=YCbCr-4:2:2; width=1920; height=1080; "
                                                "exactframerate=25; depth=8; PM=2110GPM; SSN=ST2110-20:2022\r\n";

    expect_invalid(select_raw_video_sdp_media_section(bad_alpha_port, 112));

    constexpr std::string_view bad_zero_port = "v=0\r\n"
                                               "m=video 0 RTP/AVP 112\r\n"
                                               "a=rtpmap:112 raw/90000\r\n"
                                               "a=fmtp:112 sampling=YCbCr-4:2:2; width=1920; height=1080; "
                                               "exactframerate=25; depth=8; PM=2110GPM; SSN=ST2110-20:2022\r\n";

    expect_invalid(select_raw_video_sdp_media_section(bad_zero_port, 112));

    constexpr std::string_view bad_slash_port = "v=0\r\n"
                                                "m=video 50000/2 RTP/AVP 112\r\n"
                                                "a=rtpmap:112 raw/90000\r\n"
                                                "a=fmtp:112 sampling=YCbCr-4:2:2; width=1920; height=1080; "
                                                "exactframerate=25; depth=8; PM=2110GPM; SSN=ST2110-20:2022\r\n";

    expect_invalid(select_raw_video_sdp_media_section(bad_slash_port, 112));
}

static void test_rejects_unexpected_video_media_line_protocol() {
    constexpr std::string_view sdp = "v=0\r\n"
                                     "m=video 50000 RTP/SAVP 112\r\n"
                                     "a=rtpmap:112 raw/90000\r\n"
                                     "a=fmtp:112 sampling=YCbCr-4:2:2; width=1920; height=1080; exactframerate=25; "
                                     "depth=8; PM=2110GPM; SSN=ST2110-20:2022\r\n";

    expect_invalid(select_raw_video_sdp_media_section(sdp, 112));
}

int main() {
    test_selects_matching_video_media_section();
    test_rejects_payload_type_mismatch();
    test_rejects_missing_required_rtpmap_association();
    test_rejects_duplicate_relevant_attributes();
    test_preserves_unknown_attributes();

    test_rejects_non_dynamic_payload_type_for_video_media_line();
    test_rejects_malformed_video_media_line_port_token();
    test_rejects_unexpected_video_media_line_protocol();

    return 0;
}
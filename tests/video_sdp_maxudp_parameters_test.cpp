#include "st2110/packet_parse.hpp"
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

static std::string make_base_fmtp_payload() {
    return "sampling=YCbCr-4:2:2; "
           "width=1920; "
           "height=1080; "
           "exactframerate=60000/1001; "
           "depth=10; "
           "colorimetry=BT709; "
           "PM=2110GPM; "
           "SSN=ST2110-20:2022; "
           "TCS=SDR; "
           "RANGE=FULL";
}

static std::string make_video_sdp_with_fmtp(std::string_view fmtp_payload) {
    std::string sdp;

    sdp += "v=0\r\n";
    sdp += "o=- 0 0 IN IP4 127.0.0.1\r\n";
    sdp += "s=ST2110 MAXUDP test\r\n";
    sdp += "t=0 0\r\n";
    sdp += "m=video 5004 RTP/AVP 96\r\n";
    sdp += "a=rtpmap:96 raw/90000\r\n";
    sdp += "a=fmtp:96 ";
    sdp += fmtp_payload;
    sdp += "\r\n";
    sdp += "a=ts-refclk:ptp=IEEE1588-2008:39-A7-94-FF-FE-07-CB-D0:127\r\n";
    sdp += "a=mediaclk:direct=0\r\n";

    return sdp;
}

static void test_fmtp_parser_extracts_maxudp_as_known_parameter() {
    const std::string payload = make_base_fmtp_payload() + "; MAXUDP=8960; "
                                                           "X-FUTURE-PARAM=future";

    auto parsed = parse_video_sdp_fmtp_payload(payload);
    assert(parsed.has_value());

    const auto &fmtp = *parsed;

    assert(fmtp.max_udp_datagram_bytes.has_value());
    assert(*fmtp.max_udp_datagram_bytes == 8960);

    assert(!fmtp_has_unknown_named(fmtp, "MAXUDP"));
    assert(fmtp_has_unknown_named(fmtp, "X-FUTURE-PARAM"));
}

static void test_fmtp_parser_rejects_duplicate_maxudp() {
    const std::string payload = make_base_fmtp_payload() + "; MAXUDP=1460; "
                                                           "MAXUDP=8960";

    auto parsed = parse_video_sdp_fmtp_payload(payload);
    assert(!parsed.has_value());
    assert(parsed.error() == Error::InvalidValue);
}

static void test_fmtp_parser_rejects_malformed_maxudp() {
    const std::string payload = make_base_fmtp_payload() + "; MAXUDP=not-a-number";

    auto parsed = parse_video_sdp_fmtp_payload(payload);
    assert(!parsed.has_value());
    assert(parsed.error() == Error::InvalidValue);
}

static void test_final_ingestion_maps_maxudp_to_signaling() {
    const std::string payload = make_base_fmtp_payload() + "; MAXUDP=8960";

    const std::string sdp = make_video_sdp_with_fmtp(payload);

    auto signaling_expected = parse_video_stream_signaling_from_sdp(sdp, 96);
    assert(signaling_expected.has_value());

    const auto &signaling = *signaling_expected;

    assert(signaling.max_udp_datagram_bytes.has_value());
    assert(*signaling.max_udp_datagram_bytes == 8960);

    assert(validate_video_stream_signaling(signaling) == Error::Ok);
}

static void test_packet_parse_policy_uses_signaled_maxudp() {
    const std::string payload = make_base_fmtp_payload() + "; MAXUDP=8960";

    const std::string sdp = make_video_sdp_with_fmtp(payload);

    auto signaling_expected = parse_video_stream_signaling_from_sdp(sdp, 96);
    assert(signaling_expected.has_value());

    PacketParsePolicy policy = packet_parse_policy_from_video_stream_signaling(*signaling_expected);

    assert(policy.max_udp_datagram_bytes.has_value());
    assert(*policy.max_udp_datagram_bytes == 8960);
    assert(effective_max_udp_datagram_bytes(policy) == 8960);
}

static void test_absent_maxudp_preserves_default_packet_parse_policy() {
    const std::string sdp = make_video_sdp_with_fmtp(make_base_fmtp_payload());

    auto signaling_expected = parse_video_stream_signaling_from_sdp(sdp, 96);
    assert(signaling_expected.has_value());

    const auto &signaling = *signaling_expected;

    assert(!signaling.max_udp_datagram_bytes.has_value());

    PacketParsePolicy policy = packet_parse_policy_from_video_stream_signaling(signaling);

    assert(!policy.max_udp_datagram_bytes.has_value());
    assert(effective_max_udp_datagram_bytes(policy) == standardUdpDatagramSizeLimitBytes);
}

static void test_final_ingestion_rejects_too_small_maxudp_via_signaling_validation() {
    const std::string payload = make_base_fmtp_payload() + "; MAXUDP=1";

    const std::string sdp = make_video_sdp_with_fmtp(payload);

    auto signaling_expected = parse_video_stream_signaling_from_sdp(sdp, 96);
    assert(!signaling_expected.has_value());
    assert(signaling_expected.error() == Error::InvalidValue);
}

int main() {
    test_fmtp_parser_extracts_maxudp_as_known_parameter();
    test_fmtp_parser_rejects_duplicate_maxudp();
    test_fmtp_parser_rejects_malformed_maxudp();
    test_final_ingestion_maps_maxudp_to_signaling();
    test_packet_parse_policy_uses_signaled_maxudp();
    test_absent_maxudp_preserves_default_packet_parse_policy();
    test_final_ingestion_rejects_too_small_maxudp_via_signaling_validation();

    return 0;
}
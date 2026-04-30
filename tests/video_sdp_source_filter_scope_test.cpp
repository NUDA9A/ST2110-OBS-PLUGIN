#include "st2110/video_sdp_media_section.hpp"

#include <cassert>
#include <string_view>

using namespace st2110;

namespace {
constexpr std::string_view valid_sdp_with_session_and_media_source_filters =
    "v=0\n"
    "o=- 1 1 IN IP4 192.0.2.1\n"
    "s=ST2110 source-filter test\n"
    "t=0 0\n"
    "c=IN IP4 239.1.1.1/32\n"
    "a=source-filter: incl IN IP4 239.1.1.1 198.51.100.10 198.51.100.11\n"
    "m=video 50000 RTP/AVP 112\n"
    "a=mid:primary\n"
    "a=source-filter: excl IN IP6 ff15::abcd 2001:db8::10\n"
    "a=rtpmap:112 raw/90000\n"
    "a=fmtp:112 sampling=YCbCr-4:2:2; width=1920; height=1080; exactframerate=25; depth=8; colorimetry=BT709; "
    "PM=2110GPM; SSN=ST2110-20:2022\n";

constexpr std::string_view malformed_source_filter_sdp =
    "v=0\n"
    "o=- 1 1 IN IP4 192.0.2.1\n"
    "s=ST2110 malformed source-filter test\n"
    "t=0 0\n"
    "a=source-filter: incl IN IP4 239.1.1.1\n"
    "m=video 50000 RTP/AVP 112\n"
    "a=rtpmap:112 raw/90000\n"
    "a=fmtp:112 sampling=YCbCr-4:2:2; width=1920; height=1080; exactframerate=25; depth=8; colorimetry=BT709; "
    "PM=2110GPM; SSN=ST2110-20:2022\n";

void test_session_and_media_source_filters_preserve_scope_and_fields() {
    const auto parsed = select_raw_video_sdp_media_section(valid_sdp_with_session_and_media_source_filters, 112);

    assert(parsed.has_value());
    assert(parsed->source_filters.size() == 2);

    const auto &session_filter = parsed->source_filters[0];
    assert(session_filter.scope == RawSdpSourceFilter::Scope::Session);
    assert(session_filter.raw_value == "incl IN IP4 239.1.1.1 198.51.100.10 198.51.100.11");
    assert(session_filter.filter_mode == "incl");
    assert(session_filter.network_type == "IN");
    assert(session_filter.address_type == "IP4");
    assert(session_filter.destination_address == "239.1.1.1");
    assert(session_filter.source_addresses.size() == 2);
    assert(session_filter.source_addresses[0] == "198.51.100.10");
    assert(session_filter.source_addresses[1] == "198.51.100.11");

    const auto &media_filter = parsed->source_filters[1];
    assert(media_filter.scope == RawSdpSourceFilter::Scope::Media);
    assert(media_filter.raw_value == "excl IN IP6 ff15::abcd 2001:db8::10");
    assert(media_filter.filter_mode == "excl");
    assert(media_filter.network_type == "IN");
    assert(media_filter.address_type == "IP6");
    assert(media_filter.destination_address == "ff15::abcd");
    assert(media_filter.source_addresses.size() == 1);
    assert(media_filter.source_addresses[0] == "2001:db8::10");
}

void test_structurally_malformed_source_filter_is_rejected() {
    const auto parsed = select_raw_video_sdp_media_section(malformed_source_filter_sdp, 112);

    assert(!parsed.has_value());
    assert(parsed.error() == Error::InvalidValue);
}

void test_source_filter_parser_is_runtime_agnostic() {
    const auto parsed =
        parse_source_filter_attribute_value("incl IN IP6 ff15::abcd 2001:db8::10", RawSdpSourceFilter::Scope::Session);

    assert(parsed.has_value());
    assert(parsed->scope == RawSdpSourceFilter::Scope::Session);
    assert(parsed->address_type == "IP6");
    assert(parsed->destination_address == "ff15::abcd");
    assert(parsed->source_addresses.size() == 1);
    assert(parsed->source_addresses[0] == "2001:db8::10");
}
} // namespace

int main() {
    test_session_and_media_source_filters_preserve_scope_and_fields();
    test_structurally_malformed_source_filter_is_rejected();
    test_source_filter_parser_is_runtime_agnostic();

    return 0;
}
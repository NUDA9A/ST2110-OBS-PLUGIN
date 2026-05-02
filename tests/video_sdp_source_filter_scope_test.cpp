#include "st2110/video_sdp_ingestion.hpp"
#include "st2110/video_sdp_media_section.hpp"

#include <cassert>
#include <string>
#include <string_view>

using namespace st2110;

namespace {

constexpr std::string_view kBaseFmtp =
    "sampling=YCbCr-4:2:2; width=1920; height=1080; exactframerate=25; "
    "depth=8; colorimetry=BT709; PM=2110GPM; SSN=ST2110-20:2017";

std::string make_video_sdp(std::string_view session_source_filter_line,
                           std::string_view media_source_filter_line) {
    std::string sdp =
        "v=0\n"
        "o=- 1 1 IN IP4 192.0.2.1\n"
        "s=ST2110 source-filter grammar test\n"
        "t=0 0\n";

    if (!session_source_filter_line.empty()) {
        sdp += "a=source-filter:";
        sdp += session_source_filter_line;
        sdp += "\n";
    }

    sdp +=
        "m=video 50000 RTP/AVP 112\n"
        "a=ts-refclk:ptp=IEEE1588-2008:39-A7-94-FF-FE-07-CB-D0:37\n"
        "a=mediaclk:direct=0\n";

    if (!media_source_filter_line.empty()) {
        sdp += "a=source-filter:";
        sdp += media_source_filter_line;
        sdp += "\n";
    }

    sdp += "a=rtpmap:112 raw/90000\n";
    sdp += "a=fmtp:112 ";
    sdp += kBaseFmtp;
    sdp += "; TP=2110TPN\n";

    return sdp;
}

void test_valid_session_level_source_filter_is_preserved() {
    const auto parsed = select_raw_video_sdp_media_section(
        make_video_sdp(" incl IN IP4 239.1.1.1 198.51.100.10", ""), 112);

    assert(parsed.has_value());
    assert(parsed->source_filters.size() == 1);

    const auto &filter = parsed->source_filters[0];
    assert(filter.scope == RawSdpSourceFilter::Scope::Session);
    assert(filter.raw_value == "incl IN IP4 239.1.1.1 198.51.100.10");
    assert(filter.filter_mode == "incl");
    assert(filter.network_type == "IN");
    assert(filter.address_type == "IP4");
    assert(filter.destination_address == "239.1.1.1");
    assert(filter.source_addresses.size() == 1);
    assert(filter.source_addresses[0] == "198.51.100.10");
}

void test_valid_media_level_source_filter_is_preserved() {
    const auto parsed = select_raw_video_sdp_media_section(
        make_video_sdp("", " excl IN IP6 ff15::abcd 2001:db8::10"), 112);

    assert(parsed.has_value());
    assert(parsed->source_filters.size() == 1);

    const auto &filter = parsed->source_filters[0];
    assert(filter.scope == RawSdpSourceFilter::Scope::Media);
    assert(filter.raw_value == "excl IN IP6 ff15::abcd 2001:db8::10");
    assert(filter.filter_mode == "excl");
    assert(filter.network_type == "IN");
    assert(filter.address_type == "IP6");
    assert(filter.destination_address == "ff15::abcd");
    assert(filter.source_addresses.size() == 1);
    assert(filter.source_addresses[0] == "2001:db8::10");
}

void test_parsed_source_filter_remains_scope_aware() {
    const auto parsed = select_raw_video_sdp_media_section(
        make_video_sdp(" incl IN IP4 239.1.1.1 198.51.100.10 198.51.100.11",
                       " excl IN IP6 ff15::abcd 2001:db8::10"),
        112);

    assert(parsed.has_value());
    assert(parsed->source_filters.size() == 2);

    const auto &session_filter = parsed->source_filters[0];
    assert(session_filter.scope == RawSdpSourceFilter::Scope::Session);
    assert(session_filter.filter_mode == "incl");
    assert(session_filter.source_addresses.size() == 2);
    assert(session_filter.source_addresses[0] == "198.51.100.10");
    assert(session_filter.source_addresses[1] == "198.51.100.11");

    const auto &media_filter = parsed->source_filters[1];
    assert(media_filter.scope == RawSdpSourceFilter::Scope::Media);
    assert(media_filter.filter_mode == "excl");
    assert(media_filter.source_addresses.size() == 1);
    assert(media_filter.source_addresses[0] == "2001:db8::10");
}

void test_invalid_filter_mode_is_rejected() {
    const auto parsed =
        parse_source_filter_attribute_value("allow IN IP4 239.1.1.1 198.51.100.10",
                                            RawSdpSourceFilter::Scope::Session);

    assert(!parsed.has_value());
    assert(parsed.error() == Error::InvalidValue);
}

void test_missing_destination_or_source_fields_are_rejected() {
    {
        const auto parsed =
            parse_source_filter_attribute_value("incl IN IP4", RawSdpSourceFilter::Scope::Session);

        assert(!parsed.has_value());
        assert(parsed.error() == Error::InvalidValue);
    }

    {
        const auto parsed =
            parse_source_filter_attribute_value("incl IN IP4 239.1.1.1",
                                               RawSdpSourceFilter::Scope::Session);

        assert(!parsed.has_value());
        assert(parsed.error() == Error::InvalidValue);
    }
}

void test_malformed_source_list_forms_are_rejected() {
    {
        const auto parsed =
            parse_source_filter_attribute_value("incl IN IP4 239.1.1.1 198.51.100.10,198.51.100.11",
                                               RawSdpSourceFilter::Scope::Session);

        assert(!parsed.has_value());
        assert(parsed.error() == Error::InvalidValue);
    }

    {
        const auto parsed =
            parse_source_filter_attribute_value("incl IN IP4 239.1.1.1 198.51.100.10,",
                                               RawSdpSourceFilter::Scope::Session);

        assert(!parsed.has_value());
        assert(parsed.error() == Error::InvalidValue);
    }

    {
        const auto parsed =
            parse_source_filter_attribute_value("incl IN IP4 239.1.1.1 198.51.100.10;198.51.100.11",
                                               RawSdpSourceFilter::Scope::Session);

        assert(!parsed.has_value());
        assert(parsed.error() == Error::InvalidValue);
    }
}

void test_backend_runtime_behavior_remains_untouched() {
    const auto signaling =
        parse_video_stream_signaling_from_sdp(
            make_video_sdp("", " incl IN IP4 239.1.1.1 198.51.100.10"), 112);

    assert(signaling.has_value());
    assert(signaling->media.width == 1920);
    assert(signaling->media.height == 1080);
    assert(signaling->packing_mode == VideoPackingMode::Gpm);
    assert(signaling->scan_mode == VideoScanMode::Progressive);
    assert(signaling->sender_type == VideoSenderType::Narrow);
}

} // namespace

int main() {
    test_valid_session_level_source_filter_is_preserved();
    test_valid_media_level_source_filter_is_preserved();
    test_parsed_source_filter_remains_scope_aware();
    test_invalid_filter_mode_is_rejected();
    test_missing_destination_or_source_fields_are_rejected();
    test_malformed_source_list_forms_are_rejected();
    test_backend_runtime_behavior_remains_untouched();
    return 0;
}
#include <cassert>
#include <optional>
#include <string_view>

#include <st2110/video_sdp_ingestion.hpp>
#include <st2110/video_sdp_media_section.hpp>

static constexpr std::string_view kBaseFmtp = "sampling=YCbCr-4:2:2; width=1920; height=1080; exactframerate=25; "
                                              "depth=8; colorimetry=BT709; PM=2110GPM; SSN=ST2110-20:2017";

static std::string make_valid_sdp_with_transport() {
    return std::string{"v=0\n"
                       "o=- 1 1 IN IP4 192.0.2.1\n"
                       "s=ST2110 test\n"
                       "c=IN IP4 239.10.10.10/32\n"
                       "a=group:DUP primary secondary\n"
                       "a=session-future:preserved\n"
                       "m=video 50000 RTP/AVP 112\n"
                       "c=IN IP4 239.20.20.20/32\n"
                       "a=ts-refclk:ptp=IEEE1588-2008:traceable\n"
                       "a=mid:primary\n"
                       "a=source-filter: incl IN IP4 239.20.20.20 192.0.2.10\n"
                       "a=rtpmap:112 raw/90000\n"
                       "a=fmtp:112 "} +
           std::string{kBaseFmtp} +
           "\n"
           "a=media-future:kept\n";
}

static void test_selected_section_preserves_session_and_media_connection_data() {
    auto raw = st2110::select_raw_video_sdp_media_section(make_valid_sdp_with_transport(), 112);

    assert(raw.has_value());

    assert(raw->session_connection.has_value());
    assert(raw->session_connection->network_type == "IN");
    assert(raw->session_connection->address_type == "IP4");
    assert(raw->session_connection->connection_address == "239.10.10.10/32");

    assert(raw->media_connection.has_value());
    assert(raw->media_connection->network_type == "IN");
    assert(raw->media_connection->address_type == "IP4");
    assert(raw->media_connection->connection_address == "239.20.20.20/32");
}

static void test_selected_section_preserves_mid_source_filter_and_dup_group() {
    auto raw = st2110::select_raw_video_sdp_media_section(make_valid_sdp_with_transport(), 112);

    assert(raw.has_value());

    assert(raw->mid.has_value());
    assert(*raw->mid == "primary");

    assert(raw->source_filters.size() == 1);
    assert(raw->source_filters[0].raw_value == "incl IN IP4 239.20.20.20 192.0.2.10");

    assert(raw->session_groups.size() == 1);
    assert(raw->session_groups[0].semantics == "DUP");
    assert(raw->session_groups[0].mids.size() == 2);
    assert(raw->session_groups[0].mids[0] == "primary");
    assert(raw->session_groups[0].mids[1] == "secondary");
}

static void test_session_and_media_unknown_attributes_are_preserved_separately() {
    auto raw = st2110::select_raw_video_sdp_media_section(make_valid_sdp_with_transport(), 112);

    assert(raw.has_value());

    assert(raw->unknown_session_attributes.size() == 1);
    assert(raw->unknown_session_attributes[0].name == "session-future");
    assert(raw->unknown_session_attributes[0].value == "preserved");

    assert(raw->unknown_attributes.size() == 1);
    assert(raw->unknown_attributes[0].name == "media-future");
    assert(raw->unknown_attributes[0].value == "kept");
}

static void test_transport_metadata_does_not_break_video_signaling_ingestion() {
    auto signaling = st2110::parse_video_stream_signaling_from_sdp(make_valid_sdp_with_transport(), 112);

    assert(signaling.has_value());
    assert(signaling->media.width == 1920);
    assert(signaling->media.height == 1080);
    assert(signaling->packing_mode == st2110::VideoPackingMode::Gpm);
}

static void test_media_connection_duplicate_is_rejected() {
    const std::string sdp = std::string{"v=0\n"
                                        "m=video 50000 RTP/AVP 112\n"
                                        "c=IN IP4 239.20.20.20/32\n"
                                        "c=IN IP4 239.20.20.21/32\n"
                                        "a=rtpmap:112 raw/90000\n"
                                        "a=fmtp:112 "} +
                            std::string{kBaseFmtp} + "\n";

    auto raw = st2110::select_raw_video_sdp_media_section(sdp, 112);
    assert(!raw.has_value());
    assert(raw.error() == st2110::Error::InvalidValue);
}

static void test_session_connection_duplicate_is_rejected() {
    const std::string sdp = std::string{"v=0\n"
                                        "c=IN IP4 239.10.10.10/32\n"
                                        "c=IN IP4 239.10.10.11/32\n"
                                        "m=video 50000 RTP/AVP 112\n"
                                        "a=rtpmap:112 raw/90000\n"
                                        "a=fmtp:112 "} +
                            std::string{kBaseFmtp} + "\n";

    auto raw = st2110::select_raw_video_sdp_media_section(sdp, 112);
    assert(!raw.has_value());
    assert(raw.error() == st2110::Error::InvalidValue);
}

static void test_mid_duplicate_is_rejected() {
    const std::string sdp = std::string{"v=0\n"
                                        "m=video 50000 RTP/AVP 112\n"
                                        "a=mid:primary\n"
                                        "a=mid:secondary\n"
                                        "a=rtpmap:112 raw/90000\n"
                                        "a=fmtp:112 "} +
                            std::string{kBaseFmtp} + "\n";

    auto raw = st2110::select_raw_video_sdp_media_section(sdp, 112);
    assert(!raw.has_value());
    assert(raw.error() == st2110::Error::InvalidValue);
}

static void test_other_media_section_transport_is_not_leaked_into_selected_section() {
    const std::string sdp = std::string{"v=0\n"
                                        "m=audio 50002 RTP/AVP 96\n"
                                        "c=IN IP4 239.99.99.99/32\n"
                                        "a=mid:audio_mid\n"
                                        "m=video 50000 RTP/AVP 112\n"
                                        "a=mid:video_mid\n"
                                        "a=source-filter: incl IN IP4 239.20.20.20 192.0.2.10\n"
                                        "a=rtpmap:112 raw/90000\n"
                                        "a=fmtp:112 "} +
                            std::string{kBaseFmtp} + "\n";

    auto raw = st2110::select_raw_video_sdp_media_section(sdp, 112);

    assert(raw.has_value());
    assert(!raw->media_connection.has_value());
    assert(raw->mid.has_value());
    assert(*raw->mid == "video_mid");
    assert(raw->source_filters.size() == 1);
}

int main() {
    test_selected_section_preserves_session_and_media_connection_data();
    test_selected_section_preserves_mid_source_filter_and_dup_group();
    test_session_and_media_unknown_attributes_are_preserved_separately();
    test_transport_metadata_does_not_break_video_signaling_ingestion();
    test_media_connection_duplicate_is_rejected();
    test_session_connection_duplicate_is_rejected();
    test_mid_duplicate_is_rejected();
    test_other_media_section_transport_is_not_leaked_into_selected_section();

    return 0;
}
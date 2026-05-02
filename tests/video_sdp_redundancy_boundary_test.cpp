#include "st2110/video_sdp_ingestion.hpp"
#include "st2110/video_sdp_media_section.hpp"

#include <cassert>
#include <string>

using namespace st2110;

namespace {
constexpr uint8_t kPayloadType = 112;

[[nodiscard]] std::string valid_fmtp_payload() {
    return "sampling=YCbCr-4:2:2; width=1920; height=1080; exactframerate=25; "
           "depth=8; colorimetry=BT709; PM=2110GPM; SSN=ST2110-20:2017; "
           "TCS=SDR; TP=2110TPN";
}

[[nodiscard]] std::string primary_media_section(std::string mid) {
    return "m=video 50000 RTP/AVP 112\n"
           "c=IN IP4 239.1.1.1\n"
           "a=mid:" +
           mid +
           "\n"
           "a=source-filter: incl IN IP4 239.1.1.1 192.0.2.10\n"
           "a=rtpmap:112 raw/90000\n"
           "a=fmtp:112 " +
           valid_fmtp_payload() +
           "\n"
           "a=ts-refclk:ptp=IEEE1588-2008:traceable\n"
           "a=mediaclk:direct=0\n"
           "a=tsmode:SAMP\n";
}

[[nodiscard]] std::string duplicate_media_section(std::string mid) {
    return "m=video 50002 RTP/AVP 112\n"
           "c=IN IP4 239.1.1.2\n"
           "a=mid:" +
           mid +
           "\n"
           "a=source-filter: incl IN IP4 239.1.1.2 192.0.2.11 192.0.2.12\n"
           "a=rtpmap:112 raw/90000\n"
           "a=fmtp:112 " +
           valid_fmtp_payload() + "\n";
}

void rejects_two_matching_video_sections_without_dup_group() {
    const std::string sdp = "v=0\n"
                            "o=- 0 0 IN IP4 127.0.0.1\n"
                            "s=ST2110 test\n"
                            "t=0 0\n" +
                            primary_media_section("primary") + duplicate_media_section("backup");

    const auto raw = select_raw_video_sdp_media_section(sdp, kPayloadType);
    assert(!raw.has_value());
    assert(raw.error() == Error::InvalidValue);
}

void rejects_two_matching_video_sections_with_unrelated_dup_group() {
    const std::string sdp = "v=0\n"
                            "o=- 0 0 IN IP4 127.0.0.1\n"
                            "s=ST2110 test\n"
                            "t=0 0\n"
                            "a=group:DUP primary other\n" +
                            primary_media_section("primary") + duplicate_media_section("backup");

    const auto raw = select_raw_video_sdp_media_section(sdp, kPayloadType);
    assert(!raw.has_value());
    assert(raw.error() == Error::InvalidValue);
}

void rejects_duplicate_candidate_without_mid_even_with_dup_group() {
    const std::string sdp = "v=0\n"
                            "o=- 0 0 IN IP4 127.0.0.1\n"
                            "s=ST2110 test\n"
                            "t=0 0\n"
                            "a=group:DUP primary backup\n" +
                            primary_media_section("primary") +
                            "m=video 50002 RTP/AVP 112\n"
                            "c=IN IP4 239.1.1.2\n"
                            "a=rtpmap:112 raw/90000\n"
                            "a=fmtp:112 " +
                            valid_fmtp_payload() + "\n";

    const auto raw = select_raw_video_sdp_media_section(sdp, kPayloadType);
    assert(!raw.has_value());
    assert(raw.error() == Error::InvalidValue);
}

void preserves_duplicate_candidate_when_mids_are_in_same_dup_group() {
    const std::string sdp = "v=0\n"
                            "o=- 0 0 IN IP4 127.0.0.1\n"
                            "s=ST2110 test\n"
                            "t=0 0\n"
                            "a=group:DUP primary backup\n" +
                            primary_media_section("primary") + duplicate_media_section("backup");

    const auto raw = select_raw_video_sdp_media_section(sdp, kPayloadType);
    assert(raw.has_value());

    assert(raw->payload_type == kPayloadType);
    assert(raw->mid.has_value());
    assert(*raw->mid == "primary");
    assert(raw->media_connection.has_value());
    assert(raw->media_connection->connection_address == "239.1.1.1");

    assert(raw->source_filters.size() == 1);
    assert(raw->source_filters[0].scope == RawSdpSourceFilter::Scope::Media);
    assert(raw->source_filters[0].destination_address == "239.1.1.1");
    assert(raw->source_filters[0].source_addresses.size() == 1);
    assert(raw->source_filters[0].source_addresses[0] == "192.0.2.10");

    assert(raw->duplicate_candidates.size() == 1);

    const auto &duplicate = raw->duplicate_candidates[0];
    assert(duplicate.payload_type == kPayloadType);
    assert(duplicate.mid.has_value());
    assert(*duplicate.mid == "backup");
    assert(duplicate.media_connection.has_value());
    assert(duplicate.media_connection->connection_address == "239.1.1.2");
    assert(duplicate.rtpmap == "raw/90000");
    assert(!duplicate.fmtp.empty());

    assert(duplicate.source_filters.size() == 1);
    assert(duplicate.source_filters[0].scope == RawSdpSourceFilter::Scope::Media);
    assert(duplicate.source_filters[0].filter_mode == "incl");
    assert(duplicate.source_filters[0].network_type == "IN");
    assert(duplicate.source_filters[0].address_type == "IP4");
    assert(duplicate.source_filters[0].destination_address == "239.1.1.2");
    assert(duplicate.source_filters[0].source_addresses.size() == 2);
    assert(duplicate.source_filters[0].source_addresses[0] == "192.0.2.11");
    assert(duplicate.source_filters[0].source_addresses[1] == "192.0.2.12");
}

void final_ingestion_still_uses_primary_media_section() {
    const std::string sdp = "v=0\n"
                            "o=- 0 0 IN IP4 127.0.0.1\n"
                            "s=ST2110 test\n"
                            "t=0 0\n"
                            "a=group:DUP primary backup\n" +
                            primary_media_section("primary") + duplicate_media_section("backup");

    const auto signaling = parse_video_stream_signaling_from_sdp(sdp, kPayloadType);
    assert(signaling.has_value());

    assert(signaling->media.width == 1920);
    assert(signaling->media.height == 1080);
    assert(signaling->scan_mode == VideoScanMode::Progressive);
    assert(signaling->packing_mode == VideoPackingMode::Gpm);
}

void single_media_section_ingestion_remains_unchanged() {
    const std::string sdp = "v=0\n"
                            "o=- 0 0 IN IP4 127.0.0.1\n"
                            "s=ST2110 test\n"
                            "t=0 0\n" +
                            primary_media_section("primary");

    const auto raw = select_raw_video_sdp_media_section(sdp, kPayloadType);
    assert(raw.has_value());
    assert(raw->duplicate_candidates.empty());

    const auto signaling = parse_video_stream_signaling_from_sdp(sdp, kPayloadType);
    assert(signaling.has_value());

    assert(signaling->media.width == 1920);
    assert(signaling->media.height == 1080);
    assert(signaling->scan_mode == VideoScanMode::Progressive);
    assert(signaling->packing_mode == VideoPackingMode::Gpm);
}
} // namespace

int main() {
    rejects_two_matching_video_sections_without_dup_group();
    rejects_two_matching_video_sections_with_unrelated_dup_group();
    rejects_duplicate_candidate_without_mid_even_with_dup_group();
    preserves_duplicate_candidate_when_mids_are_in_same_dup_group();
    final_ingestion_still_uses_primary_media_section();
    single_media_section_ingestion_remains_unchanged();

    return 0;
}
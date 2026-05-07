#include "st2110/ingress/video/video_sdp_ingestion.hpp"
#include "st2110/ingress/video/video_sdp_media_section.hpp"
#include "st2110/ingress/video/video_sdp_timing_attributes.hpp"
#include "st2110/ingress/video/video_sdp_fmtp.hpp"

#include <cassert>
#include <cstdint>
#include <string>

using namespace st2110;

namespace {
constexpr uint8_t kPayloadType = 112;

std::string valid_session_refclk(unsigned domain = 1) {
    return "a=ts-refclk:ptp=IEEE1588-2008:00-11-22-33-44-55-66-77:" + std::to_string(domain) + "\n";
}

std::string valid_fmtp_line(const std::string &extra = {}, std::string_view sender_type = "2110TPN") {
    std::string line = "a=fmtp:112 "
                       "sampling=YCbCr-4:2:2; "
                       "width=1920; "
                       "height=1080; "
                       "exactframerate=25; "
                       "depth=8; "
                       "colorimetry=BT709; "
                       "PM=2110GPM; "
                       "SSN=ST2110-20:2017; "
                       "TCS=SDR; "
                       "TP=";
    line += sender_type;

    if (!extra.empty()) {
        line += "; ";
        line += extra;
    }

    line += "\n";
    return line;
}

std::string make_video_sdp(const std::string &session_attributes = {}, const std::string &media_attributes = {},
                           const std::string &fmtp_extra = {}, std::string_view sender_type = "2110TPN") {
    std::string sdp;
    sdp += "v=0\n";
    sdp += "o=- 0 0 IN IP4 127.0.0.1\n";
    sdp += "s=ST2110 test\n";
    sdp += "t=0 0\n";
    sdp += session_attributes;
    sdp += "m=video 50000 RTP/AVP 112\n";
    sdp += "c=IN IP4 239.1.1.1\n";
    sdp += "a=mid:primary\n";
    sdp += "a=rtpmap:112 raw/90000\n";
    sdp += valid_fmtp_line(fmtp_extra, sender_type);
    sdp += media_attributes;
    return sdp;
}

void assert_fmtp_sender_type(const RawVideoSdpMediaSection &raw, std::string_view expected_sender_type) {
    auto fmtp = parse_video_sdp_fmtp_payload(raw.fmtp);
    assert(fmtp.has_value());
    assert(fmtp->sender_type.has_value());
    assert(*fmtp->sender_type == expected_sender_type);
}

void session_level_reference_clock_and_media_clock_are_preserved_but_session_only_mediaclk_is_rejected_by_final_ingestion() {
    const std::string sdp = make_video_sdp(valid_session_refclk(1) + "a=mediaclk:direct=0\n");

    auto raw = select_raw_video_sdp_media_section(sdp, kPayloadType);
    assert(raw.has_value());

    assert(raw->session_ts_refclk.has_value());
    assert(raw->session_ts_refclk->scope == RawSdpAttributeScope::Session);
    assert(raw->session_ts_refclk->value == "ptp=IEEE1588-2008:00-11-22-33-44-55-66-77:1");

    assert(!raw->media_ts_refclk.has_value());
    assert(raw->ts_refclk.has_value());
    assert(*raw->ts_refclk == "ptp=IEEE1588-2008:00-11-22-33-44-55-66-77:1");

    assert(raw->session_mediaclk.has_value());
    assert(raw->session_mediaclk->scope == RawSdpAttributeScope::Session);
    assert(raw->session_mediaclk->value == "direct=0");

    assert(!raw->media_mediaclk.has_value());
    assert(raw->mediaclk.has_value());
    assert(*raw->mediaclk == "direct=0");

    auto timing = parse_video_sdp_timing_attributes(*raw);
    assert(timing.has_value());

    assert(timing->reference_clock.has_value());
    assert(timing->reference_clock->scope == RawSdpAttributeScope::Session);
    assert(timing->reference_clock->value.kind == RawVideoSdpReferenceClock::Kind::Ptp);

    assert(timing->media_clock.has_value());
    assert(timing->media_clock->scope == RawSdpAttributeScope::Session);
    assert(timing->media_clock->value.kind == RawVideoSdpMediaClock::Kind::Direct);

    assert(!timing->sender_type.has_value());
    assert_fmtp_sender_type(*raw, "2110TPN");

    assert(!raw_video_sdp_has_media_level_mediaclk(*timing));

    auto signaling = parse_video_stream_signaling_from_sdp(sdp, kPayloadType);
    assert(!signaling.has_value());
    assert(signaling.error() == Error::InvalidValue);
}

void media_level_reference_clock_and_media_clock_override_session_level_values() {
    const std::string sdp = make_video_sdp(valid_session_refclk(1) + "a=mediaclk:sender\n",
                                           "a=ts-refclk:ptp=IEEE1588-2008:00-11-22-33-44-55-66-77:2\n"
                                           "a=mediaclk:direct=0\n");

    auto raw = select_raw_video_sdp_media_section(sdp, kPayloadType);
    assert(raw.has_value());

    assert(raw->session_ts_refclk.has_value());
    assert(raw->session_ts_refclk->scope == RawSdpAttributeScope::Session);
    assert(raw->session_ts_refclk->value == "ptp=IEEE1588-2008:00-11-22-33-44-55-66-77:1");

    assert(raw->media_ts_refclk.has_value());
    assert(raw->media_ts_refclk->scope == RawSdpAttributeScope::Media);
    assert(raw->media_ts_refclk->value == "ptp=IEEE1588-2008:00-11-22-33-44-55-66-77:2");

    assert(raw->ts_refclk.has_value());
    assert(*raw->ts_refclk == "ptp=IEEE1588-2008:00-11-22-33-44-55-66-77:2");

    assert(raw->session_mediaclk.has_value());
    assert(raw->session_mediaclk->value == "sender");

    assert(raw->media_mediaclk.has_value());
    assert(raw->media_mediaclk->value == "direct=0");

    assert(raw->mediaclk.has_value());
    assert(*raw->mediaclk == "direct=0");

    auto timing = parse_video_sdp_timing_attributes(*raw);
    assert(timing.has_value());

    assert(timing->reference_clock.has_value());
    assert(timing->reference_clock->scope == RawSdpAttributeScope::Media);
    assert(timing->reference_clock->value.kind == RawVideoSdpReferenceClock::Kind::Ptp);
    assert(timing->reference_clock->value.ptp.has_value());
    assert(timing->reference_clock->value.ptp->domain.has_value());
    assert(*timing->reference_clock->value.ptp->domain == 2);

    assert(timing->media_clock.has_value());
    assert(timing->media_clock->scope == RawSdpAttributeScope::Media);
    assert(timing->media_clock->value.kind == RawVideoSdpMediaClock::Kind::Direct);

    assert(!timing->sender_type.has_value());
    assert_fmtp_sender_type(*raw, "2110TPN");

    assert(raw_video_sdp_has_media_level_mediaclk(*timing));

    auto signaling = parse_video_stream_signaling_from_sdp(sdp, kPayloadType);
    assert(signaling.has_value());

    assert(signaling->reference_clock.kind == ReferenceClockKind::Ptp);
    assert(signaling->reference_clock.ptp.has_value());
    assert(signaling->reference_clock.ptp->domain_number == 2);
    assert(signaling->media_clock_mode == MediaClockMode::Direct);
    assert(signaling->sender_type == VideoSenderType::Narrow);
}

void duplicate_session_level_timing_attribute_is_rejected() {
    const std::string sdp = make_video_sdp(valid_session_refclk(1) + valid_session_refclk(2));

    auto raw = select_raw_video_sdp_media_section(sdp, kPayloadType);
    assert(!raw.has_value());
    assert(raw.error() == Error::InvalidValue);
}

void duplicate_media_level_timing_attribute_is_rejected() {
    const std::string sdp = make_video_sdp({}, "a=mediaclk:direct=0\n"
                                               "a=mediaclk:sender\n");

    auto raw = select_raw_video_sdp_media_section(sdp, kPayloadType);
    assert(!raw.has_value());
    assert(raw.error() == Error::InvalidValue);
}

void fmtp_media_level_timing_field_overrides_session_level_standalone_attribute() {
    const std::string sdp =
        make_video_sdp(valid_session_refclk(1) + "a=mediaclk:direct=0\n" + "a=tsmode:NEW\n", "a=mediaclk:direct=0\n",
                       "TSMODE=PRES");

    auto raw = select_raw_video_sdp_media_section(sdp, kPayloadType);
    assert(raw.has_value());

    assert(raw->session_tsmode.has_value());
    assert(raw->session_tsmode->scope == RawSdpAttributeScope::Session);
    assert(raw->session_tsmode->value == "NEW");

    assert(!raw->media_tsmode.has_value());

    auto timing = parse_video_sdp_timing_attributes(*raw);
    assert(timing.has_value());
    assert(timing->timestamp_mode.has_value());
    assert(timing->timestamp_mode->scope == RawSdpAttributeScope::Session);
    assert(timing->timestamp_mode->value.raw_token == "NEW");
    assert(!timing->sender_type.has_value());
    assert_fmtp_sender_type(*raw, "2110TPN");
    assert(raw_video_sdp_has_media_level_mediaclk(*timing));

    auto signaling = parse_video_stream_signaling_from_sdp(sdp, kPayloadType);
    assert(signaling.has_value());

    assert(signaling->timestamp_mode == TimestampMode::Pres);
    assert(signaling->sender_type == VideoSenderType::Narrow);
}

void fmtp_timing_field_conflicts_with_media_level_standalone_attribute() {
    const std::string sdp =
        make_video_sdp(valid_session_refclk(1) + "a=mediaclk:direct=0\n", "a=mediaclk:direct=0\n"
                                                                           "a=tsmode:NEW\n",
                       "TSMODE=PRES");

    auto raw = select_raw_video_sdp_media_section(sdp, kPayloadType);
    assert(raw.has_value());

    auto timing = parse_video_sdp_timing_attributes(*raw);
    assert(timing.has_value());
    assert(timing->timestamp_mode.has_value());
    assert(timing->timestamp_mode->scope == RawSdpAttributeScope::Media);
    assert(!timing->sender_type.has_value());
    assert_fmtp_sender_type(*raw, "2110TPN");
    assert(raw_video_sdp_has_media_level_mediaclk(*timing));

    auto signaling = parse_video_stream_signaling_from_sdp(sdp, kPayloadType);
    assert(!signaling.has_value());
    assert(signaling.error() == Error::InvalidValue);
}

void existing_media_level_only_sdp_behavior_remains_unchanged() {
    const std::string sdp = make_video_sdp({}, "a=ts-refclk:ptp=IEEE1588-2008:00-11-22-33-44-55-66-77:3\n"
                                           "a=mediaclk:direct=0\n"
                                           "a=tsmode:NEW\n"
                                           "a=tsdelay:1800\n"
                                           "a=TROFF:42\n"
                                           "a=CMAX:7\n",
                                           {}, "2110TPW");

    auto raw = select_raw_video_sdp_media_section(sdp, kPayloadType);
    assert(raw.has_value());

    assert(!raw->session_ts_refclk.has_value());
    assert(raw->media_ts_refclk.has_value());
    assert(raw->media_ts_refclk->scope == RawSdpAttributeScope::Media);

    assert(!raw->session_mediaclk.has_value());
    assert(raw->media_mediaclk.has_value());
    assert(raw->media_mediaclk->scope == RawSdpAttributeScope::Media);

    auto timing = parse_video_sdp_timing_attributes(*raw);
    assert(timing.has_value());

    assert(timing->reference_clock.has_value());
    assert(timing->reference_clock->scope == RawSdpAttributeScope::Media);

    assert(timing->media_clock.has_value());
    assert(timing->media_clock->scope == RawSdpAttributeScope::Media);
    assert(raw_video_sdp_has_media_level_mediaclk(*timing));

    assert(timing->timestamp_mode.has_value());
    assert(timing->timestamp_mode->scope == RawSdpAttributeScope::Media);
    assert(timing->timestamp_mode->value.raw_token == "NEW");

    assert(timing->ts_delay_sender_ticks.has_value());
    assert(timing->ts_delay_sender_ticks->scope == RawSdpAttributeScope::Media);
    assert(timing->ts_delay_sender_ticks->value == 1800);

    assert(!timing->sender_type.has_value());
    assert_fmtp_sender_type(*raw, "2110TPW");

    assert(timing->troff_us.has_value());
    assert(timing->troff_us->scope == RawSdpAttributeScope::Media);
    assert(timing->troff_us->value == 42);

    assert(timing->cmax.has_value());
    assert(timing->cmax->scope == RawSdpAttributeScope::Media);
    assert(timing->cmax->value == 7);

    auto signaling = parse_video_stream_signaling_from_sdp(sdp, kPayloadType);
    assert(signaling.has_value());

    assert(signaling->reference_clock.kind == ReferenceClockKind::Ptp);
    assert(signaling->reference_clock.ptp.has_value());
    assert(signaling->reference_clock.ptp->domain_number == 3);

    assert(signaling->media_clock_mode == MediaClockMode::Direct);
    assert(signaling->timestamp_mode == TimestampMode::New);
    assert(signaling->ts_delay_sender_ticks == 1800);
    assert(signaling->sender_type == VideoSenderType::Wide);
    assert(signaling->troff_us == 42);
    assert(signaling->cmax == 7);
}
} // namespace

int main() {
    session_level_reference_clock_and_media_clock_are_preserved_but_session_only_mediaclk_is_rejected_by_final_ingestion();
    media_level_reference_clock_and_media_clock_override_session_level_values();
    duplicate_session_level_timing_attribute_is_rejected();
    duplicate_media_level_timing_attribute_is_rejected();
    fmtp_media_level_timing_field_overrides_session_level_standalone_attribute();
    fmtp_timing_field_conflicts_with_media_level_standalone_attribute();
    existing_media_level_only_sdp_behavior_remains_unchanged();

    return 0;
}
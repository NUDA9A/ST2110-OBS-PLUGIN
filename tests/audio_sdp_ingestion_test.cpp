#include "st2110/foundation/error.hpp"
#include "st2110/ingress/audio/audio_sdp_ingestion.hpp"
#include "st2110/ingress/audio/audio_sdp_media_section.hpp"
#include "st2110/model/audio/audio_signaling.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace {
constexpr uint8_t kPayloadType = 101;

bool has_attribute(const std::vector<st2110::RawAudioSdpAttribute> &attributes, std::string_view name,
                   std::string_view value) {
    for (const auto &attribute : attributes) {
        if (attribute.name == name && attribute.value == value) {
            return true;
        }
    }

    return false;
}

std::string valid_level_a_sdp_with_channel_order() {
    return "v=0\n"
           "o=- 1 1 IN IP4 192.0.2.10\n"
           "s=ST2110-30 Level A stereo\n"
           "t=0 0\n"
           "a=ts-refclk:ptp=IEEE1588-2008:39-A7-94-FF-FE-07-CB-D0:37\n"
           "m=audio 5004 RTP/AVP 101\n"
           "c=IN IP4 239.10.10.10/32\n"
           "a=rtpmap:101 L24/48000/2\n"
           "a=ptime:1\n"
           "a=mediaclk:direct=0\n"
           "a=fmtp:101 channel-order=SMPTE2110.(ST)\n";
}

std::string valid_level_a_sdp_without_channel_order_but_with_unknowns() {
    return "v=0\n"
           "o=- 1 1 IN IP4 192.0.2.10\n"
           "s=ST2110-30 Level A mono\n"
           "t=0 0\n"
           "a=x-session-unknown:preserve-session\n"
           "a=ts-refclk:ptp=IEEE1588-2008:39-A7-94-FF-FE-07-CB-D0:37\n"
           "m=audio 5004 RTP/AVP 101\n"
           "c=IN IP4 239.10.10.10/32\n"
           "a=rtpmap:101 L16/48000/1\n"
           "a=ptime:1\n"
           "a=mediaclk:direct=0\n"
           "a=x-media-unknown:preserve-media\n";
}

std::string sdp_missing_rtpmap() {
    return "v=0\n"
           "o=- 1 1 IN IP4 192.0.2.10\n"
           "s=Missing rtpmap\n"
           "t=0 0\n"
           "a=ts-refclk:ptp=IEEE1588-2008:39-A7-94-FF-FE-07-CB-D0:37\n"
           "m=audio 5004 RTP/AVP 101\n"
           "c=IN IP4 239.10.10.10/32\n"
           "a=ptime:1\n"
           "a=mediaclk:direct=0\n";
}

std::string sdp_invalid_ptime() {
    return "v=0\n"
           "o=- 1 1 IN IP4 192.0.2.10\n"
           "s=Invalid ptime\n"
           "t=0 0\n"
           "a=ts-refclk:ptp=IEEE1588-2008:39-A7-94-FF-FE-07-CB-D0:37\n"
           "m=audio 5004 RTP/AVP 101\n"
           "c=IN IP4 239.10.10.10/32\n"
           "a=rtpmap:101 L24/48000/2\n"
           "a=ptime:0\n"
           "a=mediaclk:direct=0\n";
}

std::string sdp_with_unsupported_encoding() {
    return "v=0\n"
           "o=- 1 1 IN IP4 192.0.2.10\n"
           "s=Unsupported encoding\n"
           "t=0 0\n"
           "a=ts-refclk:ptp=IEEE1588-2008:39-A7-94-FF-FE-07-CB-D0:37\n"
           "m=audio 5004 RTP/AVP 101\n"
           "c=IN IP4 239.10.10.10/32\n"
           "a=rtpmap:101 AM824/48000/2\n"
           "a=ptime:1\n"
           "a=mediaclk:direct=0\n";
}

std::string sdp_missing_ts_refclk() {
    return "v=0\n"
           "o=- 1 1 IN IP4 192.0.2.10\n"
           "s=Missing ts-refclk\n"
           "t=0 0\n"
           "m=audio 5004 RTP/AVP 101\n"
           "c=IN IP4 239.10.10.10/32\n"
           "a=rtpmap:101 L24/48000/2\n"
           "a=ptime:1\n"
           "a=mediaclk:direct=0\n";
}

std::string sdp_missing_media_level_mediaclk() {
    return "v=0\n"
           "o=- 1 1 IN IP4 192.0.2.10\n"
           "s=Missing media-level mediaclk\n"
           "t=0 0\n"
           "a=ts-refclk:ptp=IEEE1588-2008:39-A7-94-FF-FE-07-CB-D0:37\n"
           "m=audio 5004 RTP/AVP 101\n"
           "c=IN IP4 239.10.10.10/32\n"
           "a=rtpmap:101 L24/48000/2\n"
           "a=ptime:1\n";
}

std::string sdp_with_session_level_only_mediaclk() {
    return "v=0\n"
           "o=- 1 1 IN IP4 192.0.2.10\n"
           "s=Session-level only mediaclk\n"
           "t=0 0\n"
           "a=ts-refclk:ptp=IEEE1588-2008:39-A7-94-FF-FE-07-CB-D0:37\n"
           "a=mediaclk:direct=0\n"
           "m=audio 5004 RTP/AVP 101\n"
           "c=IN IP4 239.10.10.10/32\n"
           "a=rtpmap:101 L24/48000/2\n"
           "a=ptime:1\n";
}

std::string sdp_with_malformed_known_ts_refclk() {
    return "v=0\n"
           "o=- 1 1 IN IP4 192.0.2.10\n"
           "s=Malformed known ts-refclk\n"
           "t=0 0\n"
           "a=ts-refclk:ptp=IEEE1588-2008\n"
           "m=audio 5004 RTP/AVP 101\n"
           "c=IN IP4 239.10.10.10/32\n"
           "a=rtpmap:101 L24/48000/2\n"
           "a=ptime:1\n"
           "a=mediaclk:direct=0\n";
}

std::string sdp_with_malformed_media_level_mediaclk() {
    return "v=0\n"
           "o=- 1 1 IN IP4 192.0.2.10\n"
           "s=Malformed media-level mediaclk\n"
           "t=0 0\n"
           "a=ts-refclk:ptp=IEEE1588-2008:39-A7-94-FF-FE-07-CB-D0:37\n"
           "m=audio 5004 RTP/AVP 101\n"
           "c=IN IP4 239.10.10.10/32\n"
           "a=rtpmap:101 L24/48000/2\n"
           "a=ptime:1\n"
           "a=mediaclk:direct=abc\n";
}

std::string sdp_with_unknown_ts_refclk_form() {
    return "v=0\n"
           "o=- 1 1 IN IP4 192.0.2.10\n"
           "s=Unknown ts-refclk form\n"
           "t=0 0\n"
           "a=ts-refclk:future-clock-source\n"
           "m=audio 5004 RTP/AVP 101\n"
           "c=IN IP4 239.10.10.10/32\n"
           "a=rtpmap:101 L24/48000/2\n"
           "a=ptime:1\n"
           "a=mediaclk:direct=0\n";
}

std::string sdp_with_unknown_media_level_mediaclk_form() {
    return "v=0\n"
           "o=- 1 1 IN IP4 192.0.2.10\n"
           "s=Unknown media-level mediaclk form\n"
           "t=0 0\n"
           "a=ts-refclk:ptp=IEEE1588-2008:39-A7-94-FF-FE-07-CB-D0:37\n"
           "m=audio 5004 RTP/AVP 101\n"
           "c=IN IP4 239.10.10.10/32\n"
           "a=rtpmap:101 L24/48000/2\n"
           "a=ptime:1\n"
           "a=mediaclk:capture=relative\n";
}

void valid_level_a_sdp_maps_to_audio_stream_signaling() {
    auto parsed = st2110::parse_audio_stream_signaling_from_sdp(valid_level_a_sdp_with_channel_order(), kPayloadType);

    assert(parsed.has_value());

    const auto &signaling = *parsed;

    assert(signaling.media.pcm_encoding == st2110::AudioPcmEncoding::LinearPcm);
    assert(signaling.media.sampling_rate_hz == 48000);
    assert(signaling.media.packet_time_us == 1000);
    assert(signaling.media.channel_count == 2);

    assert(signaling.channel_order.has_value());
    assert(signaling.channel_order->convention == st2110::AudioChannelOrderConvention::Smpte2110);
    assert(signaling.channel_order->raw_value == "SMPTE2110.(ST)");
}

void payload_type_mismatch_is_rejected() {
    auto parsed = st2110::parse_audio_stream_signaling_from_sdp(valid_level_a_sdp_with_channel_order(), 102);

    assert(!parsed.has_value());
    assert(parsed.error() == st2110::Error::InvalidValue);
}

void missing_required_rtpmap_is_rejected() {
    auto parsed = st2110::parse_audio_stream_signaling_from_sdp(sdp_missing_rtpmap(), kPayloadType);

    assert(!parsed.has_value());
    assert(parsed.error() == st2110::Error::InvalidValue);
}

void invalid_ptime_is_rejected() {
    auto parsed = st2110::parse_audio_stream_signaling_from_sdp(sdp_invalid_ptime(), kPayloadType);

    assert(!parsed.has_value());
    assert(parsed.error() == st2110::Error::InvalidValue);
}

void unsupported_encoding_is_rejected() {
    auto parsed = st2110::parse_audio_stream_signaling_from_sdp(sdp_with_unsupported_encoding(), kPayloadType);

    assert(!parsed.has_value());
    assert(parsed.error() == st2110::Error::Unsupported);
}

void missing_ts_refclk_is_rejected_by_final_ingestion() {
    auto parsed = st2110::parse_audio_stream_signaling_from_sdp(sdp_missing_ts_refclk(), kPayloadType);

    assert(!parsed.has_value());
    assert(parsed.error() == st2110::Error::InvalidValue);
}

void missing_media_level_mediaclk_is_rejected_by_final_ingestion() {
    auto parsed = st2110::parse_audio_stream_signaling_from_sdp(sdp_missing_media_level_mediaclk(), kPayloadType);

    assert(!parsed.has_value());
    assert(parsed.error() == st2110::Error::InvalidValue);
}

void session_level_only_mediaclk_is_not_enough_for_final_ingestion() {
    auto parsed = st2110::parse_audio_stream_signaling_from_sdp(sdp_with_session_level_only_mediaclk(), kPayloadType);

    assert(!parsed.has_value());
    assert(parsed.error() == st2110::Error::InvalidValue);
}

void malformed_known_ts_refclk_is_rejected_by_final_ingestion() {
    auto parsed = st2110::parse_audio_stream_signaling_from_sdp(sdp_with_malformed_known_ts_refclk(), kPayloadType);

    assert(!parsed.has_value());
    assert(parsed.error() == st2110::Error::InvalidValue);
}

void malformed_media_level_mediaclk_is_rejected_by_final_ingestion() {
    auto parsed = st2110::parse_audio_stream_signaling_from_sdp(sdp_with_malformed_media_level_mediaclk(), kPayloadType);

    assert(!parsed.has_value());
    assert(parsed.error() == st2110::Error::InvalidValue);
}

void unknown_ts_refclk_form_is_accepted_as_open_ended_reference_clock_signaling() {
    auto parsed = st2110::parse_audio_stream_signaling_from_sdp(sdp_with_unknown_ts_refclk_form(), kPayloadType);

    assert(parsed.has_value());

    const auto &signaling = *parsed;
    assert(signaling.media.pcm_encoding == st2110::AudioPcmEncoding::LinearPcm);
    assert(signaling.media.sampling_rate_hz == 48000);
    assert(signaling.media.packet_time_us == 1000);
    assert(signaling.media.channel_count == 2);
}

void unknown_media_level_mediaclk_form_is_accepted_as_open_ended_clock_signaling() {
    auto parsed = st2110::parse_audio_stream_signaling_from_sdp(sdp_with_unknown_media_level_mediaclk_form(), kPayloadType);

    assert(parsed.has_value());

    const auto &signaling = *parsed;
    assert(signaling.media.pcm_encoding == st2110::AudioPcmEncoding::LinearPcm);
    assert(signaling.media.sampling_rate_hz == 48000);
    assert(signaling.media.packet_time_us == 1000);
    assert(signaling.media.channel_count == 2);
}

void unknown_attributes_are_preserved_raw_but_ignored_by_final_mapping() {
    const std::string sdp = valid_level_a_sdp_without_channel_order_but_with_unknowns();

    auto raw = st2110::select_raw_audio_sdp_media_section(sdp, kPayloadType);
    assert(raw.has_value());

    assert(has_attribute(raw->unknown_session_attributes, "x-session-unknown", "preserve-session"));
    assert(has_attribute(raw->unknown_attributes, "x-media-unknown", "preserve-media"));

    auto parsed = st2110::parse_audio_stream_signaling_from_sdp(sdp, kPayloadType);
    assert(parsed.has_value());

    const auto &signaling = *parsed;

    assert(signaling.media.pcm_encoding == st2110::AudioPcmEncoding::LinearPcm);
    assert(signaling.media.sampling_rate_hz == 48000);
    assert(signaling.media.packet_time_us == 1000);
    assert(signaling.media.channel_count == 1);
    assert(!signaling.channel_order.has_value());
}
} // namespace

int main() {
    valid_level_a_sdp_maps_to_audio_stream_signaling();
    payload_type_mismatch_is_rejected();
    missing_required_rtpmap_is_rejected();
    invalid_ptime_is_rejected();
    unsupported_encoding_is_rejected();
    missing_ts_refclk_is_rejected_by_final_ingestion();
    missing_media_level_mediaclk_is_rejected_by_final_ingestion();
    session_level_only_mediaclk_is_not_enough_for_final_ingestion();
    malformed_known_ts_refclk_is_rejected_by_final_ingestion();
    malformed_media_level_mediaclk_is_rejected_by_final_ingestion();
    unknown_ts_refclk_form_is_accepted_as_open_ended_reference_clock_signaling();
    unknown_media_level_mediaclk_form_is_accepted_as_open_ended_clock_signaling();
    unknown_attributes_are_preserved_raw_but_ignored_by_final_mapping();

    return 0;
}
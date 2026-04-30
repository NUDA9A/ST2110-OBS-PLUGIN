#include <st2110/audio_sdp_media_section.hpp>
#include <st2110/error.hpp>

#include <cassert>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

int main() {
    using namespace st2110;

    static_assert(
        std::is_same_v<decltype(select_raw_audio_sdp_media_section(std::declval<std::string_view>(), std::uint8_t{})),
                       std::expected<RawAudioSdpMediaSection, Error>>);

    {
        const std::string sdp = "v=0\r\n"
                                "o=- 1 1 IN IP4 192.0.2.1\r\n"
                                "s=Audio test\r\n"
                                "a=tool:test-suite\r\n"
                                "m=audio 5004 RTP/AVP 111\r\n"
                                "c=IN IP4 239.1.1.10/32\r\n"
                                "a=rtpmap:111 L24/48000/2\r\n"
                                "a=ptime:1\r\n"
                                "a=fmtp:111 channel-order=SMPTE2110.(ST)\r\n"
                                "a=x-audio-future:preserve-me\r\n";

        auto raw = select_raw_audio_sdp_media_section(sdp, 111);

        assert(raw.has_value());
        assert(raw->media_line == "audio 5004 RTP/AVP 111");
        assert(raw->payload_type == 111);
        assert(raw->media_payload_types.size() == 1);
        assert(raw->media_payload_types[0] == 111);

        assert(raw->rtpmap == "L24/48000/2");
        assert(raw->parsed_rtpmap.encoding_name == "L24");
        assert(raw->parsed_rtpmap.sampling_rate_hz == 48000);
        assert(raw->parsed_rtpmap.channel_count.has_value());
        assert(*raw->parsed_rtpmap.channel_count == 2);

        assert(raw->packet_time_us.has_value());
        assert(*raw->packet_time_us == 1000);

        assert(raw->fmtp == "channel-order=SMPTE2110.(ST)");
        assert(raw->parsed_fmtp.channel_order.has_value());
        assert(*raw->parsed_fmtp.channel_order == "SMPTE2110.(ST)");

        assert(raw->channel_order.has_value());
        assert(*raw->channel_order == "SMPTE2110.(ST)");

        assert(raw->unknown_session_attributes.size() == 1);
        assert(raw->unknown_session_attributes[0].name == "tool");
        assert(raw->unknown_session_attributes[0].value == "test-suite");

        assert(raw->unknown_attributes.size() == 2);
        assert(raw->unknown_attributes[0].name == "c");
        assert(raw->unknown_attributes[0].value == "IN IP4 239.1.1.10/32");
        assert(raw->unknown_attributes[1].name == "x-audio-future");
        assert(raw->unknown_attributes[1].value == "preserve-me");
    }

    {
        const std::string sdp = "v=0\r\n"
                                "m=video 5000 RTP/AVP 112\r\n"
                                "a=rtpmap:112 raw/90000\r\n"
                                "m=audio 5004 RTP/AVP 110 111\r\n"
                                "a=rtpmap:110 L16/48000/2\r\n"
                                "a=rtpmap:111 L24/48000/8\r\n"
                                "a=ptime:1\r\n";

        auto raw = select_raw_audio_sdp_media_section(sdp, 111);

        assert(raw.has_value());
        assert(raw->payload_type == 111);
        assert(raw->media_payload_types.size() == 2);
        assert(raw->media_payload_types[0] == 110);
        assert(raw->media_payload_types[1] == 111);

        assert(raw->rtpmap == "L24/48000/8");
        assert(raw->parsed_rtpmap.encoding_name == "L24");
        assert(raw->parsed_rtpmap.sampling_rate_hz == 48000);
        assert(raw->parsed_rtpmap.channel_count.has_value());
        assert(*raw->parsed_rtpmap.channel_count == 8);
    }

    {
        const std::string sdp = "v=0\r\n"
                                "m=audio 5004 RTP/AVP 111\r\n"
                                "a=rtpmap:111 L24/48000/2\r\n"
                                "a=ptime:0.125\r\n";

        auto raw = select_raw_audio_sdp_media_section(sdp, 111);

        assert(raw.has_value());
        assert(raw->packet_time_us.has_value());
        assert(*raw->packet_time_us == 125);
    }

    {
        const std::string sdp = "v=0\r\n"
                                "m=audio 5004 RTP/AVP 111\r\n"
                                "a=rtpmap:111 L24/48000\r\n"
                                "a=ptime:1\r\n";

        auto raw = select_raw_audio_sdp_media_section(sdp, 111);

        assert(raw.has_value());
        assert(raw->parsed_rtpmap.encoding_name == "L24");
        assert(raw->parsed_rtpmap.sampling_rate_hz == 48000);
        assert(!raw->parsed_rtpmap.channel_count.has_value());
    }

    {
        const std::string sdp = "v=0\r\n"
                                "m=audio 5004 RTP/AVP 111\r\n"
                                "a=rtpmap:111 L24/48000/2\r\n"
                                "a=fmtp:111 channel-order=SMPTE2110.(51,ST); future-param=kept\r\n";

        auto raw = select_raw_audio_sdp_media_section(sdp, 111);

        assert(raw.has_value());
        assert(raw->parsed_fmtp.channel_order.has_value());
        assert(*raw->parsed_fmtp.channel_order == "SMPTE2110.(51,ST)");
        assert(raw->parsed_fmtp.unknown_parameters.size() == 1);
        assert(raw->parsed_fmtp.unknown_parameters[0].name == "future-param");
        assert(raw->parsed_fmtp.unknown_parameters[0].value == "kept");
    }

    {
        const std::string sdp = "v=0\r\n"
                                "m=audio 5004 RTP/AVP 111\r\n"
                                "a=rtpmap:111 L24/48000/2\r\n"
                                "a=channel-order:SMPTE2110.(ST)\r\n";

        auto raw = select_raw_audio_sdp_media_section(sdp, 111);

        assert(raw.has_value());
        assert(!raw->channel_order.has_value());
        assert(!raw->parsed_fmtp.channel_order.has_value());
        assert(raw->unknown_attributes.size() == 1);
        assert(raw->unknown_attributes[0].name == "channel-order");
        assert(raw->unknown_attributes[0].value == "SMPTE2110.(ST)");
    }

    {
        const std::string sdp = "v=0\r\n"
                                "m=audio 5004 RTP/AVP 110\r\n"
                                "a=rtpmap:110 L24/48000/2\r\n";

        auto raw = select_raw_audio_sdp_media_section(sdp, 111);

        assert(!raw.has_value());
        assert(raw.error() == Error::InvalidValue);
    }

    {
        const std::string sdp = "v=0\r\n"
                                "m=audio 5004 RTP/AVP 111\r\n"
                                "a=ptime:1\r\n";

        auto raw = select_raw_audio_sdp_media_section(sdp, 111);

        assert(!raw.has_value());
        assert(raw.error() == Error::InvalidValue);
    }

    {
        const std::string sdp = "v=0\r\n"
                                "m=audio 5004 RTP/AVP 111\r\n"
                                "a=rtpmap:111 L24/48000/2\r\n"
                                "a=rtpmap:111 L24/48000/2\r\n";

        auto raw = select_raw_audio_sdp_media_section(sdp, 111);

        assert(!raw.has_value());
        assert(raw.error() == Error::InvalidValue);
    }

    {
        const std::string sdp = "v=0\r\n"
                                "m=audio 5004 RTP/AVP 111\r\n"
                                "a=rtpmap:111 L24/48000/2\r\n"
                                "a=ptime:1\r\n"
                                "a=ptime:1\r\n";

        auto raw = select_raw_audio_sdp_media_section(sdp, 111);

        assert(!raw.has_value());
        assert(raw.error() == Error::InvalidValue);
    }

    {
        const std::string sdp = "v=0\r\n"
                                "m=audio 5004 RTP/AVP 111\r\n"
                                "a=rtpmap:111 L24/48000/2\r\n"
                                "a=fmtp:111 channel-order=SMPTE2110.(ST)\r\n"
                                "a=fmtp:111 channel-order=SMPTE2110.(ST)\r\n";

        auto raw = select_raw_audio_sdp_media_section(sdp, 111);

        assert(!raw.has_value());
        assert(raw.error() == Error::InvalidValue);
    }

    {
        const std::string sdp = "v=0\r\n"
                                "m=audio 5004 RTP/AVP 111\r\n"
                                "a=rtpmap:111 L24/48000/2\r\n"
                                "a=fmtp:111 channel-order=SMPTE2110.(ST); channel-order=SMPTE2110.(ST)\r\n";

        auto raw = select_raw_audio_sdp_media_section(sdp, 111);

        assert(!raw.has_value());
        assert(raw.error() == Error::InvalidValue);
    }

    {
        const std::string sdp = "v=0\r\n"
                                "m=audio 5004 RTP/AVP 111\r\n"
                                "a=rtpmap:111 L24/not-a-rate/2\r\n";

        auto raw = select_raw_audio_sdp_media_section(sdp, 111);

        assert(!raw.has_value());
        assert(raw.error() == Error::InvalidValue);
    }

    {
        const std::string sdp = "v=0\r\n"
                                "m=audio 5004 RTP/AVP 111\r\n"
                                "a=rtpmap:111 L24/48000/0\r\n";

        auto raw = select_raw_audio_sdp_media_section(sdp, 111);

        assert(!raw.has_value());
        assert(raw.error() == Error::InvalidValue);
    }

    {
        const std::string sdp = "v=0\r\n"
                                "m=audio 5004 RTP/AVP 111\r\n"
                                "a=rtpmap:111 L24/48000/2/extra\r\n";

        auto raw = select_raw_audio_sdp_media_section(sdp, 111);

        assert(!raw.has_value());
        assert(raw.error() == Error::InvalidValue);
    }

    {
        const std::string sdp = "v=0\r\n"
                                "m=audio 5004 RTP/AVP 111\r\n"
                                "a=rtpmap:111 L24/48000/2\r\n"
                                "a=ptime:0\r\n";

        auto raw = select_raw_audio_sdp_media_section(sdp, 111);

        assert(!raw.has_value());
        assert(raw.error() == Error::InvalidValue);
    }

    {
        const std::string sdp = "v=0\r\n"
                                "m=audio 5004 RTP/AVP 111\r\n"
                                "a=rtpmap:111 L24/48000/2\r\n"
                                "a=ptime:0.0001\r\n";

        auto raw = select_raw_audio_sdp_media_section(sdp, 111);

        assert(!raw.has_value());
        assert(raw.error() == Error::InvalidValue);
    }

    return 0;
}
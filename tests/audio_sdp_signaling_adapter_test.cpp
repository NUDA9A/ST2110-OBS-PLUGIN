#include <st2110/foundation/error.hpp>
#include <st2110/ingress/audio/audio_sdp_media_section.hpp>
#include <st2110/ingress/audio/audio_sdp_signaling_adapter.hpp>
#include <st2110/model/audio/audio_signaling.hpp>

#include <array>
#include <cassert>
#include <cstdint>
#include <expected>
#include <string>
#include <type_traits>
#include <utility>

int main() {
    using namespace st2110;

    static_assert(std::is_same_v<decltype(audio_stream_signaling_from_raw_audio_sdp_media_section(
                                     std::declval<const RawAudioSdpMediaSection &>())),
                                 std::expected<AudioStreamSignaling, Error>>);

    const std::array<AudioConformanceRange, 1> supported_level_a{audio_level_a_receiver_baseline()};

    {
        const std::string sdp = "v=0\r\n"
                                "m=audio 5004 RTP/AVP 111\r\n"
                                "a=rtpmap:111 L24/48000/2\r\n"
                                "a=ptime:1\r\n"
                                "a=fmtp:111 channel-order=SMPTE2110.(ST)\r\n";

        auto raw = select_raw_audio_sdp_media_section(sdp, 111);
        assert(raw.has_value());

        auto signaling = audio_stream_signaling_from_raw_audio_sdp_media_section(*raw);
        assert(signaling.has_value());

        assert(signaling->media.pcm_encoding == AudioPcmEncoding::LinearPcm);
        assert(signaling->media.sampling_rate_hz == 48000);
        assert(signaling->media.packet_time_us == 1000);
        assert(signaling->media.channel_count == 2);

        assert(signaling->channel_order.has_value());
        assert(signaling->channel_order->convention == AudioChannelOrderConvention::Smpte2110);
        assert(signaling->channel_order->raw_value == "SMPTE2110.(ST)");
        assert(signaling->media.pcm_bit_depth == AudioPcmBitDepth::Bits24);

        assert(validate_audio_stream_signaling(*signaling) == Error::Ok);
        assert(validate_audio_stream_signaling_against_conformance_ranges(*signaling, supported_level_a) == Error::Ok);
    }

    {
        for (const std::uint16_t channel_count : {std::uint16_t{1}, std::uint16_t{8}}) {
            const std::string sdp = "v=0\r\n"
                                    "m=audio 5004 RTP/AVP 111\r\n"
                                    "a=rtpmap:111 L24/48000/" +
                                    std::to_string(channel_count) +
                                    "\r\n"
                                    "a=ptime:1\r\n";

            auto raw = select_raw_audio_sdp_media_section(sdp, 111);
            assert(raw.has_value());

            auto signaling = audio_stream_signaling_from_raw_audio_sdp_media_section(*raw);
            assert(signaling.has_value());

            assert(signaling->media.pcm_encoding == AudioPcmEncoding::LinearPcm);
            assert(signaling->media.sampling_rate_hz == 48000);
            assert(signaling->media.packet_time_us == 1000);
            assert(signaling->media.channel_count == channel_count);
            assert(!signaling->channel_order.has_value());
            assert(validate_audio_stream_signaling(*signaling) == Error::Ok);
            assert(validate_audio_stream_signaling_against_conformance_ranges(*signaling, supported_level_a) ==
                   Error::Ok);
        }
        {
            const std::string sdp = "v=0\r\n"
                                    "m=audio 5004 RTP/AVP 111\r\n"
                                    "a=rtpmap:111 L16/48000/2\r\n"
                                    "a=ptime:1\r\n";

            auto raw = select_raw_audio_sdp_media_section(sdp, 111);
            assert(raw.has_value());

            auto signaling = audio_stream_signaling_from_raw_audio_sdp_media_section(*raw);
            assert(signaling.has_value());

            assert(signaling->media.pcm_encoding == AudioPcmEncoding::LinearPcm);
            assert(signaling->media.pcm_bit_depth == AudioPcmBitDepth::Bits16);
            assert(signaling->media.sampling_rate_hz == 48000);
            assert(signaling->media.packet_time_us == 1000);
            assert(signaling->media.channel_count == 2);
            assert(validate_audio_stream_signaling(*signaling) == Error::Ok);
            assert(validate_audio_stream_signaling_against_conformance_ranges(*signaling, supported_level_a) ==
                   Error::Ok);
        }
    }

    {
        const std::string sdp = "v=0\r\n"
                                "m=audio 5004 RTP/AVP 111\r\n"
                                "a=rtpmap:111 OPUS/48000/2\r\n"
                                "a=ptime:1\r\n";

        auto raw = select_raw_audio_sdp_media_section(sdp, 111);
        assert(raw.has_value());

        auto signaling = audio_stream_signaling_from_raw_audio_sdp_media_section(*raw);
        assert(!signaling.has_value());
        assert(signaling.error() == Error::Unsupported);
    }

    {
        const std::string sdp = "v=0\r\n"
                                "m=audio 5004 RTP/AVP 111\r\n"
                                "a=rtpmap:111 L24/48000/2\r\n";

        auto raw = select_raw_audio_sdp_media_section(sdp, 111);
        assert(raw.has_value());
        assert(!raw->packet_time_us.has_value());

        auto signaling = audio_stream_signaling_from_raw_audio_sdp_media_section(*raw);
        assert(!signaling.has_value());
        assert(signaling.error() == Error::InvalidValue);
    }

    {
        const std::string sdp = "v=0\r\n"
                                "m=audio 5004 RTP/AVP 111\r\n"
                                "a=rtpmap:111 L24/48000\r\n"
                                "a=ptime:1\r\n";

        auto raw = select_raw_audio_sdp_media_section(sdp, 111);
        assert(raw.has_value());
        assert(!raw->parsed_rtpmap.channel_count.has_value());

        auto signaling = audio_stream_signaling_from_raw_audio_sdp_media_section(*raw);
        assert(!signaling.has_value());
        assert(signaling.error() == Error::InvalidValue);
    }

    {
        const std::string sdp = "v=0\r\n"
                                "m=audio 5004 RTP/AVP 111\r\n"
                                "a=rtpmap:111 L24/96000/2\r\n"
                                "a=ptime:1\r\n";

        auto raw = select_raw_audio_sdp_media_section(sdp, 111);
        assert(raw.has_value());

        auto signaling = audio_stream_signaling_from_raw_audio_sdp_media_section(*raw);
        assert(signaling.has_value());
        assert(signaling->media.pcm_encoding == AudioPcmEncoding::LinearPcm);
        assert(signaling->media.pcm_bit_depth == AudioPcmBitDepth::Bits24);
        assert(signaling->media.sampling_rate_hz == 96000);
        assert(signaling->media.packet_time_us == 1000);
        assert(signaling->media.channel_count == 2);
        assert(validate_audio_stream_signaling(*signaling) == Error::Ok);
        assert(validate_audio_stream_signaling_against_conformance_ranges(*signaling, supported_level_a) ==
               Error::Unsupported);
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

        auto signaling = audio_stream_signaling_from_raw_audio_sdp_media_section(*raw);
        assert(signaling.has_value());
        assert(signaling->media.pcm_encoding == AudioPcmEncoding::LinearPcm);
        assert(signaling->media.pcm_bit_depth == AudioPcmBitDepth::Bits24);
        assert(signaling->media.sampling_rate_hz == 48000);
        assert(signaling->media.packet_time_us == 125);
        assert(signaling->media.channel_count == 2);
        assert(validate_audio_stream_signaling(*signaling) == Error::Ok);
        assert(validate_audio_stream_signaling_against_conformance_ranges(*signaling, supported_level_a) ==
               Error::Unsupported);
    }

    {
        const std::string sdp = "v=0\r\n"
                                "m=audio 5004 RTP/AVP 111\r\n"
                                "a=rtpmap:111 L24/48000/2\r\n"
                                "a=ptime:1\r\n"
                                "a=fmtp:111 channel-order=ACME.(X)\r\n";

        auto raw = select_raw_audio_sdp_media_section(sdp, 111);
        assert(raw.has_value());

        auto signaling = audio_stream_signaling_from_raw_audio_sdp_media_section(*raw);
        assert(signaling.has_value());

        assert(signaling->channel_order.has_value());
        assert(signaling->channel_order->convention == AudioChannelOrderConvention::Other);
        assert(signaling->channel_order->raw_value == "ACME.(X)");
        assert(validate_audio_stream_signaling(*signaling) == Error::Ok);
        assert(validate_audio_stream_signaling_against_conformance_ranges(*signaling, supported_level_a) == Error::Ok);
    }

    {
        const std::string sdp = "v=0\r\n"
                                "m=audio 5004 RTP/AVP 111\r\n"
                                "a=rtpmap:111 L24/48000/2\r\n"
                                "a=ptime:1\r\n"
                                "a=channel-order:SMPTE2110.(ST)\r\n";

        auto raw = select_raw_audio_sdp_media_section(sdp, 111);
        assert(raw.has_value());
        assert(!raw->channel_order.has_value());
        assert(raw->unknown_attributes.size() == 1);
        assert(raw->unknown_attributes[0].name == "channel-order");

        auto signaling = audio_stream_signaling_from_raw_audio_sdp_media_section(*raw);
        assert(signaling.has_value());
        assert(!signaling->channel_order.has_value());
        assert(validate_audio_stream_signaling(*signaling) == Error::Ok);
        assert(validate_audio_stream_signaling_against_conformance_ranges(*signaling, supported_level_a) == Error::Ok);
    }

    return 0;
}
#include "st2110/audio_signaling.hpp"
#include "st2110/error.hpp"

#include <cassert>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

namespace {
    st2110::AudioStreamSignaling make_level_a_stream(std::uint16_t channels) {
        using namespace st2110;

        AudioStreamSignaling signaling{};
        signaling.media.pcm_encoding = AudioPcmEncoding::LinearPcm;
        signaling.media.sampling_rate_hz = 48000;
        signaling.media.packet_time_us = 1000;
        signaling.media.channel_count = channels;
        return signaling;
    }
}

int main() {
    using namespace st2110;

    static_assert(std::is_enum_v<AudioConformanceLevel>);
    static_assert(std::is_enum_v<AudioPcmEncoding>);
    static_assert(std::is_enum_v<AudioChannelOrderConvention>);

    static_assert(!std::is_convertible_v<AudioConformanceLevel, int>);
    static_assert(!std::is_convertible_v<AudioPcmEncoding, int>);
    static_assert(!std::is_convertible_v<AudioChannelOrderConvention, int>);

    static_assert(std::is_same_v<
                  decltype(validate_audio_conformance_range(std::declval<const AudioConformanceRange&>())),
                  Error>);

    static_assert(std::is_same_v<
                  decltype(audio_media_description_matches_conformance_range(
                          std::declval<const AudioMediaDescription&>(),
                          std::declval<const AudioConformanceRange&>())),
                  bool>);

    static_assert(std::is_same_v<
                  decltype(validate_audio_media_description_against_conformance_range(
                          std::declval<const AudioMediaDescription&>(),
                          std::declval<const AudioConformanceRange&>())),
                  Error>);

    static_assert(std::is_same_v<
                  decltype(validate_audio_channel_order_signaling(
                          std::declval<const AudioChannelOrderSignaling&>())),
                  Error>);

    static_assert(std::is_same_v<
                  decltype(validate_audio_stream_signaling(std::declval<const AudioStreamSignaling&>())),
                  Error>);

    constexpr auto level_a = audio_level_a_receiver_baseline();

    static_assert(level_a.level == AudioConformanceLevel::LevelA);
    static_assert(level_a.sampling_rate_hz == 48000);
    static_assert(level_a.packet_time_us == 1000);
    static_assert(level_a.min_channels == 1);
    static_assert(level_a.max_channels == 8);

    static_assert(validate_audio_conformance_range(level_a) == Error::Ok);

    constexpr AudioConformanceRange invalid_zero_min_channels{
            AudioConformanceLevel::LevelA,
            48000,
            1000,
            0,
            8
    };
    static_assert(validate_audio_conformance_range(invalid_zero_min_channels) == Error::InvalidValue);

    constexpr AudioConformanceRange invalid_reversed_channel_range{
            AudioConformanceLevel::LevelA,
            48000,
            1000,
            8,
            1
    };
    static_assert(validate_audio_conformance_range(invalid_reversed_channel_range) == Error::InvalidValue);

    constexpr AudioConformanceRange invalid_zero_sampling_rate{
            AudioConformanceLevel::LevelA,
            0,
            1000,
            1,
            8
    };
    static_assert(validate_audio_conformance_range(invalid_zero_sampling_rate) == Error::InvalidValue);

    constexpr AudioConformanceRange invalid_zero_packet_time{
            AudioConformanceLevel::LevelA,
            48000,
            0,
            1,
            8
    };
    static_assert(validate_audio_conformance_range(invalid_zero_packet_time) == Error::InvalidValue);

    constexpr AudioConformanceRange invalid_unknown_level{
            static_cast<AudioConformanceLevel>(255),
            48000,
            1000,
            1,
            8
    };
    static_assert(validate_audio_conformance_range(invalid_unknown_level) == Error::InvalidValue);

    AudioStreamSignaling level_a_stereo = make_level_a_stream(2);

    assert(level_a_stereo.media.pcm_encoding == AudioPcmEncoding::LinearPcm);
    assert(level_a_stereo.media.sampling_rate_hz == 48000);
    assert(level_a_stereo.media.packet_time_us == 1000);
    assert(level_a_stereo.media.channel_count == 2);

    assert(audio_media_description_matches_conformance_range(level_a_stereo.media, level_a));
    assert(validate_audio_media_description_against_conformance_range(level_a_stereo.media, level_a) == Error::Ok);
    assert(validate_audio_stream_signaling(level_a_stereo) == Error::Ok);

    // Absence of channel-order is explicitly representable.
    // ST 2110-30 says absent channel-order means channels are treated as Undefined.
    assert(!level_a_stereo.channel_order.has_value());

    AudioStreamSignaling level_a_min_channels = make_level_a_stream(1);
    assert(validate_audio_stream_signaling(level_a_min_channels) == Error::Ok);

    AudioStreamSignaling level_a_max_channels = make_level_a_stream(8);
    assert(validate_audio_stream_signaling(level_a_max_channels) == Error::Ok);

    AudioStreamSignaling zero_channels = make_level_a_stream(0);
    assert(validate_audio_stream_signaling(zero_channels) == Error::InvalidValue);

    AudioStreamSignaling too_many_level_a_channels = make_level_a_stream(9);
    assert(validate_audio_stream_signaling(too_many_level_a_channels) == Error::InvalidValue);

    AudioStreamSignaling wrong_sampling_rate = make_level_a_stream(2);
    wrong_sampling_rate.media.sampling_rate_hz = 96000;
    assert(!audio_media_description_matches_conformance_range(wrong_sampling_rate.media, level_a));
    assert(validate_audio_stream_signaling(wrong_sampling_rate) == Error::InvalidValue);

    AudioStreamSignaling wrong_packet_time = make_level_a_stream(2);
    wrong_packet_time.media.packet_time_us = 125;
    assert(!audio_media_description_matches_conformance_range(wrong_packet_time.media, level_a));
    assert(validate_audio_stream_signaling(wrong_packet_time) == Error::InvalidValue);

    AudioStreamSignaling invalid_pcm_encoding = make_level_a_stream(2);
    invalid_pcm_encoding.media.pcm_encoding = static_cast<AudioPcmEncoding>(255);
    assert(validate_audio_stream_signaling(invalid_pcm_encoding) == Error::InvalidValue);

    AudioStreamSignaling ordered_eight_channel = make_level_a_stream(8);
    ordered_eight_channel.channel_order = AudioChannelOrderSignaling{
            AudioChannelOrderConvention::Smpte2110,
            "SMPTE2110.(51,ST)"
    };

    assert(ordered_eight_channel.channel_order.has_value());
    assert(ordered_eight_channel.channel_order->convention == AudioChannelOrderConvention::Smpte2110);
    assert(ordered_eight_channel.channel_order->raw_value == "SMPTE2110.(51,ST)");
    assert(validate_audio_channel_order_signaling(*ordered_eight_channel.channel_order) == Error::Ok);
    assert(validate_audio_stream_signaling(ordered_eight_channel) == Error::Ok);

    AudioStreamSignaling bad_empty_smpte2110_channel_order = make_level_a_stream(2);
    bad_empty_smpte2110_channel_order.channel_order = AudioChannelOrderSignaling{
            AudioChannelOrderConvention::Smpte2110,
            ""
    };
    assert(validate_audio_stream_signaling(bad_empty_smpte2110_channel_order) == Error::InvalidValue);

    AudioStreamSignaling bad_smpte2110_channel_order_prefix = make_level_a_stream(2);
    bad_smpte2110_channel_order_prefix.channel_order = AudioChannelOrderSignaling{
            AudioChannelOrderConvention::Smpte2110,
            "AES67.(ST)"
    };
    assert(validate_audio_stream_signaling(bad_smpte2110_channel_order_prefix) == Error::InvalidValue);

    AudioStreamSignaling explicitly_unspecified_channel_order = make_level_a_stream(2);
    explicitly_unspecified_channel_order.channel_order = AudioChannelOrderSignaling{
            AudioChannelOrderConvention::Unspecified,
            ""
    };
    assert(validate_audio_stream_signaling(explicitly_unspecified_channel_order) == Error::Ok);

    AudioStreamSignaling bad_unspecified_channel_order_with_raw_value = make_level_a_stream(2);
    bad_unspecified_channel_order_with_raw_value.channel_order = AudioChannelOrderSignaling{
            AudioChannelOrderConvention::Unspecified,
            "SMPTE2110.(ST)"
    };
    assert(validate_audio_stream_signaling(bad_unspecified_channel_order_with_raw_value) == Error::InvalidValue);

    AudioStreamSignaling future_unknown_channel_order = make_level_a_stream(2);
    future_unknown_channel_order.channel_order = AudioChannelOrderSignaling{
            AudioChannelOrderConvention::Other,
            "FUTURECONVENTION.(X)"
    };

    assert(future_unknown_channel_order.channel_order.has_value());
    assert(future_unknown_channel_order.channel_order->convention == AudioChannelOrderConvention::Other);
    assert(future_unknown_channel_order.channel_order->raw_value == "FUTURECONVENTION.(X)");
    assert(validate_audio_stream_signaling(future_unknown_channel_order) == Error::Ok);

    AudioStreamSignaling bad_empty_future_unknown_channel_order = make_level_a_stream(2);
    bad_empty_future_unknown_channel_order.channel_order = AudioChannelOrderSignaling{
            AudioChannelOrderConvention::Other,
            ""
    };
    assert(validate_audio_stream_signaling(bad_empty_future_unknown_channel_order) == Error::InvalidValue);

    AudioStreamSignaling invalid_channel_order_convention = make_level_a_stream(2);
    invalid_channel_order_convention.channel_order = AudioChannelOrderSignaling{
            static_cast<AudioChannelOrderConvention>(255),
            "SMPTE2110.(ST)"
    };
    assert(validate_audio_stream_signaling(invalid_channel_order_convention) == Error::InvalidValue);

    return 0;
}
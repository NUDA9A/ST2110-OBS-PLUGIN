#include "st2110/audio_signaling.hpp"

#include <cassert>
#include <optional>
#include <string>
#include <type_traits>

int main() {
    using namespace st2110;

    static_assert(std::is_enum_v<AudioConformanceLevel>);
    static_assert(std::is_enum_v<AudioPcmEncoding>);
    static_assert(std::is_enum_v<AudioChannelOrderConvention>);

    static_assert(!std::is_convertible_v<AudioConformanceLevel, int>);
    static_assert(!std::is_convertible_v<AudioPcmEncoding, int>);
    static_assert(!std::is_convertible_v<AudioChannelOrderConvention, int>);

    constexpr auto level_a = audio_level_a_receiver_baseline();

    static_assert(level_a.level == AudioConformanceLevel::LevelA);
    static_assert(level_a.sampling_rate_hz == 48000);
    static_assert(level_a.packet_time_us == 1000);
    static_assert(level_a.min_channels == 1);
    static_assert(level_a.max_channels == 8);

    AudioStreamSignaling level_a_stereo{};
    level_a_stereo.media.pcm_encoding = AudioPcmEncoding::LinearPcm;
    level_a_stereo.media.sampling_rate_hz = level_a.sampling_rate_hz;
    level_a_stereo.media.packet_time_us = level_a.packet_time_us;
    level_a_stereo.media.channel_count = 2;

    assert(level_a_stereo.media.pcm_encoding == AudioPcmEncoding::LinearPcm);
    assert(level_a_stereo.media.sampling_rate_hz == 48000);
    assert(level_a_stereo.media.packet_time_us == 1000);
    assert(level_a_stereo.media.channel_count == 2);

    // Absence of channel-order is explicitly representable.
    // Later validation/projection can interpret this as Undefined channels.
    assert(!level_a_stereo.channel_order.has_value());

    AudioStreamSignaling ordered_eight_channel{};
    ordered_eight_channel.media.pcm_encoding = AudioPcmEncoding::LinearPcm;
    ordered_eight_channel.media.sampling_rate_hz = 48000;
    ordered_eight_channel.media.packet_time_us = 1000;
    ordered_eight_channel.media.channel_count = 8;
    ordered_eight_channel.channel_order = AudioChannelOrderSignaling{
            AudioChannelOrderConvention::Smpte2110,
            "SMPTE2110.(51,ST)"
    };

    assert(ordered_eight_channel.channel_order.has_value());
    assert(ordered_eight_channel.channel_order->convention == AudioChannelOrderConvention::Smpte2110);
    assert(ordered_eight_channel.channel_order->raw_value == "SMPTE2110.(51,ST)");

    AudioStreamSignaling future_unknown_channel_order{};
    future_unknown_channel_order.media.pcm_encoding = AudioPcmEncoding::LinearPcm;
    future_unknown_channel_order.media.sampling_rate_hz = 48000;
    future_unknown_channel_order.media.packet_time_us = 1000;
    future_unknown_channel_order.media.channel_count = 2;
    future_unknown_channel_order.channel_order = AudioChannelOrderSignaling{
            AudioChannelOrderConvention::Other,
            "FUTURECONVENTION.(X)"
    };

    assert(future_unknown_channel_order.channel_order.has_value());
    assert(future_unknown_channel_order.channel_order->convention == AudioChannelOrderConvention::Other);
    assert(future_unknown_channel_order.channel_order->raw_value == "FUTURECONVENTION.(X)");

    return 0;
}
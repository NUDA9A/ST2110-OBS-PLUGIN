#include <st2110/audio_signaling.hpp>
#include <st2110/audio_signaling_rx_config.hpp>
#include <st2110/error.hpp>
#include <st2110/rx_config.hpp>

#include <cassert>
#include <cstdint>
#include <expected>
#include <string>
#include <type_traits>
#include <utility>

namespace {
st2110::AudioStreamSignaling make_level_a_stream(std::uint16_t channels = 2) {
    st2110::AudioStreamSignaling signaling{};
    signaling.media.pcm_encoding = st2110::AudioPcmEncoding::LinearPcm;
    signaling.media.sampling_rate_hz = 48000;
    signaling.media.packet_time_us = 1000;
    signaling.media.channel_count = channels;
    signaling.media.pcm_bit_depth = st2110::AudioPcmBitDepth::Bits24;
    return signaling;
}
} // namespace

int main() {
    using namespace st2110;

    static_assert(
        std::is_same_v<decltype(rx_audio_config_from_audio_stream_signaling(
                           std::declval<const AudioStreamSignaling &>(), std::uint16_t{}, std::uint8_t{},
                           std::declval<std::string>(), std::declval<std::string>(), AudioSampleFormat::LinearPcm)),
                       std::expected<RxAudioConfig, Error>>);

    AudioStreamSignaling stereo = make_level_a_stream(2);

    auto projected = rx_audio_config_from_audio_stream_signaling(stereo, 30000, 111, "0.0.0.0", "239.1.1.2");

    assert(projected.has_value());
    assert(projected->sampling_rate_hz == 48000);
    assert(projected->packet_time_us == 1000);
    assert(projected->samples_per_packet == 48);
    assert(projected->channel_count == 2);
    assert(projected->udp_port == 30000);
    assert(projected->payload_type == 111);
    assert(projected->local_ip == "0.0.0.0");
    assert(projected->dest_ip == "239.1.1.2");
    assert(projected->format == AudioSampleFormat::LinearPcm);
    assert(projected->pcm_bit_depth == AudioPcmBitDepth::Bits24);
    assert(projected->is_valid());

    AudioStreamSignaling min_channels = make_level_a_stream(1);
    auto projected_min = rx_audio_config_from_audio_stream_signaling(min_channels, 30000, 111, "", "239.1.1.2");
    assert(projected_min.has_value());
    assert(projected_min->channel_count == 1);
    assert(projected_min->local_ip.empty());
    assert(projected_min->is_valid());

    AudioStreamSignaling max_channels = make_level_a_stream(8);
    auto projected_max = rx_audio_config_from_audio_stream_signaling(max_channels, 30000, 111, "0.0.0.0", "239.1.1.2");
    assert(projected_max.has_value());
    assert(projected_max->channel_count == 8);
    assert(projected_max->is_valid());

    AudioStreamSignaling with_channel_order = make_level_a_stream(8);
    with_channel_order.channel_order =
        AudioChannelOrderSignaling{AudioChannelOrderConvention::Smpte2110, "SMPTE2110.(51,ST)"};

    auto projected_with_channel_order =
        rx_audio_config_from_audio_stream_signaling(with_channel_order, 30000, 111, "0.0.0.0", "239.1.1.2");

    assert(projected_with_channel_order.has_value());
    assert(projected_with_channel_order->channel_count == 8);
    assert(projected_with_channel_order->is_valid());

    AudioStreamSignaling l16_stream = make_level_a_stream(2);
    l16_stream.media.pcm_bit_depth = AudioPcmBitDepth::Bits16;

    auto projected_l16 =
        rx_audio_config_from_audio_stream_signaling(l16_stream, 30000, 111, "0.0.0.0", "239.1.1.2");

    assert(projected_l16.has_value());
    assert(projected_l16->pcm_bit_depth == AudioPcmBitDepth::Bits16);
    assert(projected_l16->is_valid());

    AudioStreamSignaling wrong_sampling_rate = make_level_a_stream(2);
    wrong_sampling_rate.media.sampling_rate_hz = 96000;
    assert(validate_audio_stream_signaling(wrong_sampling_rate) == Error::Ok);
    auto projected_wrong_sampling_rate =
        rx_audio_config_from_audio_stream_signaling(wrong_sampling_rate, 30000, 111, "0.0.0.0", "239.1.1.2");
    assert(!projected_wrong_sampling_rate.has_value());
    assert(projected_wrong_sampling_rate.error() == Error::Unsupported);

    AudioStreamSignaling wrong_packet_time = make_level_a_stream(2);
    wrong_packet_time.media.packet_time_us = 125;
    assert(validate_audio_stream_signaling(wrong_packet_time) == Error::Ok);
    auto projected_wrong_packet_time =
        rx_audio_config_from_audio_stream_signaling(wrong_packet_time, 30000, 111, "0.0.0.0", "239.1.1.2");
    assert(!projected_wrong_packet_time.has_value());
    assert(projected_wrong_packet_time.error() == Error::Unsupported);

    AudioStreamSignaling too_many_channels = make_level_a_stream(9);
    assert(validate_audio_stream_signaling(too_many_channels) == Error::Ok);
    auto projected_too_many_channels =
        rx_audio_config_from_audio_stream_signaling(too_many_channels, 30000, 111, "0.0.0.0", "239.1.1.2");
    assert(!projected_too_many_channels.has_value());
    assert(projected_too_many_channels.error() == Error::Unsupported);

    AudioStreamSignaling valid_signaling = make_level_a_stream(2);

    auto bad_port = rx_audio_config_from_audio_stream_signaling(valid_signaling, 0, 111, "0.0.0.0", "239.1.1.2");
    assert(!bad_port.has_value());
    assert(bad_port.error() == Error::InvalidValue);

    auto bad_payload_type =
        rx_audio_config_from_audio_stream_signaling(valid_signaling, 30000, 95, "0.0.0.0", "239.1.1.2");
    assert(!bad_payload_type.has_value());
    assert(bad_payload_type.error() == Error::InvalidValue);

    auto bad_dest_ip = rx_audio_config_from_audio_stream_signaling(valid_signaling, 30000, 111, "0.0.0.0", "");
    assert(!bad_dest_ip.has_value());
    assert(bad_dest_ip.error() == Error::InvalidValue);

    auto bad_format = rx_audio_config_from_audio_stream_signaling(valid_signaling, 30000, 111, "0.0.0.0", "239.1.1.2",
                                                                  static_cast<AudioSampleFormat>(255));
    assert(!bad_format.has_value());
    assert(bad_format.error() == Error::Unsupported);

    return 0;
}
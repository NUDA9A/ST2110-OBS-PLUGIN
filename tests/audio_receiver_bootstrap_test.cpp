#include "st2110/audio_receiver_bootstrap.hpp"

#include <cassert>
#include <string>

using namespace st2110;

namespace {
    AudioStreamSignaling make_level_a_audio_stream(uint16_t channel_count) {
        AudioStreamSignaling signaling{};
        signaling.media.pcm_encoding = AudioPcmEncoding::LinearPcm;
        signaling.media.sampling_rate_hz = 48000;
        signaling.media.packet_time_us = 1000;
        signaling.media.channel_count = channel_count;
        return signaling;
    }

    void bootstrap_from_absent_channel_order_derives_rx_config_and_undefined_group() {
        auto signaling = make_level_a_audio_stream(2);

        auto bootstrap = audio_receiver_bootstrap_config_from_audio_stream_signaling(
                signaling,
                5004,
                112,
                "",
                "239.1.1.1");

        assert(bootstrap.has_value());

        const RxAudioConfig& rx = bootstrap->rx_config;
        assert(rx.sampling_rate_hz == 48000);
        assert(rx.packet_time_us == 1000);
        assert(rx.samples_per_packet == 48);
        assert(rx.channel_count == 2);
        assert(rx.udp_port == 5004);
        assert(rx.payload_type == 112);
        assert(rx.local_ip.empty());
        assert(rx.dest_ip == "239.1.1.1");
        assert(rx.format == AudioSampleFormat::LinearPcm);
        assert(rx.is_valid());

        const ParsedAudioChannelOrder& order = bootstrap->channel_order;
        assert(order.convention == AudioChannelOrderConvention::Unspecified);
        assert(order.raw_value.empty());
        assert(order.declared_channel_count == 2);
        assert(order.groups.size() == 1);
        assert(order.groups[0].kind == AudioChannelGroupKind::Undefined);
        assert(order.groups[0].symbol == "U02");
        assert(order.groups[0].channel_count == 2);
    }

    void bootstrap_preserves_exact_smpte2110_channel_order() {
        auto signaling = make_level_a_audio_stream(2);
        signaling.channel_order = AudioChannelOrderSignaling{
                .convention = AudioChannelOrderConvention::Smpte2110,
                .raw_value = "SMPTE2110.(ST)"
        };

        auto bootstrap = audio_receiver_bootstrap_config_from_audio_stream_signaling(
                signaling,
                5004,
                112,
                "",
                "239.1.1.1");

        assert(bootstrap.has_value());

        assert(bootstrap->rx_config.channel_count == 2);
        assert(bootstrap->rx_config.samples_per_packet == 48);

        const ParsedAudioChannelOrder& order = bootstrap->channel_order;
        assert(order.convention == AudioChannelOrderConvention::Smpte2110);
        assert(order.raw_value == "SMPTE2110.(ST)");
        assert(order.declared_channel_count == 2);
        assert(order.groups.size() == 1);
        assert(order.groups[0].kind == AudioChannelGroupKind::Stereo);
        assert(order.groups[0].symbol == "ST");
        assert(order.groups[0].channel_count == 2);
    }

    void bootstrap_appends_undefined_group_for_under_declared_smpte2110_order() {
        auto signaling = make_level_a_audio_stream(8);
        signaling.channel_order = AudioChannelOrderSignaling{
                .convention = AudioChannelOrderConvention::Smpte2110,
                .raw_value = "SMPTE2110.(51)"
        };

        auto bootstrap = audio_receiver_bootstrap_config_from_audio_stream_signaling(
                signaling,
                5004,
                112,
                "",
                "239.1.1.1");

        assert(bootstrap.has_value());

        assert(bootstrap->rx_config.channel_count == 8);
        assert(bootstrap->rx_config.samples_per_packet == 48);

        const ParsedAudioChannelOrder& order = bootstrap->channel_order;
        assert(order.convention == AudioChannelOrderConvention::Smpte2110);
        assert(order.raw_value == "SMPTE2110.(51)");
        assert(order.declared_channel_count == 8);
        assert(order.groups.size() == 2);

        assert(order.groups[0].kind == AudioChannelGroupKind::FiveOne);
        assert(order.groups[0].symbol == "51");
        assert(order.groups[0].channel_count == 6);

        assert(order.groups[1].kind == AudioChannelGroupKind::Undefined);
        assert(order.groups[1].symbol == "U02");
        assert(order.groups[1].channel_count == 2);
    }

    void bootstrap_preserves_other_channel_order_as_unknown_convention() {
        auto signaling = make_level_a_audio_stream(2);
        signaling.channel_order = AudioChannelOrderSignaling{
                .convention = AudioChannelOrderConvention::Other,
                .raw_value = "vendor-specific-layout"
        };

        auto bootstrap = audio_receiver_bootstrap_config_from_audio_stream_signaling(
                signaling,
                5004,
                112,
                "",
                "239.1.1.1");

        assert(bootstrap.has_value());

        assert(bootstrap->rx_config.channel_count == 2);
        assert(bootstrap->rx_config.samples_per_packet == 48);

        const ParsedAudioChannelOrder& order = bootstrap->channel_order;
        assert(order.convention == AudioChannelOrderConvention::Other);
        assert(order.raw_value == "vendor-specific-layout");
        assert(order.declared_channel_count == 0);
        assert(order.groups.empty());
    }

    void bootstrap_rejects_over_declared_smpte2110_channel_order() {
        auto signaling = make_level_a_audio_stream(2);
        signaling.channel_order = AudioChannelOrderSignaling{
                .convention = AudioChannelOrderConvention::Smpte2110,
                .raw_value = "SMPTE2110.(51)"
        };

        auto bootstrap = audio_receiver_bootstrap_config_from_audio_stream_signaling(
                signaling,
                5004,
                112,
                "",
                "239.1.1.1");

        assert(!bootstrap.has_value());
        assert(bootstrap.error() == Error::InvalidValue);
    }

    void bootstrap_rejects_invalid_runtime_transport_fields() {
        auto signaling = make_level_a_audio_stream(2);

        auto bad_port = audio_receiver_bootstrap_config_from_audio_stream_signaling(
                signaling,
                0,
                112,
                "",
                "239.1.1.1");
        assert(!bad_port.has_value());
        assert(bad_port.error() == Error::InvalidValue);

        auto bad_payload_type = audio_receiver_bootstrap_config_from_audio_stream_signaling(
                signaling,
                5004,
                95,
                "",
                "239.1.1.1");
        assert(!bad_payload_type.has_value());
        assert(bad_payload_type.error() == Error::InvalidValue);

        auto bad_dest_ip = audio_receiver_bootstrap_config_from_audio_stream_signaling(
                signaling,
                5004,
                112,
                "",
                "");
        assert(!bad_dest_ip.has_value());
        assert(bad_dest_ip.error() == Error::InvalidValue);
    }

    void bootstrap_rejects_unsupported_runtime_sample_format() {
        auto signaling = make_level_a_audio_stream(2);

        auto bootstrap = audio_receiver_bootstrap_config_from_audio_stream_signaling(
                signaling,
                5004,
                112,
                "",
                "239.1.1.1",
                static_cast<AudioSampleFormat>(255));

        assert(!bootstrap.has_value());
        assert(bootstrap.error() == Error::Unsupported);
    }
}

int main() {
    bootstrap_from_absent_channel_order_derives_rx_config_and_undefined_group();
    bootstrap_preserves_exact_smpte2110_channel_order();
    bootstrap_appends_undefined_group_for_under_declared_smpte2110_order();
    bootstrap_preserves_other_channel_order_as_unknown_convention();
    bootstrap_rejects_over_declared_smpte2110_channel_order();
    bootstrap_rejects_invalid_runtime_transport_fields();
    bootstrap_rejects_unsupported_runtime_sample_format();

    return 0;
}
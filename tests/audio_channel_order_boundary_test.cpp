#include <st2110/audio_channel_order.hpp>
#include <st2110/audio_signaling.hpp>

#include <cassert>
#include <cstdint>
#include <expected>
#include <string>

namespace {
    st2110::AudioStreamSignaling make_level_a_stream(
            uint16_t channel_count,
            std::string channel_order = {}) {
        st2110::AudioStreamSignaling signaling{};

        signaling.media.pcm_encoding = st2110::AudioPcmEncoding::LinearPcm;
        signaling.media.sampling_rate_hz = 48000;
        signaling.media.packet_time_us = 1000;
        signaling.media.channel_count = channel_count;

        if (!channel_order.empty()) {
            signaling.channel_order = st2110::AudioChannelOrderSignaling{
                    .convention = st2110::AudioChannelOrderConvention::Smpte2110,
                    .raw_value = std::move(channel_order)
            };
        }

        return signaling;
    }

    void parses_smpte2110_surround_and_stereo_groups() {
        const st2110::AudioChannelOrderSignaling channel_order{
                .convention = st2110::AudioChannelOrderConvention::Smpte2110,
                .raw_value = "SMPTE2110.(51,ST)"
        };

        const auto parsed = st2110::parse_audio_channel_order_signaling(channel_order);

        assert(parsed.has_value());
        assert(parsed->convention == st2110::AudioChannelOrderConvention::Smpte2110);
        assert(parsed->raw_value == "SMPTE2110.(51,ST)");
        assert(parsed->declared_channel_count == 8);

        assert(parsed->groups.size() == 2);

        assert(parsed->groups[0].kind == st2110::AudioChannelGroupKind::FiveOne);
        assert(parsed->groups[0].symbol == "51");
        assert(parsed->groups[0].channel_count == 6);

        assert(parsed->groups[1].kind == st2110::AudioChannelGroupKind::Stereo);
        assert(parsed->groups[1].symbol == "ST");
        assert(parsed->groups[1].channel_count == 2);
    }

    void parses_mono_and_undefined_groups() {
        const st2110::AudioChannelOrderSignaling channel_order{
                .convention = st2110::AudioChannelOrderConvention::Smpte2110,
                .raw_value = "SMPTE2110.(M,M,U02)"
        };

        const auto parsed = st2110::parse_audio_channel_order_signaling(channel_order);

        assert(parsed.has_value());
        assert(parsed->declared_channel_count == 4);
        assert(parsed->groups.size() == 3);

        assert(parsed->groups[0].kind == st2110::AudioChannelGroupKind::Mono);
        assert(parsed->groups[0].symbol == "M");
        assert(parsed->groups[0].channel_count == 1);

        assert(parsed->groups[1].kind == st2110::AudioChannelGroupKind::Mono);
        assert(parsed->groups[1].symbol == "M");
        assert(parsed->groups[1].channel_count == 1);

        assert(parsed->groups[2].kind == st2110::AudioChannelGroupKind::Undefined);
        assert(parsed->groups[2].symbol == "U02");
        assert(parsed->groups[2].channel_count == 2);
    }

    void rejects_malformed_smpte2110_channel_order_values() {
        const auto assert_invalid = [](std::string raw_value) {
            const st2110::AudioChannelOrderSignaling channel_order{
                    .convention = st2110::AudioChannelOrderConvention::Smpte2110,
                    .raw_value = std::move(raw_value)
            };

            const auto parsed = st2110::parse_audio_channel_order_signaling(channel_order);

            assert(!parsed.has_value());
            assert(parsed.error() == st2110::Error::InvalidValue);
        };

        assert_invalid("SMPTE2110.");
        assert_invalid("SMPTE2110.()");
        assert_invalid("SMPTE2110.(ST,)");
        assert_invalid("SMPTE2110.(,ST)");
        assert_invalid("SMPTE2110.(U2)");
        assert_invalid("SMPTE2110.(U00)");
        assert_invalid("SMPTE2110.ST");
        assert_invalid("OTHER.(ST)");
    }

    void validates_channel_order_against_channel_count() {
        {
            const auto signaling = make_level_a_stream(8, "SMPTE2110.(51,ST)");
            assert(st2110::validate_audio_stream_signaling(signaling) == st2110::Error::Ok);
        }

        {
            const auto signaling = make_level_a_stream(6, "SMPTE2110.(51,ST)");
            assert(st2110::validate_audio_stream_signaling(signaling) == st2110::Error::InvalidValue);
        }

        {
            const auto signaling = make_level_a_stream(4, "SMPTE2110.(ST)");
            assert(st2110::validate_audio_stream_signaling(signaling) == st2110::Error::Ok);
        }
    }

    void absent_channel_order_becomes_effective_undefined_group() {
        const auto signaling = make_level_a_stream(4);

        const auto effective =
                st2110::effective_audio_channel_order_from_audio_stream_signaling(signaling);

        assert(effective.has_value());
        assert(effective->convention == st2110::AudioChannelOrderConvention::Unspecified);
        assert(effective->raw_value.empty());
        assert(effective->declared_channel_count == 4);
        assert(effective->groups.size() == 1);

        assert(effective->groups[0].kind == st2110::AudioChannelGroupKind::Undefined);
        assert(effective->groups[0].symbol == "U04");
        assert(effective->groups[0].channel_count == 4);
    }

    void partial_channel_order_appends_effective_undefined_remainder() {
        const auto signaling = make_level_a_stream(4, "SMPTE2110.(ST)");

        const auto effective =
                st2110::effective_audio_channel_order_from_audio_stream_signaling(signaling);

        assert(effective.has_value());
        assert(effective->convention == st2110::AudioChannelOrderConvention::Smpte2110);
        assert(effective->raw_value == "SMPTE2110.(ST)");
        assert(effective->declared_channel_count == 4);
        assert(effective->groups.size() == 2);

        assert(effective->groups[0].kind == st2110::AudioChannelGroupKind::Stereo);
        assert(effective->groups[0].symbol == "ST");
        assert(effective->groups[0].channel_count == 2);

        assert(effective->groups[1].kind == st2110::AudioChannelGroupKind::Undefined);
        assert(effective->groups[1].symbol == "U02");
        assert(effective->groups[1].channel_count == 2);
    }

    void non_smpte2110_other_convention_is_preserved_but_not_mapped_to_layout() {
        const st2110::AudioChannelOrderSignaling channel_order{
                .convention = st2110::AudioChannelOrderConvention::Other,
                .raw_value = "CUSTOM.(A,B)"
        };

        const auto parsed = st2110::parse_audio_channel_order_signaling(channel_order);

        assert(parsed.has_value());
        assert(parsed->convention == st2110::AudioChannelOrderConvention::Other);
        assert(parsed->raw_value == "CUSTOM.(A,B)");
        assert(parsed->declared_channel_count == 0);
        assert(parsed->groups.empty());

        assert(st2110::validate_audio_channel_order_against_channel_count(channel_order, 2) ==
               st2110::Error::Ok);
    }
}

int main() {
    parses_smpte2110_surround_and_stereo_groups();
    parses_mono_and_undefined_groups();
    rejects_malformed_smpte2110_channel_order_values();
    validates_channel_order_against_channel_count();
    absent_channel_order_becomes_effective_undefined_group();
    partial_channel_order_appends_effective_undefined_remainder();
    non_smpte2110_other_convention_is_preserved_but_not_mapped_to_layout();

    return 0;
}
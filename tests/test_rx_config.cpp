#include <cassert>
#include <array>
#include <cstdint>
#include <expected>
#include <string>
#include <type_traits>
#include <utility>

#include <st2110/config_validation.hpp>
#include <st2110/error.hpp>
#include <st2110/pixel_format.hpp>
#include <st2110/rx_config.hpp>

namespace {
    st2110::RxAudioConfig make_valid_level_a_audio_config(std::uint16_t channels = 2) {
        st2110::RxAudioConfig cfg{};
        cfg.sampling_rate_hz = 48000;
        cfg.packet_time_us = 1000;
        cfg.samples_per_packet = 48;
        cfg.channel_count = channels;
        cfg.udp_port = 30000;
        cfg.payload_type = 111;
        cfg.dest_ip = "239.1.1.2";
        cfg.local_ip = "0.0.0.0";
        cfg.format = st2110::AudioSampleFormat::LinearPcm;
        return cfg;
    }
}

int main() {
    static_assert(std::is_enum_v<st2110::AudioSampleFormat>);
    static_assert(!std::is_convertible_v<st2110::AudioSampleFormat, int>);

    static_assert(std::is_same_v<
                  decltype(st2110::validate_rx_audio_config(std::declval<const st2110::RxAudioConfig&>())),
                  st2110::Error>);

    static_assert(std::is_same_v<
                  decltype(st2110::validate_rx_audio_config_against_runtime_support(
                          std::declval<const st2110::RxAudioConfig&>(),
                          std::declval<const st2110::AudioRuntimeSupportPolicy&>())),
                  st2110::Error>);

    static_assert(std::is_same_v<
                  decltype(st2110::config_validation::audio_samples_per_packet_from_rate_and_packet_time(
                          std::uint32_t{},
                          std::uint32_t{})),
                  std::expected<std::uint32_t, st2110::Error>>);

    {
        auto samples = st2110::config_validation::audio_samples_per_packet_from_rate_and_packet_time(48000, 1000);
        assert(samples.has_value());
        assert(*samples == 48);

        auto short_packet_time_samples =
                st2110::config_validation::audio_samples_per_packet_from_rate_and_packet_time(48000, 125);
        assert(short_packet_time_samples.has_value());
        assert(*short_packet_time_samples == 6);

        auto zero_rate = st2110::config_validation::audio_samples_per_packet_from_rate_and_packet_time(0, 1000);
        assert(!zero_rate.has_value());
        assert(zero_rate.error() == st2110::Error::InvalidValue);

        auto zero_packet_time =
                st2110::config_validation::audio_samples_per_packet_from_rate_and_packet_time(48000, 0);
        assert(!zero_packet_time.has_value());
        assert(zero_packet_time.error() == st2110::Error::InvalidValue);

        auto non_integral =
                st2110::config_validation::audio_samples_per_packet_from_rate_and_packet_time(44100, 1000);
        assert(!non_integral.has_value());
        assert(non_integral.error() == st2110::Error::InvalidValue);
    }

    {
        st2110::RxVideoConfig cfg{};
        assert(!cfg.is_valid());

        cfg.width = 1280;
        cfg.height = 720;
        cfg.fps_num = 30;
        cfg.fps_den = 1;
        cfg.udp_port = 20000;
        cfg.payload_type = 112;
        cfg.dest_ip = "239.1.1.1";
        cfg.local_ip = "0.0.0.0";
        cfg.format = st2110::PixelFormat::UYVY;

        assert(cfg.is_valid());

        cfg.udp_port = 0;
        assert(!cfg.is_valid());
    }

    {
        st2110::RxAudioConfig cfg{};
        assert(!cfg.is_valid());

        cfg = make_valid_level_a_audio_config(2);
        assert(st2110::validate_rx_audio_config(cfg) == st2110::Error::Ok);
        assert(cfg.is_valid());

        st2110::RxAudioConfig min_channels = make_valid_level_a_audio_config(1);
        assert(min_channels.is_valid());

        st2110::RxAudioConfig max_channels = make_valid_level_a_audio_config(8);
        assert(max_channels.is_valid());

        st2110::RxAudioConfig zero_channels = make_valid_level_a_audio_config(0);
        assert(!zero_channels.is_valid());

        st2110::RxAudioConfig too_many_channels = make_valid_level_a_audio_config(9);
        assert(!too_many_channels.is_valid());

        st2110::RxAudioConfig wrong_sampling_rate = make_valid_level_a_audio_config();
        wrong_sampling_rate.sampling_rate_hz = 96000;
        assert(!wrong_sampling_rate.is_valid());

        st2110::RxAudioConfig wrong_packet_time = make_valid_level_a_audio_config();
        wrong_packet_time.packet_time_us = 125;
        assert(!wrong_packet_time.is_valid());

        st2110::RxAudioConfig wrong_samples_per_packet = make_valid_level_a_audio_config();
        wrong_samples_per_packet.samples_per_packet = 47;
        assert(!wrong_samples_per_packet.is_valid());

        st2110::RxAudioConfig zero_udp_port = make_valid_level_a_audio_config();
        zero_udp_port.udp_port = 0;
        assert(!zero_udp_port.is_valid());

        st2110::RxAudioConfig low_payload_type = make_valid_level_a_audio_config();
        low_payload_type.payload_type = 95;
        assert(!low_payload_type.is_valid());

        st2110::RxAudioConfig high_payload_type = make_valid_level_a_audio_config();
        high_payload_type.payload_type = 128;
        assert(!high_payload_type.is_valid());

        st2110::RxAudioConfig empty_dest_ip = make_valid_level_a_audio_config();
        empty_dest_ip.dest_ip = "";
        assert(!empty_dest_ip.is_valid());

        st2110::RxAudioConfig empty_local_ip = make_valid_level_a_audio_config();
        empty_local_ip.local_ip = "";
        assert(empty_local_ip.is_valid());

        st2110::RxAudioConfig invalid_format = make_valid_level_a_audio_config();
        invalid_format.format = static_cast<st2110::AudioSampleFormat>(255);
        assert(!invalid_format.is_valid());
    }

    {
        static constexpr std::array custom_formats{
                st2110::AudioSampleFormat::LinearPcm
        };

        static constexpr std::array custom_ranges{
                st2110::AudioConformanceRange{
                        st2110::AudioConformanceLevel::LevelAX,
                        48000,
                        125,
                        1,
                        8
                }
        };

        const st2110::AudioRuntimeSupportPolicy custom_support{
                std::span<const st2110::AudioSampleFormat>{custom_formats},
                std::span<const st2110::AudioConformanceRange>{custom_ranges}
        };

        st2110::RxAudioConfig short_packet_time_cfg = make_valid_level_a_audio_config(2);
        short_packet_time_cfg.packet_time_us = 125;
        short_packet_time_cfg.samples_per_packet = 6;

        // Default MVP runtime support is still Level A-oriented and rejects this.
        assert(!short_packet_time_cfg.is_valid());

        // The generic runtime-support boundary accepts it when the explicit support policy allows it.
        assert(st2110::validate_rx_audio_config_against_runtime_support(
                short_packet_time_cfg,
                custom_support) == st2110::Error::Ok);

        st2110::RxAudioConfig wrong_derived_samples = short_packet_time_cfg;
        wrong_derived_samples.samples_per_packet = 48;
        assert(st2110::validate_rx_audio_config_against_runtime_support(
                wrong_derived_samples,
                custom_support) == st2110::Error::InvalidValue);
    }

    return 0;
}
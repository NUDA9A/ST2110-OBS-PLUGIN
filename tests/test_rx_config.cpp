#include <array>
#include <cassert>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>

#include <st2110/delivery/video/pixel_format.hpp>
#include <st2110/foundation/derived_values.hpp>
#include <st2110/foundation/error.hpp>
#include <st2110/model/video/video_packing_mode.hpp>
#include <st2110/model/video/video_scan_mode.hpp>
#include <st2110/rx_config.hpp>
#include <st2110/video_receive_capability.hpp>

namespace {
st2110::RxVideoConfig make_valid_video_config() {
    st2110::RxVideoConfig cfg{};
    cfg.width = 1280;
    cfg.height = 720;
    cfg.fps_num = 30;
    cfg.fps_den = 1;
    cfg.udp_port = 20000;
    cfg.payload_type = 112;
    cfg.dest_ip = "239.1.1.1";
    cfg.local_ip = "0.0.0.0";
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.scan_mode = st2110::VideoScanMode::Progressive;
    cfg.packing_mode = st2110::VideoPackingMode::Gpm;
    return cfg;
}

st2110::VideoMediaDescription make_ycbcr422_media(std::uint8_t bits = 8) {
    st2110::VideoMediaDescription media{};
    media.sampling.known = st2110::VideoSampling::Known::YCbCr422;
    media.width = 1280;
    media.height = 720;
    media.fps_num = 30;
    media.fps_den = 1;
    media.depth.bits = bits;
    media.depth.floating_point = false;
    media.colorimetry.known = st2110::VideoColorimetry::Known::Bt709;
    media.signal_standard = st2110::VideoSignalStandard{.known = st2110::VideoSignalStandard::Known::St2110_20_2022};
    media.range = st2110::VideoRange{.known = st2110::VideoRange::Known::Narrow};
    media.pixel_aspect_ratio = st2110::VideoPixelAspectRatio{.width = 1, .height = 1};
    return media;
}

st2110::VideoReceiveCapability make_ycbcr422_receive_capability(
    std::uint8_t bits = 8,
    st2110::VideoTransportPayloadFormat transport_format = st2110::VideoTransportPayloadFormat::Rfc4175Ycbcr422_8Bit,
    st2110::VideoFrameHandoffFormat handoff_format = st2110::VideoFrameHandoffFormat::Uyvy) {
    st2110::VideoReceiveCapability capability{};
    capability.media = make_ycbcr422_media(bits);
    capability.scan_mode = st2110::VideoScanMode::Progressive;
    capability.packing_mode = st2110::VideoPackingMode::Gpm;
    capability.transport_format = transport_format;
    capability.handoff_format = handoff_format;
    capability.rtp_clock = st2110::VideoReceiveRtpClock{.rtp_clock_rate = 90000};
    capability.topology =
        st2110::VideoReceiveTopology{.kind = st2110::VideoReceiveTopologyKind::SingleStream, .stream_count = 1};
    return capability;
}

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
    cfg.pcm_bit_depth = st2110::AudioPcmBitDepth::Bits24;
    return cfg;
}
} // namespace

int main() {
    static_assert(std::is_enum_v<st2110::AudioSampleFormat>);
    static_assert(!std::is_convertible_v<st2110::AudioSampleFormat, int>);

    static_assert(
        std::is_same_v<decltype(std::declval<st2110::RxVideoConfig &>().packing_mode), st2110::VideoPackingMode>);

    static_assert(std::is_same_v<decltype(std::declval<st2110::RxVideoConfig &>().receive_capability),
                                 std::optional<st2110::VideoReceiveCapability>>);

    static_assert(
        std::is_same_v<decltype(st2110::validate_rx_video_config(std::declval<const st2110::RxVideoConfig &>())),
                       st2110::Error>);

    static_assert(std::is_same_v<decltype(st2110::validate_rx_video_config_against_runtime_support(
                                     std::declval<const st2110::RxVideoConfig &>(),
                                     std::declval<const st2110::VideoRuntimeSupportPolicy &>())),
                                 st2110::Error>);

    static_assert(std::is_same_v<decltype(st2110::rx_video_config_effective_receive_capability(
                                     std::declval<const st2110::RxVideoConfig &>())),
                                 std::expected<st2110::VideoReceiveCapability, st2110::Error>>);

    static_assert(
        std::is_same_v<decltype(st2110::validate_rx_audio_config(std::declval<const st2110::RxAudioConfig &>())),
                       st2110::Error>);

    static_assert(std::is_same_v<decltype(st2110::validate_rx_audio_config_against_runtime_support(
                                     std::declval<const st2110::RxAudioConfig &>(),
                                     std::declval<const st2110::AudioRuntimeSupportPolicy &>())),
                                 st2110::Error>);

    static_assert(std::is_same_v<decltype(st2110::audio_samples_per_packet_from_rate_and_packet_time(std::uint32_t{},
                                                                                                     std::uint32_t{})),
                                 std::expected<std::uint32_t, st2110::Error>>);

    {
        auto samples = st2110::audio_samples_per_packet_from_rate_and_packet_time(48000, 1000);
        assert(samples.has_value());
        assert(*samples == 48);

        auto short_packet_time_samples = st2110::audio_samples_per_packet_from_rate_and_packet_time(48000, 125);
        assert(short_packet_time_samples.has_value());
        assert(*short_packet_time_samples == 6);

        auto zero_rate = st2110::audio_samples_per_packet_from_rate_and_packet_time(0, 1000);
        assert(!zero_rate.has_value());
        assert(zero_rate.error() == st2110::Error::InvalidValue);

        auto zero_packet_time = st2110::audio_samples_per_packet_from_rate_and_packet_time(48000, 0);
        assert(!zero_packet_time.has_value());
        assert(zero_packet_time.error() == st2110::Error::InvalidValue);

        auto non_integral = st2110::audio_samples_per_packet_from_rate_and_packet_time(44100, 1000);
        assert(!non_integral.has_value());
        assert(non_integral.error() == st2110::Error::InvalidValue);
    }

    {
        st2110::RxVideoConfig cfg{};
        assert(!cfg.is_valid());

        cfg = make_valid_video_config();

        assert(cfg.is_valid());
        assert(st2110::validate_rx_video_config(cfg) == st2110::Error::Ok);
        assert(cfg.packing_mode == st2110::VideoPackingMode::Gpm);

        auto effective_capability = st2110::rx_video_config_effective_receive_capability(cfg);
        assert(effective_capability.has_value());
        assert(effective_capability->media.width == cfg.width);
        assert(effective_capability->media.height == cfg.height);
        assert(effective_capability->media.fps_num == cfg.fps_num);
        assert(effective_capability->media.fps_den == cfg.fps_den);
        assert(effective_capability->scan_mode == cfg.scan_mode);
        assert(effective_capability->packing_mode == cfg.packing_mode);
        assert(effective_capability->transport_format == st2110::VideoTransportPayloadFormat::Rfc4175Ycbcr422_8Bit);
        assert(effective_capability->handoff_format == st2110::VideoFrameHandoffFormat::Uyvy);
        assert(effective_capability->rtp_clock.rtp_clock_rate == 90000);
    }

    {
        st2110::RxVideoConfig default_packing = make_valid_video_config();
        default_packing.packing_mode = st2110::VideoPackingMode::Gpm;
        assert(default_packing.is_valid());
        assert(st2110::validate_rx_video_config_against_runtime_support(
                   default_packing, st2110::default_video_rx_runtime_support_policy()) == st2110::Error::Ok);

        st2110::RxVideoConfig recognized_bpm = make_valid_video_config();
        recognized_bpm.packing_mode = st2110::VideoPackingMode::Bpm;
        assert(recognized_bpm.is_valid());
        assert(st2110::validate_rx_video_config(recognized_bpm) == st2110::Error::Ok);

        st2110::VideoRuntimeSupportPolicy support_without_packing_limit =
            st2110::default_video_rx_runtime_support_policy();
        support_without_packing_limit.require_runtime_packing_mode_support = false;
        assert(st2110::validate_rx_video_config_against_runtime_support(
                   recognized_bpm, support_without_packing_limit) == st2110::Error::Ok);

        st2110::RxVideoConfig recognized_gpm_single_line = make_valid_video_config();
        recognized_gpm_single_line.packing_mode = st2110::VideoPackingMode::GpmSingleLine;
        assert(recognized_gpm_single_line.is_valid());
        assert(st2110::validate_rx_video_config(recognized_gpm_single_line) == st2110::Error::Ok);
    }

    {
        st2110::RxVideoConfig interlaced = make_valid_video_config();
        interlaced.scan_mode = st2110::VideoScanMode::Interlaced;
        interlaced.receive_capability = make_ycbcr422_receive_capability();
        interlaced.receive_capability->scan_mode = st2110::VideoScanMode::Interlaced;
        assert(interlaced.is_valid());
        assert(st2110::validate_rx_video_config(interlaced) == st2110::Error::Ok);

        st2110::RxVideoConfig psf = make_valid_video_config();
        psf.scan_mode = st2110::VideoScanMode::PsF;
        psf.receive_capability = make_ycbcr422_receive_capability();
        psf.receive_capability->scan_mode = st2110::VideoScanMode::PsF;
        assert(psf.is_valid());
        assert(st2110::validate_rx_video_config(psf) == st2110::Error::Ok);
    }

    {
        st2110::RxVideoConfig ten_bit_ycbcr422 = make_valid_video_config();
        ten_bit_ycbcr422.receive_capability =
            make_ycbcr422_receive_capability(10, st2110::VideoTransportPayloadFormat::Rfc4175Ycbcr422_10Bit,
                                             st2110::VideoFrameHandoffFormat::Yuv422Planar10Le);

        assert(ten_bit_ycbcr422.is_valid());
        assert(st2110::validate_rx_video_config(ten_bit_ycbcr422) == st2110::Error::Ok);

        assert(st2110::validate_rx_video_config_against_runtime_support(
                   ten_bit_ycbcr422, st2110::default_video_rx_runtime_support_policy()) == st2110::Error::Unsupported);

        st2110::RxVideoConfig generic_rfc4175 = make_valid_video_config();
        generic_rfc4175.receive_capability = make_ycbcr422_receive_capability(
            10, st2110::VideoTransportPayloadFormat::Rfc4175, st2110::VideoFrameHandoffFormat::Yuv422Planar10Le);

        assert(generic_rfc4175.is_valid());
        assert(st2110::validate_rx_video_config(generic_rfc4175) == st2110::Error::Ok);

        st2110::RxVideoConfig unsupported_generic_rfc4175 = make_valid_video_config();
        unsupported_generic_rfc4175.receive_capability = make_ycbcr422_receive_capability(
            8, st2110::VideoTransportPayloadFormat::Rfc4175, st2110::VideoFrameHandoffFormat::Uyvy);
        unsupported_generic_rfc4175.receive_capability->media.sampling.known = st2110::VideoSampling::Known::Other;
        unsupported_generic_rfc4175.receive_capability->media.sampling.raw_token = "vendor-private-sampling";
        assert(!unsupported_generic_rfc4175.is_valid());
        assert(st2110::validate_rx_video_config(unsupported_generic_rfc4175) == st2110::Error::Unsupported);
    }

    {
        st2110::RxVideoConfig redundant = make_valid_video_config();
        redundant.receive_capability = make_ycbcr422_receive_capability();
        redundant.receive_capability->topology = st2110::VideoReceiveTopology{
            .kind = st2110::VideoReceiveTopologyKind::RedundantStreams,
            .stream_count = 2,
            .primary_mid = std::string{"primary"},
            .redundant_mid = std::string{"redundant"},
        };

        assert(redundant.is_valid());
        assert(st2110::validate_rx_video_config(redundant) == st2110::Error::Ok);

        st2110::RxVideoConfig invalid_redundant = make_valid_video_config();
        invalid_redundant.receive_capability = make_ycbcr422_receive_capability();
        invalid_redundant.receive_capability->topology = st2110::VideoReceiveTopology{
            .kind = st2110::VideoReceiveTopologyKind::RedundantStreams,
            .stream_count = 1,
        };

        assert(!invalid_redundant.is_valid());
        assert(st2110::validate_rx_video_config(invalid_redundant) == st2110::Error::InvalidValue);
    }

    {
        st2110::RxVideoConfig invalid_rtp_clock = make_valid_video_config();
        invalid_rtp_clock.receive_capability = make_ycbcr422_receive_capability();
        invalid_rtp_clock.receive_capability->rtp_clock.rtp_clock_rate = 0;
        assert(!invalid_rtp_clock.is_valid());
        assert(st2110::validate_rx_video_config(invalid_rtp_clock) == st2110::Error::InvalidValue);

        st2110::RxVideoConfig non_90khz_rtp_clock = make_valid_video_config();
        non_90khz_rtp_clock.receive_capability = make_ycbcr422_receive_capability();
        non_90khz_rtp_clock.receive_capability->rtp_clock.rtp_clock_rate = 48000;
        assert(non_90khz_rtp_clock.is_valid());
        assert(st2110::validate_rx_video_config(non_90khz_rtp_clock) == st2110::Error::Ok);
    }

    {
        st2110::RxVideoConfig mismatched_explicit_capability = make_valid_video_config();
        mismatched_explicit_capability.receive_capability = make_ycbcr422_receive_capability();
        mismatched_explicit_capability.receive_capability->media.width = 1920;
        assert(!mismatched_explicit_capability.is_valid());
        assert(st2110::validate_rx_video_config(mismatched_explicit_capability) == st2110::Error::InvalidValue);

        st2110::RxVideoConfig zero_udp_port = make_valid_video_config();
        zero_udp_port.udp_port = 0;
        assert(!zero_udp_port.is_valid());
        assert(st2110::validate_rx_video_config(zero_udp_port) == st2110::Error::InvalidValue);
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

        st2110::RxAudioConfig valid_l16 = make_valid_level_a_audio_config(2);
        valid_l16.pcm_bit_depth = st2110::AudioPcmBitDepth::Bits16;
        assert(valid_l16.is_valid());

        st2110::RxAudioConfig valid_l24 = make_valid_level_a_audio_config(2);
        valid_l24.pcm_bit_depth = st2110::AudioPcmBitDepth::Bits24;
        assert(valid_l24.is_valid());

        st2110::RxAudioConfig invalid_bit_depth = make_valid_level_a_audio_config(2);
        invalid_bit_depth.pcm_bit_depth = static_cast<st2110::AudioPcmBitDepth>(255);
        assert(!invalid_bit_depth.is_valid());

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
        static constexpr std::array custom_formats{st2110::AudioSampleFormat::LinearPcm};

        static constexpr std::array custom_ranges{
            st2110::AudioConformanceRange{st2110::AudioConformanceLevel::LevelAX, 48000, 125, 1, 8}};

        const st2110::AudioRuntimeSupportPolicy custom_support{
            std::span<const st2110::AudioSampleFormat>{custom_formats},
            std::span<const st2110::AudioConformanceRange>{custom_ranges}};

        st2110::RxAudioConfig short_packet_time_cfg = make_valid_level_a_audio_config(2);
        short_packet_time_cfg.packet_time_us = 125;
        short_packet_time_cfg.samples_per_packet = 6;

        // Default MVP runtime support is still Level A-oriented and rejects this.
        assert(!short_packet_time_cfg.is_valid());

        // The generic runtime-support boundary accepts it when the explicit support policy allows it.
        assert(st2110::validate_rx_audio_config_against_runtime_support(short_packet_time_cfg, custom_support) ==
               st2110::Error::Ok);

        st2110::RxAudioConfig wrong_derived_samples = short_packet_time_cfg;
        wrong_derived_samples.samples_per_packet = 48;
        assert(st2110::validate_rx_audio_config_against_runtime_support(wrong_derived_samples, custom_support) ==
               st2110::Error::InvalidValue);
    }

    return 0;
}
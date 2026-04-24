#include "st2110/video_receiver_timing_signaling.hpp"
#include "st2110/video_signaling.hpp"

#include <cassert>
#include <cstdint>
#include <expected>
#include <string>
#include <type_traits>
#include <utility>

namespace {

    using namespace st2110;

    using GenericBootstrapResult = decltype(
            video_receiver_bootstrap_config_from_video_stream_signaling(
            std::declval<const VideoStreamSignaling&>(),
            std::declval<uint16_t>(),
            std::declval<uint8_t>(),
            std::declval<std::string>(),
            std::declval<std::string>(),
            std::declval<PartialFramePolicy>()));

    using TimingBootstrapResult = decltype(
            video_receiver_bootstrap_config_from_video_stream_signaling(
            std::declval<const VideoStreamSignaling&>(),
            std::declval<const VideoReceiverTimingConfig&>(),
            std::declval<uint16_t>(),
            std::declval<uint8_t>(),
            std::declval<std::string>(),
            std::declval<std::string>(),
            std::declval<PartialFramePolicy>()));

    static_assert(std::is_same_v<GenericBootstrapResult, TimingBootstrapResult>);
    static_assert(std::is_same_v<GenericBootstrapResult, std::expected<VideoReceiverBootstrapConfig, Error>>);

    VideoStreamSignaling make_valid_signaling() {
        VideoStreamSignaling signaling{};

        signaling.media.sampling.known = VideoSampling::Known::YCbCr422;
        signaling.media.width = 1920;
        signaling.media.height = 1080;
        signaling.media.fps_num = 25;
        signaling.media.fps_den = 1;
        signaling.media.depth.bits = 8;
        signaling.media.depth.floating_point = false;
        signaling.media.colorimetry.known = VideoColorimetry::Known::Bt709;

        signaling.scan_mode = VideoScanMode::Progressive;
        signaling.packing_mode = VideoPackingMode::Gpm;
        signaling.max_udp_datagram_bytes = 1200;

        signaling.media_clock_mode = MediaClockMode::Direct;
        signaling.timestamp_mode = TimestampMode::New;

        signaling.reference_clock.kind = ReferenceClockKind::Ptp;
        signaling.reference_clock.ptp = PtpReferenceClock{};
        signaling.reference_clock.local_mac.reset();
        signaling.reference_clock.raw_token.reset();

        signaling.ts_delay_sender_ticks = 0;

        signaling.sender_type = VideoSenderType::Narrow;
        signaling.troff_us.reset();
        signaling.cmax.reset();

        return signaling;
    }

    VideoReceiverTimingConfig make_permissive_timing_config() {
        VideoReceiverTimingConfig cfg{};
        cfg.capability.supports_type_n = true;
        cfg.capability.supports_type_nl = true;
        cfg.capability.supports_type_w = true;

        cfg.requirements.require_reference_clock = true;
        cfg.requirements.require_media_clock = true;
        cfg.requirements.require_timestamp_mode = true;
        cfg.requirements.consume_ts_delay = true;
        cfg.requirements.consume_sender_troff = true;
        cfg.requirements.consume_sender_cmax = true;

        return cfg;
    }

    void assert_same_runtime_projection_except_timing(
            const VideoReceiverBootstrapConfig& generic_cfg,
            const VideoReceiverBootstrapConfig& timing_cfg) {
        assert(generic_cfg.packet_parse_policy.max_udp_datagram_bytes ==
               timing_cfg.packet_parse_policy.max_udp_datagram_bytes);

        assert(generic_cfg.rx_config.width == timing_cfg.rx_config.width);
        assert(generic_cfg.rx_config.height == timing_cfg.rx_config.height);
        assert(generic_cfg.rx_config.fps_num == timing_cfg.rx_config.fps_num);
        assert(generic_cfg.rx_config.fps_den == timing_cfg.rx_config.fps_den);
        assert(generic_cfg.rx_config.udp_port == timing_cfg.rx_config.udp_port);
        assert(generic_cfg.rx_config.payload_type == timing_cfg.rx_config.payload_type);
        assert(generic_cfg.rx_config.local_ip == timing_cfg.rx_config.local_ip);
        assert(generic_cfg.rx_config.dest_ip == timing_cfg.rx_config.dest_ip);
        assert(generic_cfg.rx_config.format == timing_cfg.rx_config.format);
        assert(generic_cfg.rx_config.scan_mode == timing_cfg.rx_config.scan_mode);

        assert(generic_cfg.receive_pipeline_config.depacketizer.width ==
               timing_cfg.receive_pipeline_config.depacketizer.width);
        assert(generic_cfg.receive_pipeline_config.depacketizer.height ==
               timing_cfg.receive_pipeline_config.depacketizer.height);
        assert(generic_cfg.receive_pipeline_config.depacketizer.format ==
               timing_cfg.receive_pipeline_config.depacketizer.format);
        assert(generic_cfg.receive_pipeline_config.depacketizer.partial_frame_policy ==
               timing_cfg.receive_pipeline_config.depacketizer.partial_frame_policy);
        assert(generic_cfg.receive_pipeline_config.depacketizer.scan_mode ==
               timing_cfg.receive_pipeline_config.depacketizer.scan_mode);
        assert(generic_cfg.receive_pipeline_config.depacketizer.packing_mode ==
               timing_cfg.receive_pipeline_config.depacketizer.packing_mode);

        assert(generic_cfg.receive_pipeline_config.reconstructor.format ==
               timing_cfg.receive_pipeline_config.reconstructor.format);
        assert(generic_cfg.receive_pipeline_config.reconstructor.scan_mode ==
               timing_cfg.receive_pipeline_config.reconstructor.scan_mode);
    }

    void assert_same_timing_config(
            const VideoReceiverTimingConfig& expected,
            const VideoReceiverTimingConfig& actual) {
        assert(expected.capability.supports_type_n == actual.capability.supports_type_n);
        assert(expected.capability.supports_type_nl == actual.capability.supports_type_nl);
        assert(expected.capability.supports_type_w == actual.capability.supports_type_w);

        assert(expected.requirements.require_reference_clock == actual.requirements.require_reference_clock);
        assert(expected.requirements.require_media_clock == actual.requirements.require_media_clock);
        assert(expected.requirements.require_timestamp_mode == actual.requirements.require_timestamp_mode);
        assert(expected.requirements.consume_ts_delay == actual.requirements.consume_ts_delay);
        assert(expected.requirements.consume_sender_troff == actual.requirements.consume_sender_troff);
        assert(expected.requirements.consume_sender_cmax == actual.requirements.consume_sender_cmax);
    }

    void test_timing_bootstrap_reuses_generic_runtime_projection() {
        const auto signaling = make_valid_signaling();
        const auto timing = make_permissive_timing_config();

        const auto generic_result =
                video_receiver_bootstrap_config_from_video_stream_signaling(
                        signaling,
                        5004,
                        96,
                        "127.0.0.1",
                        "239.10.10.10",
                        PartialFramePolicy::EmitWithFlag);

        assert(generic_result.has_value());

        const auto timing_result =
                video_receiver_bootstrap_config_from_video_stream_signaling(
                        signaling,
                        timing,
                        5004,
                        96,
                        "127.0.0.1",
                        "239.10.10.10",
                        PartialFramePolicy::EmitWithFlag);

        assert(timing_result.has_value());

        assert_same_runtime_projection_except_timing(*generic_result, *timing_result);
        assert_same_timing_config(timing, timing_result->timing_config);
    }

    void test_sender_type_rejection_is_localized_in_timing_boundary() {
        auto signaling = make_valid_signaling();
        signaling.sender_type = VideoSenderType::Wide;
        signaling.cmax = 7;

        VideoReceiverTimingConfig timing = make_permissive_timing_config();
        timing.capability.supports_type_w = false;

        const auto generic_result =
                video_receiver_bootstrap_config_from_video_stream_signaling(
                        signaling,
                        5004,
                        96,
                        "127.0.0.1",
                        "239.10.10.10",
                        PartialFramePolicy::EmitWithFlag);

        assert(generic_result.has_value());

        const auto timing_result =
                video_receiver_bootstrap_config_from_video_stream_signaling(
                        signaling,
                        timing,
                        5004,
                        96,
                        "127.0.0.1",
                        "239.10.10.10",
                        PartialFramePolicy::EmitWithFlag);

        assert(!timing_result.has_value());
        assert(timing_result.error() == Error::Unsupported);
    }

    void test_ts_delay_rejection_is_localized_in_timing_boundary() {
        auto signaling = make_valid_signaling();
        signaling.ts_delay_sender_ticks = 42;

        VideoReceiverTimingConfig timing = make_permissive_timing_config();
        timing.requirements.consume_ts_delay = false;

        const auto generic_result =
                video_receiver_bootstrap_config_from_video_stream_signaling(
                        signaling,
                        5004,
                        96,
                        "127.0.0.1",
                        "239.10.10.10",
                        PartialFramePolicy::Drop);

        assert(generic_result.has_value());

        const auto timing_result =
                video_receiver_bootstrap_config_from_video_stream_signaling(
                        signaling,
                        timing,
                        5004,
                        96,
                        "127.0.0.1",
                        "239.10.10.10",
                        PartialFramePolicy::Drop);

        assert(!timing_result.has_value());
        assert(timing_result.error() == Error::Unsupported);
    }

} // namespace

int main() {
    test_timing_bootstrap_reuses_generic_runtime_projection();
    test_sender_type_rejection_is_localized_in_timing_boundary();
    test_ts_delay_rejection_is_localized_in_timing_boundary();
    return 0;
}
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

#include <st2110/audio_channel_order.hpp>
#include <st2110/audio_receiver_bootstrap.hpp>
#include <st2110/backend.hpp>
#include <st2110/packet_parse.hpp>
#include <st2110/receive_reorder_tolerance_policy.hpp>
#include <st2110/signaling_structs.hpp>
#include <st2110/socket_runtime.hpp>
#include <st2110/socket_rx_audio_backend.hpp>
#include <st2110/socket_rx_single_media_backend_base.hpp>
#include <st2110/socket_rx_video_backend.hpp>

static_assert(std::is_base_of_v<st2110::IRxBackend, st2110::SocketRxSingleMediaBackendBase>);
static_assert(!std::is_base_of_v<st2110::ISocketRxVideoBackend, st2110::SocketRxSingleMediaBackendBase>);
static_assert(!std::is_base_of_v<st2110::ISocketRxAudioBackend, st2110::SocketRxSingleMediaBackendBase>);

namespace {
class ProbeSingleMediaBackend final : public st2110::SocketRxSingleMediaBackendBase {
  public:
    ProbeSingleMediaBackend(st2110::RxMediaKind media_kind, st2110::RxBackendCapabilities capabilities)
        : st2110::SocketRxSingleMediaBackendBase(media_kind, capabilities,
                                                 std::unique_ptr<st2110::ISocketRxPortFactory>{}) {}

    [[nodiscard]] static std::size_t receive_buffer_size_for(const st2110::PacketParsePolicy& policy) {
        return make_receive_buffer(policy).size();
    }

  protected:
    void process_received_datagram(st2110::ByteSpan) noexcept override {}
    void clear_media_runtime_objects() noexcept override { clear_common_runtime_objects(); }
};

st2110::RxVideoConfig make_video_rx_config() {
    st2110::RxVideoConfig cfg{};
    cfg.width = 1920;
    cfg.height = 1080;
    cfg.fps_num = 30000;
    cfg.fps_den = 1001;
    cfg.udp_port = 5004;
    cfg.payload_type = 112;
    cfg.local_ip = "127.0.0.1";
    cfg.dest_ip = "239.10.20.30";
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.scan_mode = st2110::VideoScanMode::Progressive;
    cfg.packing_mode = st2110::VideoPackingMode::Gpm;
    return cfg;
}

st2110::VideoReceivePipelineConfig make_video_pipeline_config(const st2110::RxVideoConfig& rx_cfg,
                                                              st2110::PartialFramePolicy partial_frame_policy) {
    return st2110::VideoReceivePipelineConfig{
        .depacketizer =
            st2110::DepacketizerConfig{
                .width = rx_cfg.width,
                .height = rx_cfg.height,
                .format = rx_cfg.format,
                .partial_frame_policy = partial_frame_policy,
                .scan_mode = rx_cfg.scan_mode,
                .packing_mode = rx_cfg.packing_mode,
            },
        .reconstructor =
            st2110::VideoUnitReconstructorConfig{
                .format = rx_cfg.format,
                .scan_mode = rx_cfg.scan_mode,
            },
    };
}

st2110::VideoReorderBufferConfig make_video_reorder_buffer_config(st2110::ReceiveReorderGapPolicy gap_policy,
                                                                  std::uint32_t window_size_packets) {
    return st2110::VideoReorderBufferConfig{
        .window_size_packets = window_size_packets,
        .reorder_tolerance_policy =
            st2110::ReceiveReorderTolerancePolicy{
                .gap_policy = gap_policy,
            },
    };
}

st2110::ParsedAudioChannelOrder make_stereo_channel_order() {
    auto parsed = st2110::parse_smpte2110_audio_channel_order_raw_value("SMPTE2110.(ST)");
    assert(parsed.has_value());
    return *parsed;
}

st2110::RxAudioConfig make_audio_rx_config() {
    st2110::RxAudioConfig cfg{};
    cfg.sampling_rate_hz = 48'000;
    cfg.packet_time_us = 1'000;
    cfg.samples_per_packet = 48;
    cfg.channel_count = 2;
    cfg.udp_port = 5006;
    cfg.payload_type = 111;
    cfg.local_ip = "127.0.0.1";
    cfg.dest_ip = "239.10.20.31";
    cfg.format = st2110::AudioSampleFormat::LinearPcm;
    cfg.pcm_bit_depth = st2110::AudioPcmBitDepth::Bits24;
    return cfg;
}

st2110::AudioReorderBufferConfig make_audio_reorder_buffer_config(st2110::ReceiveReorderGapPolicy gap_policy,
                                                                  std::uint16_t window_size_packets) {
    return st2110::AudioReorderBufferConfig{
        .window_size_packets = window_size_packets,
        .reorder_tolerance_policy =
            st2110::ReceiveReorderTolerancePolicy{
                .gap_policy = gap_policy,
            },
    };
}

void test_socket_rx_single_media_backend_base_remains_media_agnostic() {
    ProbeSingleMediaBackend video_probe(st2110::RxMediaKind::Video,
                                        st2110::RxBackendCapabilities{.video_rx = true, .audio_rx = false});
    ProbeSingleMediaBackend audio_probe(st2110::RxMediaKind::Audio,
                                        st2110::RxBackendCapabilities{.video_rx = false, .audio_rx = true});

    assert(video_probe.backend_name() == std::string_view{"socket"});
    assert(audio_probe.backend_name() == std::string_view{"socket"});

    assert(video_probe.capabilities().video_rx);
    assert(!video_probe.capabilities().audio_rx);
    assert(!audio_probe.capabilities().video_rx);
    assert(audio_probe.capabilities().audio_rx);

    assert(st2110::backend_is_stopped(video_probe.state()));
    assert(st2110::backend_is_stopped(audio_probe.state()));

    st2110::PacketParsePolicy default_policy{};
    st2110::PacketParsePolicy extended_policy{};
    extended_policy.max_udp_datagram_bytes = st2110::extendedUdpDatagramSizeLimitBytes;

    assert(ProbeSingleMediaBackend::receive_buffer_size_for(default_policy) ==
           (st2110::standardUdpDatagramSizeLimitBytes - st2110::udpHeaderBytes));
    assert(ProbeSingleMediaBackend::receive_buffer_size_for(extended_policy) ==
           (st2110::extendedUdpDatagramSizeLimitBytes - st2110::udpHeaderBytes));

    auto stopped_video = video_probe.stop();
    auto stopped_audio = audio_probe.stop();
    assert(stopped_video.has_value());
    assert(stopped_audio.has_value());
    assert(st2110::backend_is_stopped(*stopped_video));
    assert(st2110::backend_is_stopped(*stopped_audio));
}

void test_video_bootstrap_to_operational_adapter_preserves_modeled_axes() {
    const auto rx_cfg = make_video_rx_config();

    st2110::PacketParsePolicy packet_parse_policy{};
    packet_parse_policy.max_udp_datagram_bytes = st2110::extendedUdpDatagramSizeLimitBytes;

    const auto pipeline_cfg = make_video_pipeline_config(rx_cfg, st2110::PartialFramePolicy::EmitWithFlag);
    const st2110::VideoRtpTimestampMapperConfig timestamp_mapper_cfg{
        .rtp_clock_rate = 90'000,
        .initial_anchor_mode = st2110::RtpTimestampInitialAnchorMode::ConfiguredReference,
        .anchor_rtp_timestamp = 90'000,
        .anchor_timestamp_ns = 123'456'789,
    };
    const st2110::VideoReorderBufferConfig reorder_buffer_cfg =
        make_video_reorder_buffer_config(st2110::ReceiveReorderGapPolicy::WaitForMissing, 17U);

    st2110::VideoReceiverBootstrapConfig bootstrap{
        .packet_parse_policy = packet_parse_policy,
        .rx_config = rx_cfg,
        .receive_pipeline_config = pipeline_cfg,
        .timestamp_mapper_config = timestamp_mapper_cfg,
        .reorder_buffer_config = reorder_buffer_cfg,
        .timing_config = st2110::VideoReceiverTimingConfig{},
    };

    auto operational = st2110::socket_rx_video_operational_config_from_video_receiver_bootstrap(bootstrap);
    assert(operational.has_value());

    assert(operational->common.packet_parse_policy.max_udp_datagram_bytes == packet_parse_policy.max_udp_datagram_bytes);
    assert(operational->rx_config.scan_mode == rx_cfg.scan_mode);
    assert(operational->rx_config.packing_mode == rx_cfg.packing_mode);
    assert(operational->receive_pipeline_config.depacketizer.partial_frame_policy ==
           st2110::PartialFramePolicy::EmitWithFlag);
    assert(operational->receive_pipeline_config.depacketizer.scan_mode == rx_cfg.scan_mode);
    assert(operational->receive_pipeline_config.depacketizer.packing_mode == rx_cfg.packing_mode);
    assert(operational->timestamp_mapper_config.rtp_clock_rate == timestamp_mapper_cfg.rtp_clock_rate);
    assert(operational->timestamp_mapper_config.initial_anchor_mode == timestamp_mapper_cfg.initial_anchor_mode);
    assert(operational->timestamp_mapper_config.anchor_rtp_timestamp == timestamp_mapper_cfg.anchor_rtp_timestamp);
    assert(operational->timestamp_mapper_config.anchor_timestamp_ns == timestamp_mapper_cfg.anchor_timestamp_ns);
    assert(operational->reorder_buffer_config.window_size_packets == reorder_buffer_cfg.window_size_packets);
    assert(operational->reorder_buffer_config.reorder_tolerance_policy.gap_policy ==
           st2110::ReceiveReorderGapPolicy::WaitForMissing);

    auto expected_open_config = st2110::socket_rx_open_config_from_video_config(rx_cfg);
    assert(expected_open_config.has_value());
    assert(st2110::socket_rx_open_config_equal(operational->common.open_config, *expected_open_config));
}

void test_video_manual_to_operational_adapter_preserves_explicit_runtime_inputs() {
    const auto rx_cfg = make_video_rx_config();

    st2110::PacketParsePolicy packet_parse_policy{};
    packet_parse_policy.max_udp_datagram_bytes = st2110::extendedUdpDatagramSizeLimitBytes;

    const st2110::VideoRtpTimestampMapperConfig timestamp_mapper_cfg{
        .rtp_clock_rate = 90'000,
        .initial_anchor_mode = st2110::RtpTimestampInitialAnchorMode::ConfiguredReference,
        .anchor_rtp_timestamp = 77,
        .anchor_timestamp_ns = 88,
    };
    const st2110::VideoReorderBufferConfig reorder_buffer_cfg =
        make_video_reorder_buffer_config(st2110::ReceiveReorderGapPolicy::FlushGapOnce, 19U);

    auto operational = st2110::socket_rx_video_operational_config_from_rx_video_config(
        rx_cfg, packet_parse_policy, st2110::PartialFramePolicy::EmitWithFlag, timestamp_mapper_cfg, reorder_buffer_cfg);
    assert(operational.has_value());

    assert(operational->common.packet_parse_policy.max_udp_datagram_bytes == packet_parse_policy.max_udp_datagram_bytes);
    assert(operational->rx_config.scan_mode == rx_cfg.scan_mode);
    assert(operational->rx_config.packing_mode == rx_cfg.packing_mode);
    assert(operational->receive_pipeline_config.depacketizer.partial_frame_policy ==
           st2110::PartialFramePolicy::EmitWithFlag);
    assert(operational->timestamp_mapper_config.rtp_clock_rate == timestamp_mapper_cfg.rtp_clock_rate);
    assert(operational->timestamp_mapper_config.initial_anchor_mode == timestamp_mapper_cfg.initial_anchor_mode);
    assert(operational->timestamp_mapper_config.anchor_rtp_timestamp == 77U);
    assert(operational->timestamp_mapper_config.anchor_timestamp_ns == 88U);
    assert(operational->reorder_buffer_config.window_size_packets == 19U);
    assert(operational->reorder_buffer_config.reorder_tolerance_policy.gap_policy ==
           st2110::ReceiveReorderGapPolicy::FlushGapOnce);
}

void test_audio_bootstrap_to_operational_adapter_preserves_modeled_axes() {
    const auto rx_cfg = make_audio_rx_config();

    st2110::PacketParsePolicy packet_parse_policy{};
    packet_parse_policy.max_udp_datagram_bytes = st2110::extendedUdpDatagramSizeLimitBytes;

    auto audio_packet_policy = st2110::audio_rtp_packet_policy_from_rx_audio_config(rx_cfg);
    assert(audio_packet_policy.has_value());

    const st2110::AudioFrameAssemblerConfig frame_assembler_cfg{
        .storage_format = st2110::AudioSampleStorageFormat::InterleavedS32,
    };
    const st2110::AudioReorderBufferConfig reorder_buffer_cfg =
        make_audio_reorder_buffer_config(st2110::ReceiveReorderGapPolicy::FlushGapOnce, 7U);
    const st2110::AudioRtpTimestampMapperConfig timestamp_mapper_cfg{
        .rtp_clock_rate = rx_cfg.sampling_rate_hz,
        .initial_anchor_mode = st2110::RtpTimestampInitialAnchorMode::ConfiguredReference,
        .anchor_rtp_timestamp = 48'000,
        .anchor_timestamp_ns = 1'000'000'000ULL,
    };
    const auto channel_order = make_stereo_channel_order();

    st2110::AudioReceiverBootstrapConfig bootstrap{
        .packet_parse_policy = packet_parse_policy,
        .rx_config = rx_cfg,
        .audio_packet_policy = *audio_packet_policy,
        .frame_assembler_config = frame_assembler_cfg,
        .reorder_buffer_config = reorder_buffer_cfg,
        .timestamp_mapper_config = timestamp_mapper_cfg,
        .channel_order = channel_order,
    };

    auto operational = st2110::socket_rx_audio_operational_config_from_audio_receiver_bootstrap(bootstrap);
    assert(operational.has_value());

    assert(operational->common.packet_parse_policy.max_udp_datagram_bytes == packet_parse_policy.max_udp_datagram_bytes);
    assert(operational->audio_packet_policy.samples_per_packet == audio_packet_policy->samples_per_packet);
    assert(operational->audio_packet_policy.wire_format == audio_packet_policy->wire_format);
    assert(operational->frame_assembler_config.storage_format == frame_assembler_cfg.storage_format);
    assert(operational->reorder_buffer_config.window_size_packets == reorder_buffer_cfg.window_size_packets);
    assert(operational->reorder_buffer_config.reorder_tolerance_policy.gap_policy ==
           st2110::ReceiveReorderGapPolicy::FlushGapOnce);
    assert(operational->timestamp_mapper_config.rtp_clock_rate == timestamp_mapper_cfg.rtp_clock_rate);
    assert(operational->timestamp_mapper_config.initial_anchor_mode == timestamp_mapper_cfg.initial_anchor_mode);
    assert(operational->timestamp_mapper_config.anchor_rtp_timestamp == timestamp_mapper_cfg.anchor_rtp_timestamp);
    assert(operational->timestamp_mapper_config.anchor_timestamp_ns == timestamp_mapper_cfg.anchor_timestamp_ns);
    assert(operational->channel_order.raw_value == channel_order.raw_value);
    assert(operational->channel_order.declared_channel_count == channel_order.declared_channel_count);

    auto expected_open_config = st2110::socket_rx_open_config_from_audio_config(rx_cfg);
    assert(expected_open_config.has_value());
    assert(st2110::socket_rx_open_config_equal(operational->common.open_config, *expected_open_config));
}

void test_audio_manual_to_operational_adapter_preserves_explicit_runtime_inputs() {
    const auto rx_cfg = make_audio_rx_config();

    st2110::PacketParsePolicy packet_parse_policy{};
    packet_parse_policy.max_udp_datagram_bytes = st2110::extendedUdpDatagramSizeLimitBytes;

    const st2110::AudioFrameAssemblerConfig frame_assembler_cfg{
        .storage_format = st2110::AudioSampleStorageFormat::InterleavedS32,
    };
    const st2110::AudioReorderBufferConfig reorder_buffer_cfg =
        make_audio_reorder_buffer_config(st2110::ReceiveReorderGapPolicy::WaitForMissing, 9U);
    const st2110::AudioRtpTimestampMapperConfig timestamp_mapper_cfg{
        .rtp_clock_rate = rx_cfg.sampling_rate_hz,
        .initial_anchor_mode = st2110::RtpTimestampInitialAnchorMode::ConfiguredReference,
        .anchor_rtp_timestamp = 24,
        .anchor_timestamp_ns = 42,
    };
    const auto channel_order = make_stereo_channel_order();

    auto operational = st2110::socket_rx_audio_operational_config_from_rx_audio_config(
        rx_cfg, packet_parse_policy, frame_assembler_cfg, reorder_buffer_cfg, timestamp_mapper_cfg, channel_order);
    assert(operational.has_value());

    assert(operational->common.packet_parse_policy.max_udp_datagram_bytes == packet_parse_policy.max_udp_datagram_bytes);
    assert(operational->frame_assembler_config.storage_format == frame_assembler_cfg.storage_format);
    assert(operational->reorder_buffer_config.window_size_packets == 9U);
    assert(operational->reorder_buffer_config.reorder_tolerance_policy.gap_policy ==
           st2110::ReceiveReorderGapPolicy::WaitForMissing);
    assert(operational->timestamp_mapper_config.rtp_clock_rate == rx_cfg.sampling_rate_hz);
    assert(operational->timestamp_mapper_config.initial_anchor_mode == timestamp_mapper_cfg.initial_anchor_mode);
    assert(operational->timestamp_mapper_config.anchor_rtp_timestamp == timestamp_mapper_cfg.anchor_rtp_timestamp);
    assert(operational->timestamp_mapper_config.anchor_timestamp_ns == timestamp_mapper_cfg.anchor_timestamp_ns);
    assert(operational->channel_order.raw_value == channel_order.raw_value);
    assert(operational->channel_order.declared_channel_count == channel_order.declared_channel_count);
}

} // namespace

int main() {
    test_socket_rx_single_media_backend_base_remains_media_agnostic();
    test_video_bootstrap_to_operational_adapter_preserves_modeled_axes();
    test_video_manual_to_operational_adapter_preserves_explicit_runtime_inputs();
    test_audio_bootstrap_to_operational_adapter_preserves_modeled_axes();
    test_audio_manual_to_operational_adapter_preserves_explicit_runtime_inputs();
    return 0;
}
#include <cassert>
#include <optional>
#include <string>

#include <st2110/video_signaling.hpp>

static st2110::VideoStreamSignaling make_base_signaling() {
    st2110::VideoStreamSignaling s{};

    s.media.sampling = st2110::VideoSampling{
            st2110::VideoSampling::Known::YCbCr422,
            std::nullopt};
    s.media.width = 1920;
    s.media.height = 1080;
    s.media.fps_num = 30000;
    s.media.fps_den = 1001;
    s.media.depth = st2110::VideoBitDepth{8, false};
    s.media.colorimetry = st2110::VideoColorimetry{
            st2110::VideoColorimetry::Known::Bt709,
            std::nullopt};

    s.scan_mode = st2110::VideoScanMode::Progressive;
    s.packing_mode = st2110::VideoPackingMode::Gpm;
    s.max_udp_datagram_bytes = st2110::standardUdpDatagramSizeLimitBytes;

    s.media_clock_mode = st2110::MediaClockMode::Direct;
    s.timestamp_mode = st2110::TimestampMode::New;
    s.reference_clock.kind = st2110::ReferenceClockKind::Ptp;
    s.reference_clock.ptp = st2110::PtpReferenceClock{};
    s.ts_delay_sender_ticks = 0;

    s.sender_type = st2110::VideoSenderType::Narrow;
    s.troff_us = std::nullopt;
    s.cmax = std::nullopt;

    return s;
}

static void test_gpm_projects_to_depacketizer_config_with_runtime_packing_mode() {
    const st2110::VideoStreamSignaling s = make_base_signaling();

    auto cfg = st2110::depacketizer_config_from_video_stream_signaling(
            s,
            st2110::PartialFramePolicy::EmitWithFlag);

    assert(cfg.has_value());
    assert(cfg->width == 1920);
    assert(cfg->height == 1080);
    assert(cfg->format == st2110::PixelFormat::UYVY);
    assert(cfg->scan_mode == st2110::VideoScanMode::Progressive);
    assert(cfg->partial_frame_policy == st2110::PartialFramePolicy::EmitWithFlag);
    assert(cfg->packing_mode == st2110::VideoPackingMode::Gpm);
}

static void test_gpm_projects_to_receive_pipeline_config_with_runtime_packing_mode() {
    const st2110::VideoStreamSignaling s = make_base_signaling();

    auto cfg = st2110::video_receive_pipeline_config_from_video_stream_signaling(
            s,
            st2110::PartialFramePolicy::Drop);

    assert(cfg.has_value());
    assert(cfg->depacketizer.width == 1920);
    assert(cfg->depacketizer.height == 1080);
    assert(cfg->depacketizer.format == st2110::PixelFormat::UYVY);
    assert(cfg->depacketizer.scan_mode == st2110::VideoScanMode::Progressive);
    assert(cfg->depacketizer.partial_frame_policy == st2110::PartialFramePolicy::Drop);
    assert(cfg->depacketizer.packing_mode == st2110::VideoPackingMode::Gpm);

    assert(cfg->reconstructor.format == st2110::PixelFormat::UYVY);
    assert(cfg->reconstructor.scan_mode == st2110::VideoScanMode::Progressive);
}

static void test_gpm_projects_to_bootstrap_config_with_runtime_packing_mode() {
    const st2110::VideoStreamSignaling s = make_base_signaling();

    auto cfg = st2110::video_receiver_bootstrap_config_from_video_stream_signaling(
            s,
            5004,
            112,
            "0.0.0.0",
            "239.1.1.1",
            st2110::PartialFramePolicy::EmitWithFlag);

    assert(cfg.has_value());
    assert(cfg->packet_parse_policy.max_udp_datagram_bytes.has_value());
    assert(*cfg->packet_parse_policy.max_udp_datagram_bytes
           == st2110::standardUdpDatagramSizeLimitBytes);

    assert(cfg->rx_config.width == 1920);
    assert(cfg->rx_config.height == 1080);
    assert(cfg->rx_config.format == st2110::PixelFormat::UYVY);
    assert(cfg->rx_config.scan_mode == st2110::VideoScanMode::Progressive);

    assert(cfg->receive_pipeline_config.depacketizer.packing_mode
           == st2110::VideoPackingMode::Gpm);
}

static void test_bpm_remains_structurally_valid_in_signaling_model() {
    st2110::VideoStreamSignaling s = make_base_signaling();
    s.packing_mode = st2110::VideoPackingMode::Bpm;

    assert(st2110::validate_video_stream_signaling(s) == st2110::Error::Ok);
}

static void test_bpm_is_rejected_by_runtime_depacketizer_projection() {
    st2110::VideoStreamSignaling s = make_base_signaling();
    s.packing_mode = st2110::VideoPackingMode::Bpm;

    auto cfg = st2110::depacketizer_config_from_video_stream_signaling(
            s,
            st2110::PartialFramePolicy::EmitWithFlag);

    assert(!cfg.has_value());
    assert(cfg.error() == st2110::Error::Unsupported);
}

static void test_bpm_is_rejected_by_runtime_receive_pipeline_projection() {
    st2110::VideoStreamSignaling s = make_base_signaling();
    s.packing_mode = st2110::VideoPackingMode::Bpm;

    auto cfg = st2110::video_receive_pipeline_config_from_video_stream_signaling(
            s,
            st2110::PartialFramePolicy::EmitWithFlag);

    assert(!cfg.has_value());
    assert(cfg.error() == st2110::Error::Unsupported);
}

static void test_bpm_is_rejected_by_runtime_bootstrap_projection() {
    st2110::VideoStreamSignaling s = make_base_signaling();
    s.packing_mode = st2110::VideoPackingMode::Bpm;

    auto cfg = st2110::video_receiver_bootstrap_config_from_video_stream_signaling(
            s,
            5004,
            112,
            "0.0.0.0",
            "239.1.1.1",
            st2110::PartialFramePolicy::EmitWithFlag);

    assert(!cfg.has_value());
    assert(cfg.error() == st2110::Error::Unsupported);
}

int main() {
    test_gpm_projects_to_depacketizer_config_with_runtime_packing_mode();
    test_gpm_projects_to_receive_pipeline_config_with_runtime_packing_mode();
    test_gpm_projects_to_bootstrap_config_with_runtime_packing_mode();

    test_bpm_remains_structurally_valid_in_signaling_model();
    test_bpm_is_rejected_by_runtime_depacketizer_projection();
    test_bpm_is_rejected_by_runtime_receive_pipeline_projection();
    test_bpm_is_rejected_by_runtime_bootstrap_projection();
    return 0;
}
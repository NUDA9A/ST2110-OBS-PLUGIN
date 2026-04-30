#include <cassert>
#include <optional>
#include <string>

#include <st2110/depacketizer.hpp>
#include <st2110/video_receive_pipeline.hpp>
#include <st2110/video_signaling.hpp>
#include <st2110/video_unit_reconstructor.hpp>

static st2110::ReferenceClock make_valid_reference_clock() {
    st2110::ReferenceClock clock{};
    clock.kind = st2110::ReferenceClockKind::Ptp;
    clock.ptp = st2110::PtpReferenceClock{};
    return clock;
}

static st2110::VideoStreamSignaling make_base_signaling() {
    st2110::VideoStreamSignaling s{};

    s.media.sampling = st2110::VideoSampling{st2110::VideoSampling::Known::YCbCr422, std::nullopt};
    s.media.width = 1920;
    s.media.height = 1080;
    s.media.fps_num = 30000;
    s.media.fps_den = 1001;
    s.media.depth = st2110::VideoBitDepth{8, false};
    s.media.colorimetry = st2110::VideoColorimetry{st2110::VideoColorimetry::Known::Bt709, std::nullopt};

    s.scan_mode = st2110::VideoScanMode::Progressive;
    s.packing_mode = st2110::VideoPackingMode::Gpm;
    s.max_udp_datagram_bytes = st2110::standardUdpDatagramSizeLimitBytes;

    s.media_clock_mode = st2110::MediaClockMode::Direct;
    s.timestamp_mode = st2110::TimestampMode::New;
    s.reference_clock = make_valid_reference_clock();
    s.ts_delay_sender_ticks = 0;

    s.sender_type = st2110::VideoSenderType::Narrow;
    s.troff_us = std::nullopt;
    s.cmax = std::nullopt;

    return s;
}

static void test_depacketizer_config_is_projected_from_signaling() {
    const st2110::VideoStreamSignaling s = make_base_signaling();

    auto cfg = st2110::depacketizer_config_from_video_stream_signaling(s, st2110::PartialFramePolicy::EmitWithFlag);

    assert(cfg.has_value());
    assert(cfg->width == 1920);
    assert(cfg->height == 1080);
    assert(cfg->format == st2110::PixelFormat::UYVY);
    assert(cfg->scan_mode == st2110::VideoScanMode::Progressive);
    assert(cfg->partial_frame_policy == st2110::PartialFramePolicy::EmitWithFlag);
}

static void test_reconstructor_config_is_projected_from_signaling() {
    const st2110::VideoStreamSignaling s = make_base_signaling();

    auto cfg = st2110::video_unit_reconstructor_config_from_video_stream_signaling(s);

    assert(cfg.has_value());
    assert(cfg->format == st2110::PixelFormat::UYVY);
    assert(cfg->scan_mode == st2110::VideoScanMode::Progressive);
}

static void test_pipeline_config_is_projected_from_signaling() {
    const st2110::VideoStreamSignaling s = make_base_signaling();

    auto cfg = st2110::video_receive_pipeline_config_from_video_stream_signaling(s, st2110::PartialFramePolicy::Drop);

    assert(cfg.has_value());

    assert(cfg->depacketizer.width == 1920);
    assert(cfg->depacketizer.height == 1080);
    assert(cfg->depacketizer.format == st2110::PixelFormat::UYVY);
    assert(cfg->depacketizer.scan_mode == st2110::VideoScanMode::Progressive);
    assert(cfg->depacketizer.partial_frame_policy == st2110::PartialFramePolicy::Drop);

    assert(cfg->reconstructor.format == st2110::PixelFormat::UYVY);
    assert(cfg->reconstructor.scan_mode == st2110::VideoScanMode::Progressive);
}

static void test_pipeline_projection_preserves_interlaced_scan_mode_structurally() {
    st2110::VideoStreamSignaling s = make_base_signaling();
    s.scan_mode = st2110::VideoScanMode::Interlaced;

    auto cfg =
        st2110::video_receive_pipeline_config_from_video_stream_signaling(s, st2110::PartialFramePolicy::EmitWithFlag);

    assert(cfg.has_value());
    assert(cfg->depacketizer.scan_mode == st2110::VideoScanMode::Interlaced);
    assert(cfg->reconstructor.scan_mode == st2110::VideoScanMode::Interlaced);
}

static void test_invalid_signaling_is_rejected_before_runtime_projection() {
    st2110::VideoStreamSignaling s = make_base_signaling();
    s.reference_clock.ptp = std::nullopt; // invalid for kind=Ptp

    auto dep_cfg = st2110::depacketizer_config_from_video_stream_signaling(s, st2110::PartialFramePolicy::EmitWithFlag);
    assert(!dep_cfg.has_value());
    assert(dep_cfg.error() == st2110::Error::InvalidValue);

    auto rec_cfg = st2110::video_unit_reconstructor_config_from_video_stream_signaling(s);
    assert(!rec_cfg.has_value());
    assert(rec_cfg.error() == st2110::Error::InvalidValue);

    auto pipe_cfg =
        st2110::video_receive_pipeline_config_from_video_stream_signaling(s, st2110::PartialFramePolicy::EmitWithFlag);
    assert(!pipe_cfg.has_value());
    assert(pipe_cfg.error() == st2110::Error::InvalidValue);
}

static void test_structurally_valid_but_unmappable_runtime_format_is_unsupported() {
    st2110::VideoStreamSignaling s = make_base_signaling();
    s.media.depth =
        st2110::VideoBitDepth{10, false}; // structurally valid, but not currently mappable to UYVY runtime path

    auto dep_cfg = st2110::depacketizer_config_from_video_stream_signaling(s, st2110::PartialFramePolicy::EmitWithFlag);
    assert(!dep_cfg.has_value());
    assert(dep_cfg.error() == st2110::Error::Unsupported);

    auto rec_cfg = st2110::video_unit_reconstructor_config_from_video_stream_signaling(s);
    assert(!rec_cfg.has_value());
    assert(rec_cfg.error() == st2110::Error::Unsupported);

    auto pipe_cfg =
        st2110::video_receive_pipeline_config_from_video_stream_signaling(s, st2110::PartialFramePolicy::EmitWithFlag);
    assert(!pipe_cfg.has_value());
    assert(pipe_cfg.error() == st2110::Error::Unsupported);
}

int main() {
    test_depacketizer_config_is_projected_from_signaling();
    test_reconstructor_config_is_projected_from_signaling();
    test_pipeline_config_is_projected_from_signaling();
    test_pipeline_projection_preserves_interlaced_scan_mode_structurally();
    test_invalid_signaling_is_rejected_before_runtime_projection();
    test_structurally_valid_but_unmappable_runtime_format_is_unsupported();
    return 0;
}
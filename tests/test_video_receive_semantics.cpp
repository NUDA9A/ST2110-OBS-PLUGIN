#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <st2110/depacketizer.hpp>
#include <st2110/error.hpp>
#include <st2110/video_receive_semantics.hpp>
#include <st2110/video_scan_mode.hpp>

static st2110::PacketView make_packet(uint32_t rtp_timestamp, bool marker, uint32_t ext_seq) {
    st2110::PacketView pkt{};
    pkt.rtp.version = 2;
    pkt.rtp.payload_type = 112;
    pkt.rtp.seq_number = static_cast<uint16_t>(ext_seq & 0xFFFFu);
    pkt.rtp.timestamp = rtp_timestamp;
    pkt.rtp.ssrc = 0x12345678;
    pkt.rtp.marker = marker;
    pkt.extended_seq = ext_seq;
    pkt.segment_count = 0;
    pkt.payload_data = st2110::ByteSpan{};
    return pkt;
}

static void test_video_assembly_unit_kind_depends_on_scan_mode() {
    auto progressive = st2110::video_assembly_unit_kind(st2110::VideoScanMode::Progressive);
    auto interlaced = st2110::video_assembly_unit_kind(st2110::VideoScanMode::Interlaced);
    auto psf = st2110::video_assembly_unit_kind(st2110::VideoScanMode::PsF);

    assert(progressive.has_value());
    assert(interlaced.has_value());
    assert(psf.has_value());

    assert(*progressive == st2110::VideoAssemblyUnitKind::Frame);
    assert(*interlaced == st2110::VideoAssemblyUnitKind::Field);
    assert(*psf == st2110::VideoAssemblyUnitKind::Segment);
}

static void test_video_assembly_unit_kind_rejects_unknown_mode() {
    auto res = st2110::video_assembly_unit_kind(static_cast<st2110::VideoScanMode>(999));
    assert(!res.has_value());
    assert(res.error() == st2110::Error::InvalidValue);
}

static void test_progressive_completion_policy_matches_current_mvp_behavior() {
    auto policy = st2110::video_receive_completion_policy(st2110::VideoScanMode::Progressive);
    assert(policy.has_value());

    assert(policy->unit_kind == st2110::VideoAssemblyUnitKind::Frame);
    assert(policy->marker_terminates_current_unit);
    assert(policy->timestamp_change_terminates_previous_unit);
}

static void test_interlaced_completion_policy_is_unsupported_for_now() {
    auto policy = st2110::video_receive_completion_policy(st2110::VideoScanMode::Interlaced);
    assert(!policy.has_value());
    assert(policy.error() == st2110::Error::Unsupported);
}

static void test_psf_completion_policy_is_unsupported_for_now() {
    auto policy = st2110::video_receive_completion_policy(st2110::VideoScanMode::PsF);
    assert(!policy.has_value());
    assert(policy.error() == st2110::Error::Unsupported);
}

static void test_progressive_depacketizer_marker_behavior_is_unchanged() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 640;
    cfg.height = 480;
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.scan_mode = st2110::VideoScanMode::Progressive;

    st2110::Depacketizer dep(cfg);

    auto out1 = dep.push(make_packet(1000, false, 1));
    auto out2 = dep.push(make_packet(1000, true, 2));

    assert(out1.empty());
    assert(out2.size() == 1u);
    assert(out2[0].unit_kind == st2110::VideoAssemblyUnitKind::Frame);
    assert(out2[0].marker_seen);
    assert(dep.stats().units_partial == 1u);
}

static void test_progressive_depacketizer_timestamp_change_behavior_is_unchanged() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 640;
    cfg.height = 480;
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.scan_mode = st2110::VideoScanMode::Progressive;

    st2110::Depacketizer dep(cfg);

    auto out1 = dep.push(make_packet(1000, false, 1));
    auto out2 = dep.push(make_packet(1001, false, 2));

    assert(out1.empty());
    assert(out2.empty());

    assert(dep.has_unit_in_progress());
    assert(dep.current_unit_rtp_timestamp().has_value());
    assert(*dep.current_unit_rtp_timestamp() == 1001u);
    assert(dep.stats().units_dropped == 1u);
}

static void test_non_progressive_mode_is_rejected_locally_in_depacketizer() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 640;
    cfg.height = 480;
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.scan_mode = st2110::VideoScanMode::Interlaced;

    st2110::Depacketizer dep(cfg);

    bool threw = false;
    try {
        (void)dep.push(make_packet(1000, false, 1));
    } catch (const std::logic_error&) {
        threw = true;
    }

    assert(threw);
}

int main() {
    test_video_assembly_unit_kind_depends_on_scan_mode();
    test_video_assembly_unit_kind_rejects_unknown_mode();
    test_progressive_completion_policy_matches_current_mvp_behavior();
    test_interlaced_completion_policy_is_unsupported_for_now();
    test_psf_completion_policy_is_unsupported_for_now();
    test_progressive_depacketizer_marker_behavior_is_unchanged();
    test_progressive_depacketizer_timestamp_change_behavior_is_unchanged();
    test_non_progressive_mode_is_rejected_locally_in_depacketizer();
    return 0;
}
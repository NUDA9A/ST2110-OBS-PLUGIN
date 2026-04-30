#include <cassert>
#include <cstdint>
#include <stdexcept>

#include <st2110/depacketizer.hpp>
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

static st2110::DepacketizerConfig make_cfg(st2110::VideoScanMode mode) {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 640;
    cfg.height = 480;
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.scan_mode = mode;
    return cfg;
}

static void test_progressive_mode_reports_frame_unit_kind() {
    st2110::Depacketizer dep(make_cfg(st2110::VideoScanMode::Progressive));
    assert(dep.assembly_unit_kind() == st2110::VideoAssemblyUnitKind::Frame);
}

static void test_interlaced_mode_reports_field_unit_kind_even_before_runtime_support() {
    st2110::Depacketizer dep(make_cfg(st2110::VideoScanMode::Interlaced));
    assert(dep.assembly_unit_kind() == st2110::VideoAssemblyUnitKind::Field);
}

static void test_psf_mode_reports_segment_unit_kind_even_before_runtime_support() {
    st2110::Depacketizer dep(make_cfg(st2110::VideoScanMode::PsF));
    assert(dep.assembly_unit_kind() == st2110::VideoAssemblyUnitKind::Segment);
}

static void test_progressive_public_unit_state_api_works() {
    st2110::Depacketizer dep(make_cfg(st2110::VideoScanMode::Progressive));

    assert(!dep.has_unit_in_progress());
    assert(!dep.current_unit_rtp_timestamp().has_value());

    auto out = dep.push(make_packet(1000, false, 1));
    assert(out.empty());

    assert(dep.has_unit_in_progress());
    assert(dep.current_unit_rtp_timestamp().has_value());
    assert(*dep.current_unit_rtp_timestamp() == 1000u);
}

static void test_reset_clears_unit_state() {
    st2110::Depacketizer dep(make_cfg(st2110::VideoScanMode::Progressive));

    (void)dep.push(make_packet(1000, false, 1));
    assert(dep.has_unit_in_progress());
    assert(dep.current_unit_rtp_timestamp().has_value());

    dep.reset();

    assert(!dep.has_unit_in_progress());
    assert(!dep.current_unit_rtp_timestamp().has_value());
}

static void test_non_progressive_runtime_rejection_remains_localized() {
    st2110::Depacketizer dep(make_cfg(st2110::VideoScanMode::Interlaced));

    bool threw = false;
    try {
        (void)dep.push(make_packet(1000, false, 1));
    } catch (const std::logic_error &) {
        threw = true;
    }

    assert(threw);
}

int main() {
    test_progressive_mode_reports_frame_unit_kind();
    test_interlaced_mode_reports_field_unit_kind_even_before_runtime_support();
    test_psf_mode_reports_segment_unit_kind_even_before_runtime_support();
    test_progressive_public_unit_state_api_works();
    test_reset_clears_unit_state();
    test_non_progressive_runtime_rejection_remains_localized();
    return 0;
}
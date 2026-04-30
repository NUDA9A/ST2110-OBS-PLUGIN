#include <cassert>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <type_traits>

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

static st2110::DepacketizerConfig make_cfg(st2110::VideoScanMode mode) {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 640;
    cfg.height = 480;
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.scan_mode = mode;
    return cfg;
}

static void test_progressive_packet_assembly_key_matches_current_mvp_grouping() {
    auto pkt = make_packet(1000, false, 1);

    auto key = st2110::video_packet_assembly_key(st2110::VideoScanMode::Progressive, pkt);

    assert(key.has_value());
    assert(key->unit_kind == st2110::VideoAssemblyUnitKind::Frame);
    assert(key->rtp_timestamp == 1000u);
    assert(key->sub_unit_index == 0u);
}

static void test_interlaced_packet_assembly_key_is_unsupported_for_now() {
    auto pkt = make_packet(1000, false, 1);

    auto key = st2110::video_packet_assembly_key(st2110::VideoScanMode::Interlaced, pkt);

    assert(!key.has_value());
    assert(key.error() == st2110::Error::Unsupported);
}

static void test_psf_packet_assembly_key_is_unsupported_for_now() {
    auto pkt = make_packet(1000, false, 1);

    auto key = st2110::video_packet_assembly_key(st2110::VideoScanMode::PsF, pkt);

    assert(!key.has_value());
    assert(key.error() == st2110::Error::Unsupported);
}

static void test_same_video_assembly_key_compares_all_fields() {
    st2110::VideoAssemblyKey a{
        .unit_kind = st2110::VideoAssemblyUnitKind::Frame, .rtp_timestamp = 1000, .sub_unit_index = 0};
    st2110::VideoAssemblyKey b{
        .unit_kind = st2110::VideoAssemblyUnitKind::Frame, .rtp_timestamp = 1000, .sub_unit_index = 0};
    st2110::VideoAssemblyKey c{
        .unit_kind = st2110::VideoAssemblyUnitKind::Frame, .rtp_timestamp = 1001, .sub_unit_index = 0};
    st2110::VideoAssemblyKey d{
        .unit_kind = st2110::VideoAssemblyUnitKind::Frame, .rtp_timestamp = 1000, .sub_unit_index = 1};

    assert(st2110::same_video_assembly_key(a, b));
    assert(!st2110::same_video_assembly_key(a, c));
    assert(!st2110::same_video_assembly_key(a, d));
}

static void test_progressive_completion_policy_uses_generic_key_change_naming() {
    auto policy = st2110::video_receive_completion_policy(st2110::VideoScanMode::Progressive);
    assert(policy.has_value());

    assert(policy->unit_kind == st2110::VideoAssemblyUnitKind::Frame);
    assert(policy->marker_terminates_current_unit);
    assert(policy->key_change_terminates_previous_unit);
}

static void test_depacketizer_exposes_current_unit_key() {
    st2110::Depacketizer dep(make_cfg(st2110::VideoScanMode::Progressive));

    assert(!dep.current_unit_key().has_value());

    auto out = dep.push(make_packet(2000, false, 1));
    assert(out.empty());

    auto key = dep.current_unit_key();
    assert(key.has_value());
    assert(key->unit_kind == st2110::VideoAssemblyUnitKind::Frame);
    assert(key->rtp_timestamp == 2000u);
    assert(key->sub_unit_index == 0u);
}

static void test_depacketizer_grouping_still_behaves_the_same_through_keys() {
    st2110::Depacketizer dep(make_cfg(st2110::VideoScanMode::Progressive));

    auto out1 = dep.push(make_packet(3000, false, 1));
    auto out2 = dep.push(make_packet(3000, false, 2));
    auto out3 = dep.push(make_packet(3001, false, 3));

    assert(out1.empty());
    assert(out2.empty());
    assert(out3.empty());

    auto key = dep.current_unit_key();
    assert(key.has_value());
    assert(key->rtp_timestamp == 3001u);
    assert(key->unit_kind == st2110::VideoAssemblyUnitKind::Frame);
}

static void test_non_progressive_runtime_rejection_stays_localized_after_key_introduction() {
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
    test_progressive_packet_assembly_key_matches_current_mvp_grouping();
    test_interlaced_packet_assembly_key_is_unsupported_for_now();
    test_psf_packet_assembly_key_is_unsupported_for_now();
    test_same_video_assembly_key_compares_all_fields();
    test_progressive_completion_policy_uses_generic_key_change_naming();
    test_depacketizer_exposes_current_unit_key();
    test_depacketizer_grouping_still_behaves_the_same_through_keys();
    test_non_progressive_runtime_rejection_stays_localized_after_key_introduction();
    return 0;
}
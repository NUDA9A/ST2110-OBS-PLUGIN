#include <cassert>
#include <cstdint>

#include <st2110/depacketizer.hpp>

static st2110::PacketView make_packet(uint32_t rtp_timestamp, uint32_t ext_seq, bool marker) {
    st2110::PacketView pkt{};
    pkt.rtp.version = 2;
    pkt.rtp.payload_type = 112;
    pkt.rtp.seq_number = static_cast<uint16_t>(ext_seq & 0xFFFFu);
    pkt.rtp.timestamp = rtp_timestamp;
    pkt.rtp.marker = marker;
    pkt.rtp.ssrc = 0x12345678;
    pkt.extended_seq = ext_seq;
    pkt.segment_count = 0;
    pkt.payload_data = st2110::ByteSpan{};
    return pkt;
}

static void test_first_packet_without_marker_starts_unit_only() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 1920;
    cfg.height = 1080;
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.partial_frame_policy = st2110::PartialFramePolicy::EmitWithFlag;

    st2110::Depacketizer dep(cfg);

    auto out = dep.push(make_packet(1000, 1, false));

    assert(out.empty());
    assert(dep.has_unit_in_progress());
    assert(dep.current_unit_rtp_timestamp().has_value());
    assert(*dep.current_unit_rtp_timestamp() == 1000u);
}

static void test_marker_packet_emits_and_clears_state_under_emit_with_flag_policy() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 1920;
    cfg.height = 1080;
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.partial_frame_policy = st2110::PartialFramePolicy::EmitWithFlag;

    st2110::Depacketizer dep(cfg);

    auto out = dep.push(make_packet(1001, 2, true));

    assert(out.size() == 1);
    assert(out[0].unit_kind == st2110::VideoAssemblyUnitKind::Frame);
    assert(out[0].marker_seen);
    assert(out[0].can_emit);
    assert(!out[0].complete);
    assert(out[0].partial());
    assert(out[0].rtp_timestamp == 1001u);

    assert(!dep.has_unit_in_progress());
    assert(!dep.current_unit_rtp_timestamp().has_value());
}

static void test_marker_packet_drops_under_drop_policy_and_clears_state() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 1280;
    cfg.height = 720;
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.partial_frame_policy = st2110::PartialFramePolicy::Drop;

    st2110::Depacketizer dep(cfg);

    auto out = dep.push(make_packet(2000, 10, true));

    assert(out.empty());
    assert(!dep.has_unit_in_progress());
    assert(!dep.current_unit_rtp_timestamp().has_value());
}

static void test_same_timestamp_marker_closes_current_unit() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 640;
    cfg.height = 480;
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.partial_frame_policy = st2110::PartialFramePolicy::EmitWithFlag;

    st2110::Depacketizer dep(cfg);

    auto out1 = dep.push(make_packet(3000, 20, false));
    auto out2 = dep.push(make_packet(3000, 21, true));

    assert(out1.empty());
    assert(out2.size() == 1);
    assert(out2[0].unit_kind == st2110::VideoAssemblyUnitKind::Frame);
    assert(out2[0].rtp_timestamp == 3000u);
    assert(out2[0].marker_seen);

    assert(!dep.has_unit_in_progress());
    assert(!dep.current_unit_rtp_timestamp().has_value());
}

static void test_new_timestamp_closes_old_as_not_emittable_and_can_immediately_emit_new_marker_unit() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 320;
    cfg.height = 240;
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.partial_frame_policy = st2110::PartialFramePolicy::EmitWithFlag;

    st2110::Depacketizer dep(cfg);

    auto out1 = dep.push(make_packet(4000, 1, false));
    auto out2 = dep.push(make_packet(4001, 2, true));

    assert(out1.empty());
    assert(out2.size() == 1);
    assert(out2[0].unit_kind == st2110::VideoAssemblyUnitKind::Frame);
    assert(out2[0].rtp_timestamp == 4001u);
    assert(out2[0].marker_seen);

    assert(!dep.has_unit_in_progress());
    assert(!dep.current_unit_rtp_timestamp().has_value());
}

int main() {
    test_first_packet_without_marker_starts_unit_only();
    test_marker_packet_emits_and_clears_state_under_emit_with_flag_policy();
    test_marker_packet_drops_under_drop_policy_and_clears_state();
    test_same_timestamp_marker_closes_current_unit();
    test_new_timestamp_closes_old_as_not_emittable_and_can_immediately_emit_new_marker_unit();
    return 0;
}
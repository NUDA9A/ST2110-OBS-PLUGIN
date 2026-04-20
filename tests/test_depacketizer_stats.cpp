#include <cassert>
#include <cstdint>

#include <st2110/depacketizer.hpp>

static st2110::PacketView make_packet_header(uint32_t rtp_timestamp, uint32_t ext_seq, bool marker) {
    st2110::PacketView pkt{};
    pkt.rtp.version = 2;
    pkt.rtp.payload_type = 112;
    pkt.rtp.seq_number = static_cast<uint16_t>(ext_seq & 0xFFFFu);
    pkt.rtp.timestamp = rtp_timestamp;
    pkt.rtp.marker = marker;
    pkt.rtp.ssrc = 0x12345678;
    pkt.extended_seq = ext_seq;
    return pkt;
}

static void test_complete_frame_updates_ok_stats() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 4;
    cfg.height = 1;
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.partial_frame_policy = st2110::PartialFramePolicy::EmitWithFlag;

    st2110::Depacketizer dep(cfg);

    static const uint8_t row0[] = {0,1,2,3,4,5,6,7};

    st2110::PacketView pkt = make_packet_header(1000, 1, true);
    pkt.segment_count = 1;
    pkt.payload_data = st2110::ByteSpan(row0, sizeof(row0));
    pkt.segments[0].header.length = 8;
    pkt.segments[0].header.row_number = 0;
    pkt.segments[0].header.offset = 0;
    pkt.segments[0].header.field_id = false;
    pkt.segments[0].header.continuation = false;
    pkt.segments[0].data = pkt.payload_data;

    auto out = dep.push(pkt);
    (void)out;

    const auto& s = dep.stats();
    assert(s.packets_in == 1);
    assert(s.packets_used == 1);
    assert(s.frames_ok == 1);
    assert(s.frames_partial == 0);
    assert(s.frames_dropped == 0);
}

static void test_partial_emitted_frame_updates_partial_stats() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 4;
    cfg.height = 1;
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.partial_frame_policy = st2110::PartialFramePolicy::EmitWithFlag;

    st2110::Depacketizer dep(cfg);

    static const uint8_t partial[] = {0,1,2,3};

    st2110::PacketView pkt = make_packet_header(2000, 2, true);
    pkt.segment_count = 1;
    pkt.payload_data = st2110::ByteSpan(partial, sizeof(partial));
    pkt.segments[0].header.length = 4;
    pkt.segments[0].header.row_number = 0;
    pkt.segments[0].header.offset = 0;
    pkt.segments[0].header.field_id = false;
    pkt.segments[0].header.continuation = false;
    pkt.segments[0].data = pkt.payload_data;

    auto out = dep.push(pkt);
    (void)out;

    const auto& s = dep.stats();
    assert(s.packets_in == 1);
    assert(s.packets_used == 1);
    assert(s.frames_ok == 0);
    assert(s.frames_partial == 1);
    assert(s.frames_dropped == 0);
}

static void test_drop_policy_updates_dropped_stats() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 4;
    cfg.height = 1;
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.partial_frame_policy = st2110::PartialFramePolicy::Drop;

    st2110::Depacketizer dep(cfg);

    static const uint8_t partial[] = {0,1,2,3};

    st2110::PacketView pkt = make_packet_header(3000, 3, true);
    pkt.segment_count = 1;
    pkt.payload_data = st2110::ByteSpan(partial, sizeof(partial));
    pkt.segments[0].header.length = 4;
    pkt.segments[0].header.row_number = 0;
    pkt.segments[0].header.offset = 0;
    pkt.segments[0].header.field_id = false;
    pkt.segments[0].header.continuation = false;
    pkt.segments[0].data = pkt.payload_data;

    auto out = dep.push(pkt);
    assert(out.empty());

    const auto& s = dep.stats();
    assert(s.packets_in == 1);
    assert(s.packets_used == 1);
    assert(s.frames_ok == 0);
    assert(s.frames_partial == 0);
    assert(s.frames_dropped == 1);
}

static void test_timestamp_transition_without_marker_counts_as_dropped_frame() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 4;
    cfg.height = 1;
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.partial_frame_policy = st2110::PartialFramePolicy::EmitWithFlag;

    st2110::Depacketizer dep(cfg);

    static const uint8_t partial[] = {0,1,2,3};

    st2110::PacketView p1 = make_packet_header(4000, 10, false);
    p1.segment_count = 1;
    p1.payload_data = st2110::ByteSpan(partial, sizeof(partial));
    p1.segments[0].header.length = 4;
    p1.segments[0].header.row_number = 0;
    p1.segments[0].header.offset = 0;
    p1.segments[0].header.field_id = false;
    p1.segments[0].header.continuation = false;
    p1.segments[0].data = p1.payload_data;

    st2110::PacketView p2 = make_packet_header(4001, 11, false);
    p2.segment_count = 0;
    p2.payload_data = st2110::ByteSpan{};

    auto out1 = dep.push(p1);
    auto out2 = dep.push(p2);
    (void)out1;
    (void)out2;

    const auto& s = dep.stats();
    assert(s.packets_in == 2);
    assert(s.packets_used == 2);
    assert(s.frames_ok == 0);
    assert(s.frames_partial == 0);
    assert(s.frames_dropped == 1);
}

static void test_reset_clears_stats() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 4;
    cfg.height = 1;
    cfg.format = st2110::PixelFormat::UYVY;

    st2110::Depacketizer dep(cfg);

    static const uint8_t partial[] = {0,1,2,3};

    st2110::PacketView pkt = make_packet_header(5000, 1, true);
    pkt.segment_count = 1;
    pkt.payload_data = st2110::ByteSpan(partial, sizeof(partial));
    pkt.segments[0].header.length = 4;
    pkt.segments[0].header.row_number = 0;
    pkt.segments[0].header.offset = 0;
    pkt.segments[0].header.field_id = false;
    pkt.segments[0].header.continuation = false;
    pkt.segments[0].data = pkt.payload_data;

    (void)dep.push(pkt);

    dep.reset();

    const auto& s = dep.stats();
    assert(s.packets_in == 0);
    assert(s.packets_used == 0);
    assert(s.frames_ok == 0);
    assert(s.frames_partial == 0);
    assert(s.frames_dropped == 0);
}

int main() {
    test_complete_frame_updates_ok_stats();
    test_partial_emitted_frame_updates_partial_stats();
    test_drop_policy_updates_dropped_stats();
    test_timestamp_transition_without_marker_counts_as_dropped_frame();
    test_reset_clears_stats();
    return 0;
}
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

static void test_single_packet_full_frame_emits_complete() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 4;
    cfg.height = 1;
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.partial_frame_policy = st2110::PartialFramePolicy::EmitWithFlag;

    st2110::Depacketizer dep(cfg);

    static const uint8_t row0[] = {0, 1, 2, 3, 4, 5, 6, 7};

    st2110::PacketView pkt = make_packet_header(1000, 1, true);
    pkt.segment_count = 1;
    pkt.payload_data = st2110::ByteSpan(row0, sizeof(row0));
    pkt.segments[0].header.length = 8;
    pkt.segments[0].header.row_number = 0;
    pkt.segments[0].header.offset = 0; // sample position 0 -> byte offset 0
    pkt.segments[0].header.field_id = false;
    pkt.segments[0].header.continuation = false;
    pkt.segments[0].data = pkt.payload_data;

    auto out = dep.push(pkt);

    assert(out.size() == 1);
    assert(out[0].rtp_timestamp == 1000u);
    assert(out[0].complete == true);
    assert(out[0].partial() == false);

    const uint8_t *row = out[0].frame.row_data(0);
    for (int i = 0; i < 8; ++i) {
        assert(row[i] == static_cast<uint8_t>(i));
    }
}

static void test_two_packets_same_timestamp_accumulate_into_one_complete_frame() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 4;
    cfg.height = 1;
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.partial_frame_policy = st2110::PartialFramePolicy::EmitWithFlag;

    st2110::Depacketizer dep(cfg);

    static const uint8_t left[] = {0, 1, 2, 3};
    static const uint8_t right[] = {4, 5, 6, 7};

    st2110::PacketView p1 = make_packet_header(2000, 10, false);
    p1.segment_count = 1;
    p1.payload_data = st2110::ByteSpan(left, sizeof(left));
    p1.segments[0].header.length = 4;
    p1.segments[0].header.row_number = 0;
    p1.segments[0].header.offset = 0; // sample position 0 -> byte offset 0
    p1.segments[0].header.field_id = false;
    p1.segments[0].header.continuation = false;
    p1.segments[0].data = p1.payload_data;

    st2110::PacketView p2 = make_packet_header(2000, 11, true);
    p2.segment_count = 1;
    p2.payload_data = st2110::ByteSpan(right, sizeof(right));
    p2.segments[0].header.length = 4;
    p2.segments[0].header.row_number = 0;
    p2.segments[0].header.offset = 2; // sample position 2 -> byte offset 4
    p2.segments[0].header.field_id = false;
    p2.segments[0].header.continuation = false;
    p2.segments[0].data = p2.payload_data;

    auto out1 = dep.push(p1);
    auto out2 = dep.push(p2);

    assert(out1.empty());
    assert(out2.size() == 1);
    assert(out2[0].complete == true);

    const uint8_t *row = out2[0].frame.row_data(0);
    for (int i = 0; i < 8; ++i) {
        assert(row[i] == static_cast<uint8_t>(i));
    }
}

static void test_multi_segment_packet_writes_all_segments() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 4;
    cfg.height = 2;
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.partial_frame_policy = st2110::PartialFramePolicy::EmitWithFlag;

    st2110::Depacketizer dep(cfg);

    static const uint8_t bytes[] = {0, 1, 2, 3, 4, 5, 6, 7, 10, 11, 12, 13, 14, 15, 16, 17};

    st2110::PacketView pkt = make_packet_header(3000, 20, true);
    pkt.segment_count = 2;
    pkt.payload_data = st2110::ByteSpan(bytes, sizeof(bytes));

    pkt.segments[0].header.length = 8;
    pkt.segments[0].header.row_number = 0;
    pkt.segments[0].header.offset = 0; // sample position 0 -> byte offset 0
    pkt.segments[0].header.field_id = false;
    pkt.segments[0].header.continuation = true;
    pkt.segments[0].data = pkt.payload_data.subspan(0, 8);

    pkt.segments[1].header.length = 8;
    pkt.segments[1].header.row_number = 1;
    pkt.segments[1].header.offset = 0; // sample position 0 -> byte offset 0
    pkt.segments[1].header.field_id = false;
    pkt.segments[1].header.continuation = false;
    pkt.segments[1].data = pkt.payload_data.subspan(8, 8);

    auto out = dep.push(pkt);

    assert(out.size() == 1);
    assert(out[0].complete == true);

    const uint8_t *row0 = out[0].frame.row_data(0);
    const uint8_t *row1 = out[0].frame.row_data(1);

    for (int i = 0; i < 8; ++i) {
        assert(row0[i] == static_cast<uint8_t>(i));
        assert(row1[i] == static_cast<uint8_t>(10 + i));
    }
}

static void test_new_timestamp_discards_old_not_emittable_frame_and_starts_new_one() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 4;
    cfg.height = 1;
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.partial_frame_policy = st2110::PartialFramePolicy::EmitWithFlag;

    st2110::Depacketizer dep(cfg);

    static const uint8_t old_partial[] = {0, 1, 2, 3};
    static const uint8_t new_full[] = {10, 11, 12, 13, 14, 15, 16, 17};

    st2110::PacketView p1 = make_packet_header(4000, 1, false);
    p1.segment_count = 1;
    p1.payload_data = st2110::ByteSpan(old_partial, sizeof(old_partial));
    p1.segments[0].header.length = 4;
    p1.segments[0].header.row_number = 0;
    p1.segments[0].header.offset = 0; // sample position 0 -> byte offset 0
    p1.segments[0].header.field_id = false;
    p1.segments[0].header.continuation = false;
    p1.segments[0].data = p1.payload_data;

    st2110::PacketView p2 = make_packet_header(4001, 2, true);
    p2.segment_count = 1;
    p2.payload_data = st2110::ByteSpan(new_full, sizeof(new_full));
    p2.segments[0].header.length = 8;
    p2.segments[0].header.row_number = 0;
    p2.segments[0].header.offset = 0; // sample position 0 -> byte offset 0
    p2.segments[0].header.field_id = false;
    p2.segments[0].header.continuation = false;
    p2.segments[0].data = p2.payload_data;

    auto out1 = dep.push(p1);
    auto out2 = dep.push(p2);

    assert(out1.empty());
    assert(out2.size() == 1);
    assert(out2[0].rtp_timestamp == 4001u);
    assert(out2[0].complete == true);

    const uint8_t *row = out2[0].frame.row_data(0);
    for (int i = 0; i < 8; ++i) {
        assert(row[i] == static_cast<uint8_t>(10 + i));
    }
}

int main() {
    test_single_packet_full_frame_emits_complete();
    test_two_packets_same_timestamp_accumulate_into_one_complete_frame();
    test_multi_segment_packet_writes_all_segments();
    test_new_timestamp_discards_old_not_emittable_frame_and_starts_new_one();
    return 0;
}
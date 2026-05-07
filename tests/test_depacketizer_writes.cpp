#include <cassert>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include <st2110/receive/video/depacketizer.hpp>

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

static st2110::PacketView make_single_segment_packet(uint32_t rtp_timestamp,
                                                     uint32_t ext_seq,
                                                     bool marker,
                                                     uint16_t row_number,
                                                     uint16_t offset,
                                                     const uint8_t *bytes,
                                                     std::size_t size_bytes) {
    st2110::PacketView pkt = make_packet_header(rtp_timestamp, ext_seq, marker);
    pkt.segment_count = 1;
    pkt.payload_data = st2110::ByteSpan(bytes, size_bytes);
    pkt.segments[0].header.length = static_cast<uint16_t>(size_bytes);
    pkt.segments[0].header.row_number = row_number;
    pkt.segments[0].header.offset = offset;
    pkt.segments[0].header.field_id = false;
    pkt.segments[0].header.continuation = false;
    pkt.segments[0].data = pkt.payload_data;
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

    auto pkt = make_single_segment_packet(1000, 1, true, 0, 0, row0, sizeof(row0));

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

static void test_valid_multi_packet_same_row_fragmentation_emits_complete_frame() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 4;
    cfg.height = 1;
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.partial_frame_policy = st2110::PartialFramePolicy::EmitWithFlag;

    st2110::Depacketizer dep(cfg);

    static const uint8_t left[] = {0, 1, 2, 3};
    static const uint8_t right[] = {4, 5, 6, 7};

    auto p1 = make_single_segment_packet(2000, 10, false, 0, 0, left, sizeof(left));
    auto p2 = make_single_segment_packet(2000, 11, true, 0, 2, right, sizeof(right));

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

static void test_row_advance_across_packets_is_accepted() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 4;
    cfg.height = 2;
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.partial_frame_policy = st2110::PartialFramePolicy::EmitWithFlag;

    st2110::Depacketizer dep(cfg);

    static const uint8_t row0[] = {0, 1, 2, 3, 4, 5, 6, 7};
    static const uint8_t row1[] = {10, 11, 12, 13, 14, 15, 16, 17};

    auto p1 = make_single_segment_packet(3000, 20, false, 0, 0, row0, sizeof(row0));
    auto p2 = make_single_segment_packet(3000, 21, true, 1, 0, row1, sizeof(row1));

    auto out1 = dep.push(p1);
    auto out2 = dep.push(p2);

    assert(out1.empty());
    assert(out2.size() == 1);
    assert(out2[0].complete == true);

    const uint8_t *out_row0 = out2[0].frame.row_data(0);
    const uint8_t *out_row1 = out2[0].frame.row_data(1);

    for (int i = 0; i < 8; ++i) {
        assert(out_row0[i] == static_cast<uint8_t>(i));
        assert(out_row1[i] == static_cast<uint8_t>(10 + i));
    }
}

static void test_later_packet_with_lower_row_is_rejected() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 4;
    cfg.height = 2;
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.partial_frame_policy = st2110::PartialFramePolicy::EmitWithFlag;

    st2110::Depacketizer dep(cfg);

    static const uint8_t row1_left[] = {10, 11, 12, 13};
    static const uint8_t row0_left[] = {0, 1, 2, 3};

    auto first = make_single_segment_packet(4000, 30, false, 1, 0, row1_left, sizeof(row1_left));
    auto bad = make_single_segment_packet(4000, 31, false, 0, 0, row0_left, sizeof(row0_left));

    auto out1 = dep.push(first);
    assert(out1.empty());

    bool threw = false;
    try {
        (void)dep.push(bad);
    } catch (const std::invalid_argument &) {
        threw = true;
    }

    assert(threw);
    assert(dep.has_unit_in_progress());
    assert(dep.current_unit_rtp_timestamp().has_value());
    assert(*dep.current_unit_rtp_timestamp() == 4000u);
    assert(dep.stats().packets_in == 2u);
    assert(dep.stats().packets_used == 1u);
    assert(dep.stats().units_ok == 0u);
    assert(dep.stats().units_partial == 0u);
    assert(dep.stats().units_dropped == 0u);
}

static void test_later_packet_with_same_row_and_lower_or_equal_offset_is_rejected_without_corrupting_frame() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 4;
    cfg.height = 1;
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.partial_frame_policy = st2110::PartialFramePolicy::EmitWithFlag;

    st2110::Depacketizer dep(cfg);

    static const uint8_t left[] = {0, 1, 2, 3};
    static const uint8_t bad_equal[] = {100, 101, 102, 103};
    static const uint8_t bad_lower[] = {110, 111};
    static const uint8_t right[] = {4, 5, 6, 7};

    auto first = make_single_segment_packet(5000, 40, false, 0, 0, left, sizeof(left));
    auto bad_equal_offset = make_single_segment_packet(5000, 41, false, 0, 0, bad_equal, sizeof(bad_equal));
    auto bad_lower_offset = make_single_segment_packet(5000, 42, false, 0, 1, bad_lower, sizeof(bad_lower));
    auto final = make_single_segment_packet(5000, 43, true, 0, 2, right, sizeof(right));

    auto out1 = dep.push(first);
    assert(out1.empty());

    bool threw_equal = false;
    try {
        (void)dep.push(bad_equal_offset);
    } catch (const std::invalid_argument &) {
        threw_equal = true;
    }
    assert(threw_equal);

    bool threw_lower = false;
    try {
        (void)dep.push(bad_lower_offset);
    } catch (const std::invalid_argument &) {
        threw_lower = true;
    }
    assert(threw_lower);

    auto out2 = dep.push(final);

    assert(out2.size() == 1);
    assert(out2[0].complete == true);
    assert(out2[0].partial() == false);

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

    st2110::PacketView pkt = make_packet_header(6000, 50, true);
    pkt.segment_count = 2;
    pkt.payload_data = st2110::ByteSpan(bytes, sizeof(bytes));

    pkt.segments[0].header.length = 8;
    pkt.segments[0].header.row_number = 0;
    pkt.segments[0].header.offset = 0;
    pkt.segments[0].header.field_id = false;
    pkt.segments[0].header.continuation = true;
    pkt.segments[0].data = pkt.payload_data.subspan(0, 8);

    pkt.segments[1].header.length = 8;
    pkt.segments[1].header.row_number = 1;
    pkt.segments[1].header.offset = 0;
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

    auto p1 = make_single_segment_packet(7000, 60, false, 0, 0, old_partial, sizeof(old_partial));
    auto p2 = make_single_segment_packet(7001, 61, true, 0, 0, new_full, sizeof(new_full));

    auto out1 = dep.push(p1);
    auto out2 = dep.push(p2);

    assert(out1.empty());
    assert(out2.size() == 1);
    assert(out2[0].rtp_timestamp == 7001u);
    assert(out2[0].complete == true);

    const uint8_t *row = out2[0].frame.row_data(0);
    for (int i = 0; i < 8; ++i) {
        assert(row[i] == static_cast<uint8_t>(10 + i));
    }
}

int main() {
    test_single_packet_full_frame_emits_complete();
    test_valid_multi_packet_same_row_fragmentation_emits_complete_frame();
    test_row_advance_across_packets_is_accepted();
    test_later_packet_with_lower_row_is_rejected();
    test_later_packet_with_same_row_and_lower_or_equal_offset_is_rejected_without_corrupting_frame();
    test_multi_segment_packet_writes_all_segments();
    test_new_timestamp_discards_old_not_emittable_frame_and_starts_new_one();
    return 0;
}
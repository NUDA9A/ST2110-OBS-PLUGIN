#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <st2110/receive/shared/fixed_reorder_buffer.hpp>

namespace {

st2110::StoredPacket make_packet(uint32_t ext_seq, std::initializer_list<uint8_t> bytes) {
    st2110::StoredPacket pkt{};
    pkt.rtp.version = 2;
    pkt.rtp.payload_type = 112;
    pkt.rtp.seq_number = static_cast<uint16_t>(ext_seq & 0xFFFFu);
    pkt.rtp.timestamp = 0x1000 + ext_seq;
    pkt.rtp.ssrc = 0x12345678;
    pkt.extended_seq = ext_seq;
    pkt.segment_count = 1;
    pkt.segment_headers[0].length = static_cast<uint16_t>(bytes.size());
    pkt.segment_headers[0].row_number = 0;
    pkt.segment_headers[0].offset = 0;
    pkt.segment_headers[0].field_id = false;
    pkt.segment_headers[0].continuation = false;
    pkt.payload_data.assign(bytes.begin(), bytes.end());
    return pkt;
}

void test_constructor_rejects_zero_window() {
    bool thrown = false;
    try {
        st2110::FixedWindowReorderBuffer buf(0);
    } catch (const std::invalid_argument &) {
        thrown = true;
    }
    assert(thrown);
}

void test_in_order_packets_pop_immediately() {
    st2110::FixedWindowReorderBuffer buf(4);

    auto p10 = make_packet(10, {0x10});
    auto p11 = make_packet(11, {0x11});

    buf.push(p10.view());
    auto out1 = buf.pop_next();
    assert(out1.has_value());
    assert(out1->extended_seq == 10);
    assert(out1->view().payload_data[0] == 0x10);

    buf.push(p11.view());
    auto out2 = buf.pop_next();
    assert(out2.has_value());
    assert(out2->extended_seq == 11);
    assert(out2->view().payload_data[0] == 0x11);

    assert(!buf.pop_next().has_value());
}

void test_reorders_packets_within_window() {
    st2110::FixedWindowReorderBuffer buf(4);

    auto p100 = make_packet(100, {0xA0});
    auto p102 = make_packet(102, {0xA2});
    auto p101 = make_packet(101, {0xA1});

    buf.push(p100.view());
    buf.push(p102.view());
    buf.push(p101.view());

    auto out1 = buf.pop_next();
    assert(out1.has_value());
    assert(out1->extended_seq == 100);
    assert(out1->view().payload_data[0] == 0xA0);

    auto out2 = buf.pop_next();
    assert(out2.has_value());
    assert(out2->extended_seq == 101);
    assert(out2->view().payload_data[0] == 0xA1);

    auto out3 = buf.pop_next();
    assert(out3.has_value());
    assert(out3->extended_seq == 102);
    assert(out3->view().payload_data[0] == 0xA2);

    assert(!buf.pop_next().has_value());
}

void test_missing_packet_blocks_until_it_arrives() {
    st2110::FixedWindowReorderBuffer buf(4);

    auto p200 = make_packet(200, {0x20});
    auto p202 = make_packet(202, {0x22});
    auto p201 = make_packet(201, {0x21});

    buf.push(p200.view());
    buf.push(p202.view());

    auto out1 = buf.pop_next();
    assert(out1.has_value());
    assert(out1->extended_seq == 200);

    assert(!buf.pop_next().has_value());

    buf.push(p201.view());

    auto out2 = buf.pop_next();
    assert(out2.has_value());
    assert(out2->extended_seq == 201);
    assert(out2->view().payload_data[0] == 0x21);

    auto out3 = buf.pop_next();
    assert(out3.has_value());
    assert(out3->extended_seq == 202);
    assert(out3->view().payload_data[0] == 0x22);

    assert(!buf.pop_next().has_value());
}

void test_duplicate_packet_is_ignored() {
    st2110::FixedWindowReorderBuffer buf(4);

    auto p50 = make_packet(50, {0x50});

    buf.push(p50.view());
    buf.push(p50.view());

    auto out1 = buf.pop_next();
    assert(out1.has_value());
    assert(out1->extended_seq == 50);

    assert(!buf.pop_next().has_value());
}

void test_packet_outside_window_is_ignored() {
    st2110::FixedWindowReorderBuffer buf(3);

    auto p10 = make_packet(10, {0x10});
    auto p13 = make_packet(13, {0x13});
    auto p11 = make_packet(11, {0x11});

    buf.push(p10.view());
    buf.push(p13.view()); // outside window: valid range is 10,11,12
    buf.push(p11.view());

    auto out1 = buf.pop_next();
    assert(out1.has_value());
    assert(out1->extended_seq == 10);

    auto out2 = buf.pop_next();
    assert(out2.has_value());
    assert(out2->extended_seq == 11);

    assert(!buf.pop_next().has_value());
}

void test_reset_clears_buffer_state() {
    st2110::FixedWindowReorderBuffer buf(4);

    auto p30 = make_packet(30, {0x30});
    buf.push(p30.view());

    buf.reset();

    assert(!buf.pop_next().has_value());

    auto p40 = make_packet(40, {0x40});
    buf.push(p40.view());

    auto out = buf.pop_next();
    assert(out.has_value());
    assert(out->extended_seq == 40);
}

void test_wraparound_on_uint32_sequence() {
    st2110::FixedWindowReorderBuffer buf(4);

    auto p_fffe = make_packet(0xFFFFFFFEu, {0xFE});
    auto p_0000 = make_packet(0x00000000u, {0x00});
    auto p_ffff = make_packet(0xFFFFFFFFu, {0xFF});

    buf.push(p_fffe.view());
    buf.push(p_0000.view());
    buf.push(p_ffff.view());

    auto out1 = buf.pop_next();
    assert(out1.has_value());
    assert(out1->extended_seq == 0xFFFFFFFEu);
    assert(out1->view().payload_data[0] == 0xFE);

    auto out2 = buf.pop_next();
    assert(out2.has_value());
    assert(out2->extended_seq == 0xFFFFFFFFu);
    assert(out2->view().payload_data[0] == 0xFF);

    auto out3 = buf.pop_next();
    assert(out3.has_value());
    assert(out3->extended_seq == 0x00000000u);
    assert(out3->view().payload_data[0] == 0x00);

    assert(!buf.pop_next().has_value());
}

} // namespace

int main() {
    test_constructor_rejects_zero_window();
    test_in_order_packets_pop_immediately();
    test_reorders_packets_within_window();
    test_missing_packet_blocks_until_it_arrives();
    test_duplicate_packet_is_ignored();
    test_packet_outside_window_is_ignored();
    test_reset_clears_buffer_state();
    test_wraparound_on_uint32_sequence();
    return 0;
}
#include <cassert>
#include <cstdint>

#include <st2110/fixed_reorder_buffer.hpp>

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

    void test_stats_count_stored_and_popped_packets() {
        st2110::FixedWindowReorderBuffer buf(4);

        auto p10 = make_packet(10, {0x10});
        auto p11 = make_packet(11, {0x11});

        buf.push(p10.view());
        buf.push(p11.view());

        const auto& s1 = buf.stats();
        assert(s1.packets_pushed == 2);
        assert(s1.packets_stored == 2);
        assert(s1.packets_popped == 0);
        assert(s1.duplicates == 0);
        assert(s1.out_of_window == 0);
        assert(s1.late_packets == 0);
        assert(s1.missing_seq == 0);

        auto out1 = buf.pop_next();
        assert(out1.has_value());
        auto out2 = buf.pop_next();
        assert(out2.has_value());

        const auto& s2 = buf.stats();
        assert(s2.packets_popped == 2);
    }

    void test_duplicate_packet_is_accounted() {
        st2110::FixedWindowReorderBuffer buf(4);

        auto p50 = make_packet(50, {0x50});
        buf.push(p50.view());
        buf.push(p50.view());

        const auto& s = buf.stats();
        assert(s.packets_pushed == 2);
        assert(s.packets_stored == 1);
        assert(s.duplicates == 1);
        assert(s.out_of_window == 0);
        assert(s.late_packets == 0);
    }

    void test_out_of_window_packet_is_accounted() {
        st2110::FixedWindowReorderBuffer buf(3);

        auto p10 = make_packet(10, {0x10});
        auto p13 = make_packet(13, {0x13}); // valid window from 10 is 10,11,12

        buf.push(p10.view());
        buf.push(p13.view());

        const auto& s = buf.stats();
        assert(s.packets_pushed == 2);
        assert(s.packets_stored == 1);
        assert(s.out_of_window == 1);
        assert(s.late_packets == 0);
    }

    void test_late_packet_is_accounted() {
        st2110::FixedWindowReorderBuffer buf(4);

        auto p100 = make_packet(100, {0x10});
        auto p101 = make_packet(101, {0x11});

        buf.push(p100.view());
        auto out1 = buf.pop_next();
        assert(out1.has_value());
        assert(out1->extended_seq == 100);

        buf.push(p100.view()); // now late, next_expected is 101
        buf.push(p101.view());

        const auto& s = buf.stats();
        assert(s.packets_pushed == 3);
        assert(s.packets_stored == 2);
        assert(s.packets_popped == 1);
        assert(s.late_packets == 1);
        assert(s.out_of_window == 0);
    }

    void test_missing_seq_counted_once_per_missing_head() {
        st2110::FixedWindowReorderBuffer buf(4);

        auto p200 = make_packet(200, {0x20});
        auto p202 = make_packet(202, {0x22});

        buf.push(p200.view());
        buf.push(p202.view());

        auto out1 = buf.pop_next();
        assert(out1.has_value());
        assert(out1->extended_seq == 200);

        assert(!buf.pop_next().has_value());
        assert(!buf.pop_next().has_value()); // same missing head, must not count twice

        const auto& s = buf.stats();
        assert(s.missing_seq == 1);
    }

    void test_missing_seq_advances_to_next_gap_after_recovery() {
        st2110::FixedWindowReorderBuffer buf(5);

        auto p300 = make_packet(300, {0x30});
        auto p302 = make_packet(302, {0x32});
        auto p303 = make_packet(303, {0x33});
        auto p301 = make_packet(301, {0x31});

        buf.push(p300.view());
        buf.push(p302.view());
        buf.push(p303.view());

        auto out1 = buf.pop_next();
        assert(out1.has_value());
        assert(out1->extended_seq == 300);

        assert(!buf.pop_next().has_value()); // missing 301
        assert(buf.stats().missing_seq == 1);

        buf.push(p301.view());

        auto out2 = buf.pop_next();
        assert(out2.has_value());
        assert(out2->extended_seq == 301);

        auto out3 = buf.pop_next();
        assert(out3.has_value());
        assert(out3->extended_seq == 302);

        auto out4 = buf.pop_next();
        assert(out4.has_value());
        assert(out4->extended_seq == 303);

        assert(buf.stats().missing_seq == 1);
    }

    void test_reset_clears_stats_and_missing_state() {
        st2110::FixedWindowReorderBuffer buf(4);

        auto p10 = make_packet(10, {0x10});
        auto p12 = make_packet(12, {0x12});

        buf.push(p10.view());
        buf.push(p12.view());
        auto out1 = buf.pop_next();
        assert(out1.has_value());
        assert(!buf.pop_next().has_value());
        assert(buf.stats().missing_seq == 1);

        buf.reset();

        const auto& s = buf.stats();
        assert(s.packets_pushed == 0);
        assert(s.packets_stored == 0);
        assert(s.packets_popped == 0);
        assert(s.duplicates == 0);
        assert(s.out_of_window == 0);
        assert(s.late_packets == 0);
        assert(s.missing_seq == 0);

        auto p20 = make_packet(20, {0x20});
        buf.push(p20.view());
        auto out2 = buf.pop_next();
        assert(out2.has_value());
        assert(out2->extended_seq == 20);
    }

} // namespace

int main() {
    test_stats_count_stored_and_popped_packets();
    test_duplicate_packet_is_accounted();
    test_out_of_window_packet_is_accounted();
    test_late_packet_is_accounted();
    test_missing_seq_counted_once_per_missing_head();
    test_missing_seq_advances_to_next_gap_after_recovery();
    test_reset_clears_stats_and_missing_state();
    return 0;
}
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

    void test_flush_missing_once_noop_when_uninitialized() {
        st2110::FixedWindowReorderBuffer buf(4);

        assert(!buf.flush_missing_once());
        assert(buf.stats().missing_seq_flushed == 0);
    }

    void test_flush_missing_once_noop_when_next_packet_is_ready() {
        st2110::FixedWindowReorderBuffer buf(4);

        auto p10 = make_packet(10, {0x10});
        buf.push(p10.view());

        assert(!buf.flush_missing_once());
        assert(buf.stats().missing_seq_flushed == 0);

        auto out = buf.pop_next();
        assert(out.has_value());
        assert(out->extended_seq == 10);
    }

    void test_flush_missing_once_unblocks_single_gap() {
        st2110::FixedWindowReorderBuffer buf(4);

        auto p100 = make_packet(100, {0x10});
        auto p102 = make_packet(102, {0x12});

        buf.push(p100.view());
        buf.push(p102.view());

        auto out1 = buf.pop_next();
        assert(out1.has_value());
        assert(out1->extended_seq == 100);

        assert(!buf.pop_next().has_value());
        assert(buf.stats().missing_seq == 1);
        assert(buf.stats().missing_seq_flushed == 0);

        assert(buf.flush_missing_once());
        assert(buf.stats().missing_seq_flushed == 1);

        auto out2 = buf.pop_next();
        assert(out2.has_value());
        assert(out2->extended_seq == 102);
        assert(out2->view().payload_data[0] == 0x12);

        assert(!buf.pop_next().has_value());
    }

    void test_flush_missing_once_can_advance_multiple_gaps_one_by_one() {
        st2110::FixedWindowReorderBuffer buf(5);

        auto p200 = make_packet(200, {0x20});
        auto p203 = make_packet(203, {0x23});

        buf.push(p200.view());
        buf.push(p203.view());

        auto out1 = buf.pop_next();
        assert(out1.has_value());
        assert(out1->extended_seq == 200);

        assert(!buf.pop_next().has_value());
        assert(buf.stats().missing_seq == 1);

        assert(buf.flush_missing_once()); // flush 201
        assert(buf.stats().missing_seq_flushed == 1);

        assert(!buf.pop_next().has_value()); // now 202 missing
        assert(buf.stats().missing_seq == 2);

        assert(buf.flush_missing_once()); // flush 202
        assert(buf.stats().missing_seq_flushed == 2);

        auto out2 = buf.pop_next();
        assert(out2.has_value());
        assert(out2->extended_seq == 203);
        assert(out2->view().payload_data[0] == 0x23);
    }

    void test_flush_missing_once_noop_when_buffer_empty_after_progress() {
        st2110::FixedWindowReorderBuffer buf(4);

        auto p50 = make_packet(50, {0x50});
        buf.push(p50.view());

        auto out = buf.pop_next();
        assert(out.has_value());
        assert(out->extended_seq == 50);

        assert(!buf.flush_missing_once());
        assert(buf.stats().missing_seq_flushed == 0);
    }

    void test_reset_clears_flush_stats_and_state() {
        st2110::FixedWindowReorderBuffer buf(4);

        auto p10 = make_packet(10, {0x10});
        auto p12 = make_packet(12, {0x12});

        buf.push(p10.view());
        buf.push(p12.view());

        auto out1 = buf.pop_next();
        assert(out1.has_value());
        assert(!buf.pop_next().has_value());

        assert(buf.flush_missing_once());
        assert(buf.stats().missing_seq_flushed == 1);

        buf.reset();

        assert(buf.stats().missing_seq == 0);
        assert(buf.stats().missing_seq_flushed == 0);
        assert(!buf.flush_missing_once());
    }

} // namespace

int main() {
    test_flush_missing_once_noop_when_uninitialized();
    test_flush_missing_once_noop_when_next_packet_is_ready();
    test_flush_missing_once_unblocks_single_gap();
    test_flush_missing_once_can_advance_multiple_gaps_one_by_one();
    test_flush_missing_once_noop_when_buffer_empty_after_progress();
    test_reset_clears_flush_stats_and_state();
    return 0;
}
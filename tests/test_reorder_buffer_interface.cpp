#include <cassert>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include <st2110/reorder_buffer.hpp>

static_assert(std::is_abstract_v<st2110::IReorderBuffer>);
static_assert(
    std::is_same_v<decltype(std::declval<st2110::IReorderBuffer &>().pop_next()), std::optional<st2110::StoredPacket>>);
static_assert(
    std::is_same_v<decltype(std::declval<const st2110::IReorderBuffer &>().stats()), st2110::ReorderBufferStats>);

namespace {

st2110::StoredPacket copy_packet(const st2110::PacketView &src) {
    st2110::StoredPacket dst{};
    dst.rtp = src.rtp;
    dst.extended_seq = src.extended_seq;
    dst.segment_count = src.segment_count;
    dst.payload_data.assign(src.payload_data.begin(), src.payload_data.end());

    for (std::size_t i = 0; i < src.segment_count; ++i) {
        dst.segment_headers[i] = src.segments[i].header;
    }

    return dst;
}

class FakeReorderBuffer final : public st2110::IReorderBuffer {
  public:
    void push(const st2110::PacketView &packet) override {
        ++stats_.packets_pushed;
        stored_ = copy_packet(packet);
        ++stats_.packets_stored;
    }

    std::optional<st2110::StoredPacket> pop_next() override {
        if (!stored_.has_value()) {
            return std::nullopt;
        }

        auto out = std::move(stored_);
        stored_.reset();
        ++stats_.packets_popped;
        return out;
    }

    [[nodiscard]] st2110::ReorderBufferStats stats() const override { return stats_; }

    void reset() override {
        stored_.reset();
        stats_ = {};
    }

  private:
    std::optional<st2110::StoredPacket> stored_{};
    st2110::ReorderBufferStats stats_{};
};

void test_stored_packet_view_reconstructs_segments() {
    st2110::StoredPacket pkt{};
    pkt.rtp.version = 2;
    pkt.rtp.marker = true;
    pkt.rtp.payload_type = 112;
    pkt.rtp.seq_number = 0x1234;
    pkt.rtp.timestamp = 0x01020304;
    pkt.rtp.ssrc = 0x55667788;
    pkt.extended_seq = 0xABCD1234;
    pkt.segment_count = 2;

    pkt.segment_headers[0].length = 4;
    pkt.segment_headers[0].row_number = 10;
    pkt.segment_headers[0].offset = 0;
    pkt.segment_headers[0].field_id = false;
    pkt.segment_headers[0].continuation = true;

    pkt.segment_headers[1].length = 6;
    pkt.segment_headers[1].row_number = 11;
    pkt.segment_headers[1].offset = 8;
    pkt.segment_headers[1].field_id = false;
    pkt.segment_headers[1].continuation = false;

    pkt.payload_data = {0x10, 0x11, 0x12, 0x13, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25};

    const st2110::PacketView view = pkt.view();

    assert(view.rtp.version == 2);
    assert(view.rtp.marker == true);
    assert(view.rtp.payload_type == 112);
    assert(view.rtp.seq_number == 0x1234);
    assert(view.rtp.timestamp == 0x01020304);
    assert(view.rtp.ssrc == 0x55667788);

    assert(view.extended_seq == 0xABCD1234u);
    assert(view.segment_count == 2);
    assert(view.payload_data.size() == 10);

    assert(view.segments[0].header.length == 4);
    assert(view.segments[0].header.row_number == 10);
    assert(view.segments[0].data.size() == 4);
    assert(view.segments[0].data[0] == 0x10);
    assert(view.segments[0].data[3] == 0x13);

    assert(view.segments[1].header.length == 6);
    assert(view.segments[1].header.row_number == 11);
    assert(view.segments[1].data.size() == 6);
    assert(view.segments[1].data[0] == 0x20);
    assert(view.segments[1].data[5] == 0x25);
}

void test_fake_reorder_buffer_push_pop_reset_and_stats() {
    const uint8_t payload_bytes[] = {0xAA, 0xBB, 0xCC, 0xDD};

    st2110::PacketView src{};
    src.rtp.version = 2;
    src.rtp.payload_type = 112;
    src.rtp.seq_number = 0x0022;
    src.rtp.timestamp = 0x99;
    src.rtp.ssrc = 0xDEADBEEF;
    src.extended_seq = 0x10022;
    src.segment_count = 1;
    src.payload_data = st2110::ByteSpan(payload_bytes, sizeof(payload_bytes));

    src.segments[0].header.length = 4;
    src.segments[0].header.row_number = 5;
    src.segments[0].header.offset = 0;
    src.segments[0].header.field_id = false;
    src.segments[0].header.continuation = false;
    src.segments[0].data = src.payload_data;

    FakeReorderBuffer buf{};

    const auto empty_stats = buf.stats();
    assert(empty_stats.packets_pushed == 0);
    assert(empty_stats.packets_stored == 0);
    assert(empty_stats.packets_popped == 0);

    assert(!buf.pop_next().has_value());

    buf.push(src);

    const auto after_push = buf.stats();
    assert(after_push.packets_pushed == 1);
    assert(after_push.packets_stored == 1);
    assert(after_push.packets_popped == 0);

    auto stored = buf.pop_next();
    assert(stored.has_value());

    const auto after_pop = buf.stats();
    assert(after_pop.packets_pushed == 1);
    assert(after_pop.packets_stored == 1);
    assert(after_pop.packets_popped == 1);

    const st2110::PacketView out = stored->view();
    assert(out.extended_seq == 0x10022u);
    assert(out.segment_count == 1);
    assert(out.payload_data.size() == 4);
    assert(out.segments[0].header.length == 4);
    assert(out.segments[0].data.size() == 4);
    assert(out.segments[0].data[0] == 0xAA);
    assert(out.segments[0].data[3] == 0xDD);

    assert(!buf.pop_next().has_value());

    buf.push(src);
    const auto before_reset = buf.stats();
    assert(before_reset.packets_pushed == 2);
    assert(before_reset.packets_stored == 2);
    assert(before_reset.packets_popped == 1);

    buf.reset();
    assert(!buf.pop_next().has_value());

    const auto after_reset = buf.stats();
    assert(after_reset.packets_pushed == 0);
    assert(after_reset.packets_stored == 0);
    assert(after_reset.packets_popped == 0);
    assert(after_reset.duplicates == 0);
    assert(after_reset.out_of_window == 0);
    assert(after_reset.late_packets == 0);
    assert(after_reset.missing_seq == 0);
    assert(after_reset.missing_seq_flushed == 0);
}

} // namespace

int main() {
    test_stored_packet_view_reconstructs_segments();
    test_fake_reorder_buffer_push_pop_reset_and_stats();
    return 0;
}
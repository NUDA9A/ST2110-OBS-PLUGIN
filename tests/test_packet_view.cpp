#include <cassert>
#include <cstdint>
#include <type_traits>

#include <st2110/packet_view.hpp>

static_assert(std::is_standard_layout_v<st2110::SrdSegmentView>);
static_assert(std::is_standard_layout_v<st2110::PacketView>);

static void test_default_constructed_packet_view_is_empty() {
  st2110::PacketView pkt{};

  assert(pkt.extended_seq == 0);
  assert(pkt.segment_count == 0);
  assert(pkt.payload_data.size() == 0);

  for (std::size_t i = 0; i < st2110::maxPacketSrdSegments; ++i) {
    assert(pkt.segments[i].data.size() == 0);
    assert(pkt.segments[i].header.length == 0);
    assert(pkt.segments[i].header.row_number == 0);
    assert(pkt.segments[i].header.offset == 0);
    assert(pkt.segments[i].header.field_id == false);
    assert(pkt.segments[i].header.continuation == false);
  }
}

static void test_packet_view_can_hold_rtp_and_srd_segment_views() {
  const uint8_t payload_bytes[] = {0x11, 0x12, 0x13, 0x14, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26};

  st2110::PacketView pkt{};
  pkt.rtp.version = 2;
  pkt.rtp.marker = true;
  pkt.rtp.payload_type = 112;
  pkt.rtp.seq_number = 0x3456;
  pkt.rtp.timestamp = 0x01020304;
  pkt.rtp.ssrc = 0x11223344;

  pkt.extended_seq = 0x12343456;
  pkt.payload_data = st2110::ByteSpan(payload_bytes, sizeof(payload_bytes));
  pkt.segment_count = 2;

  pkt.segments[0].header.length = 4;
  pkt.segments[0].header.row_number = 10;
  pkt.segments[0].header.offset = 0;
  pkt.segments[0].header.field_id = false;
  pkt.segments[0].header.continuation = true;
  pkt.segments[0].data = pkt.payload_data.subspan(0, 4);

  pkt.segments[1].header.length = 6;
  pkt.segments[1].header.row_number = 11;
  pkt.segments[1].header.offset = 8;
  pkt.segments[1].header.field_id = false;
  pkt.segments[1].header.continuation = false;
  pkt.segments[1].data = pkt.payload_data.subspan(4, 6);

  assert(pkt.rtp.version == 2);
  assert(pkt.rtp.marker);
  assert(pkt.rtp.payload_type == 112);
  assert(pkt.rtp.seq_number == 0x3456);
  assert(pkt.rtp.timestamp == 0x01020304);
  assert(pkt.rtp.ssrc == 0x11223344);

  assert(pkt.extended_seq == 0x12343456);
  assert(pkt.payload_data.size() == 10);
  assert(pkt.segment_count == 2);

  assert(pkt.segments[0].header.length == 4);
  assert(pkt.segments[0].header.row_number == 10);
  assert(pkt.segments[0].header.offset == 0);
  assert(pkt.segments[0].header.continuation);
  assert(pkt.segments[0].data.size() == 4);
  assert(pkt.segments[0].data[0] == 0x11);
  assert(pkt.segments[0].data[3] == 0x14);

  assert(pkt.segments[1].header.length == 6);
  assert(pkt.segments[1].header.row_number == 11);
  assert(pkt.segments[1].header.offset == 8);
  assert(!pkt.segments[1].header.continuation);
  assert(pkt.segments[1].data.size() == 6);
  assert(pkt.segments[1].data[0] == 0x21);
  assert(pkt.segments[1].data[5] == 0x26);
}

static void test_packet_view_supports_zero_length_single_segment_case() {
  st2110::PacketView pkt{};
  pkt.segment_count = 1;
  pkt.payload_data = st2110::ByteSpan{};
  pkt.segments[0].header.length = 0;
  pkt.segments[0].header.row_number = 0;
  pkt.segments[0].header.offset = 0;
  pkt.segments[0].header.field_id = false;
  pkt.segments[0].header.continuation = false;
  pkt.segments[0].data = st2110::ByteSpan{};

  assert(pkt.segment_count == 1);
  assert(pkt.payload_data.size() == 0);
  assert(pkt.segments[0].header.length == 0);
  assert(pkt.segments[0].data.size() == 0);
}

int main() {
  test_default_constructed_packet_view_is_empty();
  test_packet_view_can_hold_rtp_and_srd_segment_views();
  test_packet_view_supports_zero_length_single_segment_case();
  return 0;
}
#include <cassert>
#include <cstdint>
#include <vector>

#include <st2110/packet_view.hpp>

static std::vector<uint8_t> make_packet(bool marker, const std::vector<uint8_t> &segment_bytes,
                                        const std::vector<uint8_t> &trailing_bytes) {
  std::vector<uint8_t> p;

  // RTP header (12 bytes)
  p.push_back(0x80);                                              // V=2
  p.push_back(static_cast<uint8_t>((marker ? 0x80 : 0x00) | 96)); // M + PT
  p.push_back(0x00);
  p.push_back(0x01); // seq
  p.push_back(0x00);
  p.push_back(0x00);
  p.push_back(0x00);
  p.push_back(0x2A); // ts
  p.push_back(0x00);
  p.push_back(0x00);
  p.push_back(0x00);
  p.push_back(0x01); // ssrc

  // ST 2110-20 payload header
  p.push_back(0x00);
  p.push_back(0x00); // ext seq hi16
  p.push_back(0x00);
  p.push_back(0x04); // SRD length = 4
  p.push_back(0x00);
  p.push_back(0x00); // F=0, row=0
  p.push_back(0x00);
  p.push_back(0x00); // C=0, offset=0

  p.insert(p.end(), segment_bytes.begin(), segment_bytes.end());
  p.insert(p.end(), trailing_bytes.begin(), trailing_bytes.end());
  return p;
}

static void test_parse_packet_exposes_trailing_padding_separately() {
  const std::vector<uint8_t> segment = {1, 2, 3, 4};
  const std::vector<uint8_t> trailing = {0, 0};

  const std::vector<uint8_t> bytes = make_packet(true, segment, trailing);

  auto parsed = st2110::PacketView::from_udp_datagram(st2110::ByteSpan(bytes.data(), bytes.size()));

  assert(parsed.has_value());

  const st2110::PacketView &pkt = *parsed;
  assert(pkt.segment_count == 1);
  assert(pkt.segments[0].data.size() == 4);
  assert(pkt.trailing_padding.size() == 2);

  assert(pkt.segments[0].data[0] == 1);
  assert(pkt.segments[0].data[1] == 2);
  assert(pkt.segments[0].data[2] == 3);
  assert(pkt.segments[0].data[3] == 4);

  assert(pkt.trailing_padding[0] == 0);
  assert(pkt.trailing_padding[1] == 0);

  assert(pkt.payload_data.size() == 6); // 4 bytes segment + 2 bytes trailing
}

static void test_parse_packet_without_trailing_padding_has_empty_tail() {
  const std::vector<uint8_t> segment = {9, 8, 7, 6};
  const std::vector<uint8_t> trailing = {};

  const std::vector<uint8_t> bytes = make_packet(true, segment, trailing);

  auto parsed = st2110::PacketView::from_udp_datagram(st2110::ByteSpan(bytes.data(), bytes.size()));

  assert(parsed.has_value());

  const st2110::PacketView &pkt = *parsed;
  assert(pkt.segment_count == 1);
  assert(pkt.segments[0].data.size() == 4);
  assert(pkt.trailing_padding.empty());
  assert(pkt.payload_data.size() == 4);
}

int main() {
  test_parse_packet_exposes_trailing_padding_separately();
  test_parse_packet_without_trailing_padding_has_empty_tail();
  return 0;
}
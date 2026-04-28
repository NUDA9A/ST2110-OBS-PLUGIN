#include <cassert>
#include <cstdint>
#include <stdexcept>

#include <st2110/depacketizer.hpp>

static st2110::PacketView make_packet_with_one_segment(uint32_t rtp_timestamp, uint32_t ext_seq, bool marker,
                                                       uint16_t row, uint16_t offset, uint16_t length,
                                                       const uint8_t *data, std::size_t size) {
  st2110::PacketView pkt{};
  pkt.rtp.version = 2;
  pkt.rtp.payload_type = 112;
  pkt.rtp.seq_number = static_cast<uint16_t>(ext_seq & 0xFFFFu);
  pkt.rtp.timestamp = rtp_timestamp;
  pkt.rtp.marker = marker;
  pkt.rtp.ssrc = 0x12345678;
  pkt.extended_seq = ext_seq;

  pkt.segment_count = 1;
  pkt.payload_data = st2110::ByteSpan(data, size);

  pkt.segments[0].header.length = length;
  pkt.segments[0].header.row_number = row;
  pkt.segments[0].header.offset = offset;
  pkt.segments[0].header.field_id = false;
  pkt.segments[0].header.continuation = false;
  pkt.segments[0].data = pkt.payload_data;

  return pkt;
}

static st2110::DepacketizerConfig make_cfg() {
  st2110::DepacketizerConfig cfg{};
  cfg.width = 4;
  cfg.height = 1;
  cfg.format = st2110::PixelFormat::UYVY;
  cfg.scan_mode = st2110::VideoScanMode::Progressive;
  cfg.partial_frame_policy = st2110::PartialFramePolicy::EmitWithFlag;
  return cfg;
}

static void test_depacketizer_accepts_aligned_progressive_uyvy_segment() {
  st2110::Depacketizer dep(make_cfg());

  static const uint8_t seg[] = {0xAA, 0xBB, 0xCC, 0xDD};

  auto pkt = make_packet_with_one_segment(1000, 1, true, 0, 2, 4, seg, sizeof(seg));

  auto out = dep.push(pkt);

  assert(out.size() == 1u);
  assert(out[0].unit_kind == st2110::VideoAssemblyUnitKind::Frame);
  assert(out[0].partial());

  const uint8_t *row0 = out[0].frame.row_data(0);
  assert(row0[4] == 0xAA);
  assert(row0[5] == 0xBB);
  assert(row0[6] == 0xCC);
  assert(row0[7] == 0xDD);
}

static void test_depacketizer_rejects_misaligned_offset_as_invalid_argument() {
  st2110::Depacketizer dep(make_cfg());

  static const uint8_t seg[] = {0x10, 0x11, 0x12, 0x13};

  auto pkt = make_packet_with_one_segment(1000, 1, true, 0, 1, 4, seg, sizeof(seg));

  bool threw = false;
  try {
    (void)dep.push(pkt);
  } catch (const std::invalid_argument &) {
    threw = true;
  }

  assert(threw);
}

static void test_depacketizer_rejects_misaligned_length_as_invalid_argument() {
  st2110::Depacketizer dep(make_cfg());

  static const uint8_t seg[] = {0x20, 0x21};

  auto pkt = make_packet_with_one_segment(1000, 1, true, 0, 0, 2, seg, sizeof(seg));

  bool threw = false;
  try {
    (void)dep.push(pkt);
  } catch (const std::invalid_argument &) {
    threw = true;
  }

  assert(threw);
}

int main() {
  test_depacketizer_accepts_aligned_progressive_uyvy_segment();
  test_depacketizer_rejects_misaligned_offset_as_invalid_argument();
  test_depacketizer_rejects_misaligned_length_as_invalid_argument();
  return 0;
}
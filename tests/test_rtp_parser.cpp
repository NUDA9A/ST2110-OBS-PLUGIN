#include <cassert>
#include <cstdint>
#include <vector>

#include <st2110/rtp.hpp>
#include <st2110/bytes.hpp>
#include <st2110/error.hpp>

static void test_basic_header() {
  // V=2,P=0,X=0,CC=0 => 0x80
  // M=1, PT=112      => 0xF0
  const uint8_t pkt[] = {0x80, 0xF0, 0x12, 0x34, 0x01, 0x02, 0x03, 0x04, 0x0A, 0x0B, 0x0C, 0x0D};

  auto res = st2110::parse_rtp_header(st2110::ByteSpan{pkt});
  assert(res.has_value());

  const auto &h = res.value();
  assert(h.version == 2);
  assert(h.padding_flag == false);
  assert(h.extension_flag == false);
  assert(h.csrc_count == 0);
  assert(h.marker == true);
  assert(h.payload_type == 112);
  assert(h.seq_number == 0x1234);
  assert(h.timestamp == 0x01020304);
  assert(h.ssrc == 0x0A0B0C0D);
  assert(h.payload_offset == 12);
  assert(h.payload_len == 0);
}

static void test_bad_version() {
  const uint8_t pkt[] = {0x40, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02};

  auto res = st2110::parse_rtp_header(st2110::ByteSpan{pkt});
  assert(!res.has_value());
  assert(res.error() == st2110::Error::BadRTPVersion);
}

static void test_short_packet() {
  const uint8_t pkt[] = {0x80, 0x00, 0x00};

  auto res = st2110::parse_rtp_header(st2110::ByteSpan{pkt});
  assert(!res.has_value());
  assert(res.error() == st2110::Error::ShortPacket);
}

static void test_csrc_offset() {
  // CC=2 => header size = 12 + 8 = 20
  const uint8_t pkt[] = {0x82, 0x70, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
                         0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x33, 0x44, 0x99};

  auto res = st2110::parse_rtp_header(st2110::ByteSpan{pkt});
  assert(res.has_value());

  const auto &h = res.value();
  assert(h.csrc_count == 2);
  assert(h.payload_offset == 20);
  assert(h.payload_len == 1);
}

static void test_extension_offset() {
  // X=1, len_words=1 => +4 bytes ext header +4 bytes ext data
  const uint8_t pkt[] = {0x90, 0x70, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
                         0x02, 0xBE, 0xDE, 0x00, 0x01, 0xDE, 0xAD, 0xBE, 0xEF, 0x77};

  auto res = st2110::parse_rtp_header(st2110::ByteSpan{pkt});
  assert(res.has_value());

  const auto &h = res.value();
  assert(h.extension_flag == true);
  assert(h.payload_offset == 20);
  assert(h.payload_len == 1);
}

static void test_csrc_and_extension_offset() {
  // CC=2 => 8 bytes CSRC
  // X=1, len_words=2 => +4 bytes ext header +8 bytes ext data
  // total header size = 12 + 8 + 4 + 8 = 32
  const uint8_t pkt[] = {0x92, 0x60, 0x00, 0x05, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x20,

                         0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x33, 0x44,

                         0x10, 0x00, 0x00, 0x02, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,

                         0x99, 0x88};

  auto res = st2110::parse_rtp_header(st2110::ByteSpan{pkt});
  assert(res.has_value());

  const auto &h = res.value();
  assert(h.extension_flag == true);
  assert(h.csrc_count == 2);
  assert(h.payload_offset == 32);
  assert(h.payload_len == 2);
}

static void test_truncated_extension_header_is_short_packet() {
  // X=1, but only 2 bytes remain after fixed RTP header
  const uint8_t pkt[] = {0x90, 0x70, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0xBE, 0xDE};

  auto res = st2110::parse_rtp_header(st2110::ByteSpan{pkt});
  assert(!res.has_value());
  assert(res.error() == st2110::Error::ShortPacket);
}

static void test_truncated_extension_data_is_short_packet() {
  // X=1, len_words=2 => need 8 bytes ext data, provide only 4
  const uint8_t pkt[] = {0x90, 0x70, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
                         0x00, 0x02, 0xBE, 0xDE, 0x00, 0x02, 0xDE, 0xAD, 0xBE, 0xEF};

  auto res = st2110::parse_rtp_header(st2110::ByteSpan{pkt});
  assert(!res.has_value());
  assert(res.error() == st2110::Error::ShortPacket);
}

static void test_padding() {
  // P=1, last byte says padding size = 4
  std::vector<uint8_t> pkt = {0xA0, 0x70, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
                              0x00, 0x02, 0x10, 0x20, 0x30, 0x00, 0x00, 0x00, 0x04};

  auto res = st2110::parse_rtp_header(st2110::ByteSpan{pkt.data(), pkt.size()});
  assert(res.has_value());

  const auto &h = res.value();
  assert(h.padding_flag == true);
  assert(h.payload_offset == 12);
  assert(h.payload_len == 3);
}

static void test_padding_with_extension_and_csrc() {
  // P=1, X=1, CC=1
  // header = 12 + 4 + 4 + 4 = 24
  // payload = 3 bytes
  // padding = 2 bytes, last byte == 2
  const uint8_t pkt[] = {0xB1, 0x70, 0x00, 0x09, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x22,

                         0xCA, 0xFE, 0xBA, 0xBE,

                         0xBE, 0xDE, 0x00, 0x01, 0x10, 0x20, 0x30, 0x40,

                         0xAA, 0xBB, 0xCC, 0x00, 0x02};

  auto res = st2110::parse_rtp_header(st2110::ByteSpan{pkt});
  assert(res.has_value());

  const auto &h = res.value();
  assert(h.padding_flag == true);
  assert(h.extension_flag == true);
  assert(h.csrc_count == 1);
  assert(h.payload_offset == 24);
  assert(h.payload_len == 3);
}

static void test_invalid_padding() {
  // P=1, but last byte says padding size = 0 -> invalid
  std::vector<uint8_t> pkt = {0xA0, 0x70, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
                              0x00, 0x02, 0x10, 0x20, 0x30, 0x00, 0x00, 0x00, 0x00};

  auto res = st2110::parse_rtp_header(st2110::ByteSpan{pkt.data(), pkt.size()});
  assert(!res.has_value());
  assert(res.error() == st2110::Error::InvalidValue);
}

int main() {
  test_basic_header();
  test_bad_version();
  test_short_packet();
  test_csrc_offset();
  test_extension_offset();
  test_csrc_and_extension_offset();
  test_truncated_extension_header_is_short_packet();
  test_truncated_extension_data_is_short_packet();
  test_padding();
  test_padding_with_extension_and_csrc();
  test_invalid_padding();
  return 0;
}
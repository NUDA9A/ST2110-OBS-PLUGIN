#include <cassert>
#include <cstdint>
#include <vector>

#include <st2110/bytes.hpp>
#include <st2110/error.hpp>
#include <st2110/pixel_format.hpp>
#include <st2110/st2110_20.hpp>
#include <st2110/video_segment_constraints.hpp>

static st2110::SrdHeader make_header(uint16_t length, uint16_t row, uint16_t offset) {
  st2110::SrdHeader h{};
  h.length = length;
  h.row_number = row;
  h.offset = offset;
  h.field_id = false;
  h.continuation = false;
  return h;
}

static void test_uyvy_rules_are_reported() {
  auto rules = st2110::video_segment_constraints(st2110::PixelFormat::UYVY);
  assert(rules.has_value());
  assert(rules->pgroup_bytes == 4u);
  assert(rules->offset_alignment_samples == 2u);
}

static void test_uyvy_aligned_segment_is_valid() {
  const auto header = make_header(8, 3, 4);
  const std::vector<uint8_t> bytes(8, 0xAB);

  const auto err = st2110::validate_video_segment_for_format(st2110::PixelFormat::UYVY, header,
                                                             st2110::ByteSpan(bytes.data(), bytes.size()));

  assert(err == st2110::Error::Ok);
}

static void test_uyvy_rejects_length_not_multiple_of_pgroup() {
  const auto header = make_header(6, 1, 4);
  const std::vector<uint8_t> bytes(6, 0x11);

  const auto err = st2110::validate_video_segment_for_format(st2110::PixelFormat::UYVY, header,
                                                             st2110::ByteSpan(bytes.data(), bytes.size()));

  assert(err == st2110::Error::InvalidValue);
}

static void test_uyvy_rejects_offset_not_aligned_to_full_bandwidth_samples() {
  const auto header = make_header(8, 1, 3);
  const std::vector<uint8_t> bytes(8, 0x22);

  const auto err = st2110::validate_video_segment_for_format(st2110::PixelFormat::UYVY, header,
                                                             st2110::ByteSpan(bytes.data(), bytes.size()));

  assert(err == st2110::Error::InvalidValue);
}

static void test_rejects_data_size_mismatch_with_header_length() {
  const auto header = make_header(8, 1, 4);
  const std::vector<uint8_t> bytes(4, 0x33);

  const auto err = st2110::validate_video_segment_for_format(st2110::PixelFormat::UYVY, header,
                                                             st2110::ByteSpan(bytes.data(), bytes.size()));

  assert(err == st2110::Error::InvalidValue);
}

static void test_zero_length_with_empty_data_is_allowed_by_format_helper() {
  const auto header = make_header(0, 0, 0);
  const st2110::ByteSpan empty{};

  const auto err = st2110::validate_video_segment_for_format(st2110::PixelFormat::UYVY, header, empty);

  assert(err == st2110::Error::Ok);
}

static void test_unknown_format_is_unsupported() {
  const auto header = make_header(8, 0, 0);
  const std::vector<uint8_t> bytes(8, 0x44);

  const auto err = st2110::validate_video_segment_for_format(static_cast<st2110::PixelFormat>(999), header,
                                                             st2110::ByteSpan(bytes.data(), bytes.size()));

  assert(err == st2110::Error::Unsupported);
}

int main() {
  test_uyvy_rules_are_reported();
  test_uyvy_aligned_segment_is_valid();
  test_uyvy_rejects_length_not_multiple_of_pgroup();
  test_uyvy_rejects_offset_not_aligned_to_full_bandwidth_samples();
  test_rejects_data_size_mismatch_with_header_length();
  test_zero_length_with_empty_data_is_allowed_by_format_helper();
  test_unknown_format_is_unsupported();
  return 0;
}
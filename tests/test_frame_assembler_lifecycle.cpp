#include <cassert>
#include <cstdint>
#include <stdexcept>

#include <st2110/bytes.hpp>
#include <st2110/frame_assembler.hpp>

static void test_begin_write_end_roundtrip() {
  st2110::FrameAssembler assembler(8, 2, st2110::PixelFormat::UYVY);

  const uint8_t seg0[] = {0x10, 0x11, 0x12, 0x13};
  const uint8_t seg1[] = {0x20, 0x21, 0x22, 0x23};

  assert(!assembler.in_progress());

  assembler.begin(0x11223344);
  assert(assembler.in_progress());
  assert(assembler.current_rtp_timestamp() == 0x11223344u);

  assembler.write_segment(0, 0, 0, st2110::ByteSpan(seg0, sizeof(seg0)));
  assembler.write_segment(0, 1, 4, st2110::ByteSpan(seg1, sizeof(seg1)));

  st2110::FrameAssemblerEndResult result = assembler.end(true);
  assert(result.status == st2110::FrameAssemblerEndStatus::EmittedPartial);
  assert(result.unit.has_value());

  assert(!assembler.in_progress());

  const st2110::AssembledVideoUnit &out = *result.unit;

  assert(out.unit_kind == st2110::VideoAssemblyUnitKind::Frame);
  assert(out.rtp_timestamp == 0x11223344u);
  assert(out.marker_seen == true);
  assert(out.can_emit == true);
  assert(out.complete == false);
  assert(out.partial() == true);

  assert(out.frame.width() == 8);
  assert(out.frame.height() == 2);
  assert(out.frame.format() == st2110::PixelFormat::UYVY);
  assert(out.frame.stride_bytes() == 16);

  const uint8_t *row0 = out.frame.row_data(0);
  const uint8_t *row1 = out.frame.row_data(1);

  assert(row0[0] == 0x10);
  assert(row0[1] == 0x11);
  assert(row0[2] == 0x12);
  assert(row0[3] == 0x13);

  assert(row1[4] == 0x20);
  assert(row1[5] == 0x21);
  assert(row1[6] == 0x22);
  assert(row1[7] == 0x23);
}

static void test_end_marker_false_is_not_emittable() {
  st2110::FrameAssembler assembler(4, 1, st2110::PixelFormat::UYVY);

  assembler.begin(77);
  st2110::FrameAssemblerEndResult result = assembler.end(false);

  assert(result.status == st2110::FrameAssemblerEndStatus::NotEmittable);
  assert(!result.unit.has_value());
}

static void test_write_before_begin_rejected() {
  st2110::FrameAssembler assembler(4, 1, st2110::PixelFormat::UYVY);

  const uint8_t seg[] = {0xAA, 0xBB};

  bool thrown = false;
  try {
    assembler.write_segment(0, 0, 0, st2110::ByteSpan(seg, sizeof(seg)));
  } catch (const std::logic_error &) {
    thrown = true;
  }
  assert(thrown);
}

static void test_end_before_begin_rejected() {
  st2110::FrameAssembler assembler(4, 1, st2110::PixelFormat::UYVY);

  bool thrown = false;
  try {
    (void)assembler.end(true);
  } catch (const std::logic_error &) {
    thrown = true;
  }
  assert(thrown);
}

static void test_current_timestamp_before_begin_rejected() {
  st2110::FrameAssembler assembler(4, 1, st2110::PixelFormat::UYVY);

  bool thrown = false;
  try {
    (void)assembler.current_rtp_timestamp();
  } catch (const std::logic_error &) {
    thrown = true;
  }
  assert(thrown);
}

static void test_begin_while_in_progress_rejected() {
  st2110::FrameAssembler assembler(4, 1, st2110::PixelFormat::UYVY);

  assembler.begin(100);

  bool thrown = false;
  try {
    assembler.begin(101);
  } catch (const std::logic_error &) {
    thrown = true;
  }
  assert(thrown);
}

static void test_second_begin_after_end_starts_clean_frame() {
  st2110::FrameAssembler assembler(4, 1, st2110::PixelFormat::UYVY);

  const uint8_t seg[] = {0xDE, 0xAD, 0xBE, 0xEF};

  assembler.begin(1);
  assembler.write_segment(0, 0, 0, st2110::ByteSpan(seg, sizeof(seg)));
  st2110::FrameAssemblerEndResult first_result = assembler.end(true);
  assert(first_result.status == st2110::FrameAssemblerEndStatus::EmittedPartial);
  assert(first_result.unit.has_value());

  const st2110::AssembledVideoUnit &first = *first_result.unit;
  assert(first.unit_kind == st2110::VideoAssemblyUnitKind::Frame);
  assert(first.frame.row_data(0)[0] == 0xDE);
  assert(first.frame.row_data(0)[1] == 0xAD);

  assembler.begin(2);
  st2110::FrameAssemblerEndResult second_result = assembler.end(true);
  assert(second_result.status == st2110::FrameAssemblerEndStatus::EmittedPartial);
  assert(second_result.unit.has_value());

  const st2110::AssembledVideoUnit &second = *second_result.unit;
  assert(second.unit_kind == st2110::VideoAssemblyUnitKind::Frame);
  assert(second.rtp_timestamp == 2u);
  assert(second.frame.row_data(0)[0] == 0x00);
  assert(second.frame.row_data(0)[1] == 0x00);
  assert(second.frame.row_data(0)[2] == 0x00);
  assert(second.frame.row_data(0)[3] == 0x00);
}

int main() {
  test_begin_write_end_roundtrip();
  test_end_marker_false_is_not_emittable();
  test_write_before_begin_rejected();
  test_end_before_begin_rejected();
  test_current_timestamp_before_begin_rejected();
  test_begin_while_in_progress_rejected();
  test_second_begin_after_end_starts_clean_frame();
  return 0;
}
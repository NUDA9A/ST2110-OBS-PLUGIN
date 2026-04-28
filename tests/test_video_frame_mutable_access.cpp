#include <cassert>
#include <cstdint>
#include <stdexcept>

#include <st2110/video_frame.hpp>

static void test_video_frame_basic_properties() {
  st2110::VideoFrame frame(1920, 1080, st2110::PixelFormat::UYVY);

  assert(frame.width() == 1920);
  assert(frame.height() == 1080);
  assert(frame.format() == st2110::PixelFormat::UYVY);
  assert(frame.stride_bytes() == 1920u * 2u);
  assert(frame.size_bytes() == 1920u * 2u * 1080u);
}

static void test_data_and_row_data_point_into_same_storage() {
  st2110::VideoFrame frame(8, 3, st2110::PixelFormat::UYVY);

  assert(frame.stride_bytes() == 16);

  uint8_t *base = frame.data();
  uint8_t *row0 = frame.row_data(0);
  uint8_t *row1 = frame.row_data(1);
  uint8_t *row2 = frame.row_data(2);

  assert(base != nullptr);
  assert(row0 == base);
  assert(row1 == base + 16);
  assert(row2 == base + 32);

  row0[0] = 0x10;
  row1[1] = 0x21;
  row2[2] = 0x32;

  assert(frame.data()[0] == 0x10);
  assert(frame.data()[16 + 1] == 0x21);
  assert(frame.data()[32 + 2] == 0x32);
}

static void test_const_accessors_work() {
  st2110::VideoFrame frame(6, 2, st2110::PixelFormat::UYVY);
  frame.data()[0] = 0xAA;
  frame.row_data(1)[3] = 0xBB;

  const st2110::VideoFrame &cref = frame;

  assert(cref.data()[0] == 0xAA);
  assert(cref.row_data(1)[3] == 0xBB);
  assert(cref.stride_bytes() == 12);
}

static void test_plane_out_of_range_rejected() {
  st2110::VideoFrame frame(8, 2, st2110::PixelFormat::UYVY);

  bool thrown1 = false;
  try {
    (void)frame.data(1);
  } catch (const std::out_of_range &) {
    thrown1 = true;
  }
  assert(thrown1);

  bool thrown2 = false;
  try {
    (void)frame.stride_bytes(1);
  } catch (const std::out_of_range &) {
    thrown2 = true;
  }
  assert(thrown2);

  bool thrown3 = false;
  try {
    (void)frame.row_data(0, 1);
  } catch (const std::out_of_range &) {
    thrown3 = true;
  }
  assert(thrown3);
}

static void test_row_out_of_range_rejected() {
  st2110::VideoFrame frame(8, 2, st2110::PixelFormat::UYVY);

  bool thrown = false;
  try {
    (void)frame.row_data(2);
  } catch (const std::out_of_range &) {
    thrown = true;
  }
  assert(thrown);
}

static void test_view_reflects_mutated_storage() {
  st2110::VideoFrame frame(10, 4, st2110::PixelFormat::UYVY);

  frame.data()[0] = 0xAB;
  frame.row_data(1)[5] = 0xCD;

  const st2110::VideoFrameView view = frame.view(123456);

  assert(view.format == st2110::PixelFormat::UYVY);
  assert(view.width == 10);
  assert(view.height == 4);
  assert(view.timestamp_ns == 123456);

  assert(view.data[0] == frame.data());
  assert(view.stride[0] == 20);

  assert(view.data[1] == nullptr);
  assert(view.data[2] == nullptr);
  assert(view.data[3] == nullptr);

  assert(view.stride[1] == 0);
  assert(view.stride[2] == 0);
  assert(view.stride[3] == 0);

  assert(view.data[0][0] == 0xAB);
  assert(view.data[0][20 + 5] == 0xCD);
}

int main() {
  test_video_frame_basic_properties();
  test_data_and_row_data_point_into_same_storage();
  test_const_accessors_work();
  test_plane_out_of_range_rejected();
  test_row_out_of_range_rejected();
  test_view_reflects_mutated_storage();
  return 0;
}
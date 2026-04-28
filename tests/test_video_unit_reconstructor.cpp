#include <cassert>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include <st2110/video_unit_reconstructor.hpp>
#include <st2110/frame_assembler.hpp>
#include <st2110/video_scan_mode.hpp>
#include <st2110/pixel_format.hpp>

static st2110::AssembledVideoUnit make_unit(st2110::VideoAssemblyUnitKind kind, uint32_t rtp_timestamp, bool complete,
                                            const uint8_t *bytes, std::size_t size_bytes) {
  st2110::VideoFrame frame(4, 1, st2110::PixelFormat::UYVY);
  uint8_t *row0 = frame.row_data(0);
  for (std::size_t i = 0; i < size_bytes; ++i) {
    row0[i] = bytes[i];
  }

  st2110::AssembledVideoUnit unit{.frame = std::move(frame),
                                  .unit_kind = kind,
                                  .rtp_timestamp = rtp_timestamp,
                                  .marker_seen = true,
                                  .can_emit = true,
                                  .complete = complete};
  return unit;
}

static st2110::VideoUnitReconstructorConfig make_cfg(st2110::VideoScanMode mode) {
  st2110::VideoUnitReconstructorConfig cfg{};
  cfg.format = st2110::PixelFormat::UYVY;
  cfg.scan_mode = mode;
  return cfg;
}

static void test_factory_returns_progressive_reconstructor() {
  auto reconstructor = st2110::make_video_unit_reconstructor(make_cfg(st2110::VideoScanMode::Progressive));

  assert(reconstructor.has_value());
  assert(static_cast<bool>(*reconstructor));
}

static void test_factory_rejects_interlaced_for_now() {
  auto reconstructor = st2110::make_video_unit_reconstructor(make_cfg(st2110::VideoScanMode::Interlaced));

  assert(!reconstructor.has_value());
  assert(reconstructor.error() == st2110::Error::Unsupported);
}

static void test_factory_rejects_psf_for_now() {
  auto reconstructor = st2110::make_video_unit_reconstructor(make_cfg(st2110::VideoScanMode::PsF));

  assert(!reconstructor.has_value());
  assert(reconstructor.error() == st2110::Error::Unsupported);
}

static void test_progressive_reconstructor_passes_through_complete_frame_unit() {
  auto reconstructor_expected = st2110::make_video_unit_reconstructor(make_cfg(st2110::VideoScanMode::Progressive));
  assert(reconstructor_expected.has_value());

  static const uint8_t bytes[] = {0, 1, 2, 3, 4, 5, 6, 7};
  auto unit = make_unit(st2110::VideoAssemblyUnitKind::Frame, 1000, true, bytes, sizeof(bytes));

  auto out = (*reconstructor_expected)->push(std::move(unit));

  assert(out.size() == 1u);
  assert(out[0].rtp_timestamp == 1000u);
  assert(out[0].complete == true);
  assert(out[0].partial() == false);

  const uint8_t *row0 = out[0].frame.row_data(0);
  for (int i = 0; i < 8; ++i) {
    assert(row0[i] == static_cast<uint8_t>(i));
  }
}

static void test_progressive_reconstructor_passes_through_partial_frame_unit() {
  auto reconstructor_expected = st2110::make_video_unit_reconstructor(make_cfg(st2110::VideoScanMode::Progressive));
  assert(reconstructor_expected.has_value());

  static const uint8_t bytes[] = {10, 11, 12, 13};
  auto unit = make_unit(st2110::VideoAssemblyUnitKind::Frame, 2000, false, bytes, sizeof(bytes));

  auto out = (*reconstructor_expected)->push(std::move(unit));

  assert(out.size() == 1u);
  assert(out[0].rtp_timestamp == 2000u);
  assert(out[0].complete == false);
  assert(out[0].partial() == true);

  const uint8_t *row0 = out[0].frame.row_data(0);
  assert(row0[0] == 10);
  assert(row0[1] == 11);
  assert(row0[2] == 12);
  assert(row0[3] == 13);
}

static void test_progressive_reconstructor_rejects_wrong_unit_kind() {
  auto reconstructor_expected = st2110::make_video_unit_reconstructor(make_cfg(st2110::VideoScanMode::Progressive));
  assert(reconstructor_expected.has_value());

  static const uint8_t bytes[] = {1, 2, 3, 4};
  auto unit = make_unit(st2110::VideoAssemblyUnitKind::Field, 3000, true, bytes, sizeof(bytes));

  bool threw = false;
  try {
    (void)(*reconstructor_expected)->push(std::move(unit));
  } catch (const std::logic_error &) {
    threw = true;
  }

  assert(threw);
}

static void test_progressive_reconstructor_reset_is_callable() {
  auto reconstructor_expected = st2110::make_video_unit_reconstructor(make_cfg(st2110::VideoScanMode::Progressive));
  assert(reconstructor_expected.has_value());

  (*reconstructor_expected)->reset();

  static const uint8_t bytes[] = {7, 6, 5, 4};
  auto unit = make_unit(st2110::VideoAssemblyUnitKind::Frame, 4000, true, bytes, sizeof(bytes));

  auto out = (*reconstructor_expected)->push(std::move(unit));
  assert(out.size() == 1u);
  assert(out[0].rtp_timestamp == 4000u);
  assert(out[0].complete == true);
}

int main() {
  test_factory_returns_progressive_reconstructor();
  test_factory_rejects_interlaced_for_now();
  test_factory_rejects_psf_for_now();
  test_progressive_reconstructor_passes_through_complete_frame_unit();
  test_progressive_reconstructor_passes_through_partial_frame_unit();
  test_progressive_reconstructor_rejects_wrong_unit_kind();
  test_progressive_reconstructor_reset_is_callable();
  return 0;
}
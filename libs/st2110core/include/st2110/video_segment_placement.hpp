#ifndef ST2110_OBS_PLUGIN_VIDEO_SEGMENT_PLACEMENT_HPP
#define ST2110_OBS_PLUGIN_VIDEO_SEGMENT_PLACEMENT_HPP

#include "bytes.hpp"
#include "error.hpp"
#include "pixel_format.hpp"
#include "video_scan_mode.hpp"
#include "packet_view.hpp"
#include "video_segment_constraints.hpp"
#include "video_packing_mode.hpp"

#include <expected>

namespace st2110 {
struct VideoFrameWriteOp {
  std::size_t plane = 0;
  uint32_t row = 0;
  std::size_t byte_offset = 0;
  ByteSpan bytes{};
};

[[nodiscard]] inline std::expected<VideoFrameWriteOp, Error>
map_progressive_segment_to_frame_write(PixelFormat format, const SrdSegmentView &segment) {
  if (Error err = validate_video_segment_for_format(format, segment.header, segment.data); err != Error::Ok) {
    return std::unexpected(err);
  }
  switch (format) {
  case PixelFormat::UYVY:
    return VideoFrameWriteOp{.plane = 0,
                             .row = segment.header.row_number,
                             .byte_offset = static_cast<std::size_t>(segment.header.offset) * 2uz,
                             .bytes = segment.data};
  default:
    return std::unexpected(Error::Unsupported);
  }
}

[[nodiscard]] inline std::expected<VideoFrameWriteOp, Error>
map_interlaced_segment_to_frame_write(PixelFormat format, const SrdSegmentView &segment) {
  return std::unexpected(Error::Unsupported);
}

[[nodiscard]] inline std::expected<VideoFrameWriteOp, Error>
map_psf_segment_to_frame_write(PixelFormat format, const SrdSegmentView &segment) {
  return std::unexpected(Error::Unsupported);
}

inline std::expected<VideoFrameWriteOp, Error>
map_gpm_video_segment_to_frame_write(PixelFormat format, VideoScanMode scan_mode, const SrdSegmentView &segment) {
  switch (scan_mode) {
  case VideoScanMode::Progressive:
    return map_progressive_segment_to_frame_write(format, segment);
  case VideoScanMode::Interlaced:
    return map_interlaced_segment_to_frame_write(format, segment);
  case VideoScanMode::PsF:
    return map_psf_segment_to_frame_write(format, segment);
  default:
    return std::unexpected(Error::InvalidValue);
  }
}

inline std::expected<VideoFrameWriteOp, Error>
map_bpm_video_segment_to_frame_write(PixelFormat format, VideoScanMode scan_mode, const SrdSegmentView &segment) {
  (void)format;
  (void)scan_mode;
  (void)segment;
  return std::unexpected(Error::Unsupported);
}

inline std::expected<VideoFrameWriteOp, Error> map_video_segment_to_frame_write(VideoPackingMode packing_mode,
                                                                                PixelFormat format,
                                                                                VideoScanMode scan_mode,
                                                                                const SrdSegmentView &segment) {
  switch (packing_mode) {
  case VideoPackingMode::Gpm:
    return map_gpm_video_segment_to_frame_write(format, scan_mode, segment);
  case VideoPackingMode::Bpm:
    return map_bpm_video_segment_to_frame_write(format, scan_mode, segment);
  default:
    return std::unexpected(Error::InvalidValue);
  }
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_VIDEO_SEGMENT_PLACEMENT_HPP

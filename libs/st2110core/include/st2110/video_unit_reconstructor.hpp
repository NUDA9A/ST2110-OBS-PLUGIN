#ifndef ST2110_OBS_PLUGIN_VIDEO_UNIT_RECONSTRUCTOR_HPP
#define ST2110_OBS_PLUGIN_VIDEO_UNIT_RECONSTRUCTOR_HPP

#include "video_frame.hpp"
#include "pixel_format.hpp"
#include "video_scan_mode.hpp"
#include "frame_assembler.hpp"
#include "error.hpp"

#include <vector>
#include <expected>
#include <memory>
#include <stdexcept>

namespace st2110 {
struct ReconstructedVideoFrame {
  VideoFrame frame;
  uint32_t rtp_timestamp = 0;
  bool complete = false;

  [[nodiscard]] bool partial() const { return !complete; }
};

struct VideoUnitReconstructorConfig {
  PixelFormat format = PixelFormat::UYVY;
  VideoScanMode scan_mode = VideoScanMode::Progressive;
};

class IVideoUnitReconstructor {
public:
  virtual std::vector<ReconstructedVideoFrame> push(AssembledVideoUnit unit) = 0;
  virtual void reset() = 0;
  virtual ~IVideoUnitReconstructor() = default;
};

class ProgressiveVideoUnitReconstructor : public IVideoUnitReconstructor {
public:
  ProgressiveVideoUnitReconstructor() = default;
  std::vector<ReconstructedVideoFrame> push(AssembledVideoUnit unit) override {
    if (unit.unit_kind != VideoAssemblyUnitKind::Frame) {
      throw std::logic_error("Unsupported");
    }
    return {ReconstructedVideoFrame{
        .frame = std::move(unit.frame), .rtp_timestamp = unit.rtp_timestamp, .complete = unit.complete}};
  }

  void reset() override {}

  ~ProgressiveVideoUnitReconstructor() override = default;
};

[[nodiscard]] inline std::expected<std::unique_ptr<IVideoUnitReconstructor>, Error>
make_video_unit_reconstructor(const VideoUnitReconstructorConfig &cfg) {
  switch (cfg.scan_mode) {
  case VideoScanMode::Progressive: {
    std::unique_ptr<IVideoUnitReconstructor> ptr = std::make_unique<ProgressiveVideoUnitReconstructor>();
    return ptr;
  }
  case VideoScanMode::Interlaced:
  case VideoScanMode::PsF:
    return std::unexpected(Error::Unsupported);
  default:
    return std::unexpected(Error::InvalidValue);
  }
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_VIDEO_UNIT_RECONSTRUCTOR_HPP

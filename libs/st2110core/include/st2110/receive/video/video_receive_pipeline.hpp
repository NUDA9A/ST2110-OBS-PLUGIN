#ifndef ST2110_OBS_PLUGIN_VIDEO_RECEIVE_PIPELINE_HPP
#define ST2110_OBS_PLUGIN_VIDEO_RECEIVE_PIPELINE_HPP

#include <st2110/receive/video/depacketizer.hpp>
#include <st2110/receive/video/video_unit_reconstructor.hpp>
#include <st2110/contracts/video/video_receice_pipeline_config.hpp>

#include <memory>
#include <vector>

namespace st2110 {
class VideoReceivePipeline {
  public:
    explicit VideoReceivePipeline(const VideoReceivePipelineConfig &cfg)
        : depacketizer_(cfg.depacketizer), reconstructor_(cfg.reconstructor) {}

    [[nodiscard]] std::vector<ReconstructedVideoFrame> push(std::unique_ptr<VideoPacketView> packet) {
        std::vector<ReconstructedVideoFrame> res;
        auto units = depacketizer_.push(std::move(packet));

        for (auto &unit : units) {
            auto frame = reconstructor_.push(std::move(unit));
            if (frame) {
                res.push_back(std::move(*frame));
            }
        }
        return res;
    }

    [[nodiscard]] const DepacketizerStats &depacketizer_stats() const { return depacketizer_.stats(); }

  private:
    Depacketizer depacketizer_;
    VideoUnitReconstructor reconstructor_;
};
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_VIDEO_RECEIVE_PIPELINE_HPP

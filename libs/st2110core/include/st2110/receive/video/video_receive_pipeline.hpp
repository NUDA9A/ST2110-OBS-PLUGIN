#ifndef ST2110_OBS_PLUGIN_VIDEO_RECEIVE_PIPELINE_HPP
#define ST2110_OBS_PLUGIN_VIDEO_RECEIVE_PIPELINE_HPP

#include "st2110/ingress/shared/packet_view.hpp"
#include "depacketizer.hpp"
#include "video_unit_reconstructor.hpp"

#include <expected>
#include <memory>
#include <stdexcept>
#include <vector>

namespace st2110 {
struct VideoReceivePipelineConfig {
    DepacketizerConfig depacketizer{};
    VideoUnitReconstructorConfig reconstructor{};
};

class VideoReceivePipeline {
  public:
    explicit VideoReceivePipeline(const VideoReceivePipelineConfig &cfg) : depacketizer_(cfg.depacketizer) {
        if (cfg.reconstructor.format != cfg.depacketizer.format) {
            throw std::invalid_argument("reconstructor.format != depacketizer.format");
        }
        if (cfg.reconstructor.scan_mode != cfg.depacketizer.scan_mode) {
            throw std::invalid_argument("reconstructor.scan_mode != depacketizer.scan_mode");
        }
        auto reconstructor_expected = make_video_unit_reconstructor(cfg.reconstructor);
        if (!reconstructor_expected.has_value()) {
            Error err = reconstructor_expected.error();
            if (err == Error::Unsupported) {
                throw std::logic_error("Unsupported video unit reconstructor configuration");
            }
            if (err == Error::InvalidValue) {
                throw std::invalid_argument("Invalid video unit reconstructor configuration");
            }
            throw std::logic_error("Failed to construct video unit reconstructor");
        }
        reconstructor_ = std::move(*reconstructor_expected);
    }

    [[nodiscard]] std::vector<ReconstructedVideoFrame> push(const PacketView &packet) {
        std::vector<ReconstructedVideoFrame> res;
        auto units = depacketizer_.push(packet);

        for (auto &unit : units) {
            auto frames = reconstructor_->push(std::move(unit));
            for (auto &frame : frames) {
                res.push_back(std::move(frame));
            }
        }
        return res;
    }

    void reset() {
        depacketizer_.reset();
        reconstructor_->reset();
    }

    [[nodiscard]] const DepacketizerStats &depacketizer_stats() const { return depacketizer_.stats(); }

  private:
    Depacketizer depacketizer_;
    std::unique_ptr<IVideoUnitReconstructor> reconstructor_;
};
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_VIDEO_RECEIVE_PIPELINE_HPP

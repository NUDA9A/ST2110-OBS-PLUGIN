#ifndef ST2110_OBS_PLUGIN_FRAME_ASSEMBLER_HPP
#define ST2110_OBS_PLUGIN_FRAME_ASSEMBLER_HPP

#include "video_frame.hpp"
#include "bytes.hpp"

#include <cstring>

namespace st2110 {
    struct AssembledVideoFrame {
        VideoFrame frame;
        uint32_t rtp_timestamp = 0;
        bool marker_seen = false;
    };

    class FrameAssembler {
    public:
        FrameAssembler(uint32_t width, uint32_t height, PixelFormat format) : width_(width), height_(height), format_(format),
                                                                              current_frame_(width_, height_, format_) {}

        void begin(uint32_t rtp_timestamp) {
            if (in_progress_) {
                throw std::logic_error("FrameAssembler is currently in progress and begin() was called");
            }
            current_rtp_timestamp_ = rtp_timestamp;
            in_progress_ = true;
        }

        void write_segment(uint32_t row, std::size_t byte_offset, ByteSpan bytes) {
            if (!in_progress_) {
                throw std::logic_error("begin() wasn't called before write_segment()");
            }
            auto* start = current_frame_.row_data(row);
            start += byte_offset;
            std::memcpy(start, bytes.data(), bytes.size());
        }

        [[nodiscard]] AssembledVideoFrame end(bool marker) {
            if (!in_progress_) {
                throw std::logic_error("end() was called while Assembler is not in progress");
            }

            in_progress_ = false;
            AssembledVideoFrame res{std::move(current_frame_), current_rtp_timestamp_, marker};
            current_frame_ = VideoFrame(width_, height_, format_);
            return res;
        }

        [[nodiscard]] bool in_progress() const {
            return in_progress_;
        }

        [[nodiscard]] uint32_t current_rtp_timestamp() const {
            if (!in_progress_) {
                throw std::logic_error("Assembler is not currently in progress");
            }
            return current_rtp_timestamp_;
        }

    private:
        uint32_t width_;
        uint32_t height_;
        PixelFormat format_;

        bool in_progress_ = false;
        uint32_t current_rtp_timestamp_ = 0;
        VideoFrame current_frame_;
    };
}

#endif //ST2110_OBS_PLUGIN_FRAME_ASSEMBLER_HPP

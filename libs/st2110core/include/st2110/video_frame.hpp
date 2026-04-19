#ifndef ST2110_OBS_PLUGIN_VIDEO_FRAME_HPP
#define ST2110_OBS_PLUGIN_VIDEO_FRAME_HPP

#include "pixel_format.hpp"
#include <cstdint>
#include <vector>
#include <algorithm>
#include <stdexcept>

namespace st2110 {
    struct VideoFrameView {
        PixelFormat format;
        uint32_t width;
        uint32_t height;
        const uint8_t *data[4];
        std::size_t stride[4];
        uint64_t timestamp_ns;
    };

    struct Plane {
        std::size_t offset_bytes;
        std::size_t stride_bytes;
        std::size_t active_row_bytes;
        std::size_t height_rows;
    };

    class VideoFrame {
    public:
        VideoFrame(uint32_t w, uint32_t h, PixelFormat fmt) : width_(w), height_(h), fmt_(fmt) {
            fill_planes();
            std::size_t total_size = 0;
            for (std::size_t i = 0; i < planes_count_; ++i) {
                total_size = std::max(total_size,
                                      planes_[i].offset_bytes + planes_[i].stride_bytes * planes_[i].height_rows);
            }

            frame_data.resize(total_size);
        }

        VideoFrameView view(uint64_t timestamp_ns = 0) const {
            VideoFrameView res{
                    .format = fmt_,
                    .width = width_,
                    .height = height_,
                    .data = {nullptr, nullptr, nullptr, nullptr},
                    .stride = {0, 0, 0, 0},
                    .timestamp_ns = timestamp_ns
            };
            for (std::size_t i = 0; i < planes_count_; ++i) {
                res.data[i] = frame_data.data() + planes_[i].offset_bytes;
                res.stride[i] = planes_[i].stride_bytes;
            }

            return res;
        }

        [[nodiscard]] std::size_t size_bytes() const {
            return frame_data.size();
        }

        [[nodiscard]] uint32_t width() const {
            return width_;
        }

        [[nodiscard]] uint32_t height() const {
            return height_;
        }

        [[nodiscard]] PixelFormat format() const {
            return fmt_;
        }

        [[nodiscard]] std::size_t stride_bytes(std::size_t plane = 0) const {
            if (plane >= planes_count_) {
                throw std::out_of_range("invalid plane value");
            }
            return planes_[plane].stride_bytes;
        }

        [[nodiscard]] uint8_t* data(std::size_t plane = 0) {
            if (plane >= planes_count_) {
                throw std::out_of_range("invalid plane value");
            }
            return frame_data.data() + planes_[plane].offset_bytes;
        }

        [[nodiscard]] const uint8_t* data(std::size_t plane = 0) const {
            if (plane >= planes_count_) {
                throw std::out_of_range("invalid plane value");
            }
            return frame_data.data() + planes_[plane].offset_bytes;
        }

        [[nodiscard]] uint8_t* row_data(uint32_t row, std::size_t plane = 0) {
            if (plane >= planes_count_) {
                throw std::out_of_range("invalid plane value");
            }
            if (row >= planes_[plane].height_rows) {
                throw std::out_of_range("invalid row value");
            }

            return data(plane) + row * planes_[plane].stride_bytes;
        }

        [[nodiscard]] const uint8_t* row_data(uint32_t row, std::size_t plane = 0) const {
            if (plane >= planes_count_) {
                throw std::out_of_range("invalid plane value");
            }
            if (row >= planes_[plane].height_rows) {
                throw std::out_of_range("invalid row value");
            }

            return data(plane) + row * planes_[plane].stride_bytes;
        }

        [[nodiscard]] std::size_t plane_count() const {
            return planes_count_;
        }

        [[nodiscard]] std::size_t active_row_bytes(std::size_t plane = 0) const {
            if (plane >= planes_count_) {
                throw std::out_of_range("plane value is invalid");
            }
            return planes_[plane].active_row_bytes;
        }

        [[nodiscard]] std::size_t plane_height_rows(std::size_t plane = 0) const {
            if (plane >= planes_count_) {
                throw std::out_of_range("plane value is invalid");
            }
            return planes_[plane].height_rows;
        }

    private:
        void fill_planes() {
            switch (fmt_) {
                case PixelFormat::UYVY: {
                    if (width_ % 2 != 0 || width_ == 0 || height_ == 0) {
                        throw std::invalid_argument("Invalid width/height value");
                    }
                    planes_count_ = 1;
                    planes_[0].offset_bytes = 0;
                    planes_[0].active_row_bytes = width_ * 2;
                    planes_[0].stride_bytes = planes_[0].active_row_bytes;
                    planes_[0].height_rows = height_;
                    break;
                }
                default:
                    throw std::invalid_argument("Unknown format");
            }
        }

        uint32_t width_;
        uint32_t height_;
        PixelFormat fmt_;

        std::vector<uint8_t> frame_data;

        Plane planes_[4]{};
        uint8_t planes_count_{};
    };
}

#endif //ST2110_OBS_PLUGIN_VIDEO_FRAME_HPP

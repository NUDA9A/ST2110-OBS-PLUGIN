#ifndef ST2110_OBS_PLUGIN_VIDEO_FRAME_HPP
#define ST2110_OBS_PLUGIN_VIDEO_FRAME_HPP

#include <st2110/delivery/video/pixel_format.hpp>
#include <st2110/foundation/timestamp.hpp>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace st2110 {
struct VideoFrameView {
    PixelFormat format;
    std::uint32_t width;
    std::uint32_t height;
    const std::uint8_t *data[4];
    std::size_t stride[4];
    TimestampNs timestamp_ns;
};

struct Plane {
    std::size_t offset_bytes;
    std::size_t stride_bytes;
    std::size_t active_row_bytes;
    std::size_t height_rows;
};

class VideoFrame {
  public:
    VideoFrame(std::uint32_t w, std::uint32_t h, PixelFormat fmt) : width_(w), height_(h), fmt_(fmt) {
        fill_planes();
        std::size_t total_size = 0;
        for (std::size_t i = 0; i < planes_count_; ++i) {
            total_size =
                std::max(total_size, planes_[i].offset_bytes + planes_[i].stride_bytes * planes_[i].height_rows);
        }

        frame_data.resize(total_size);
    }

    [[nodiscard]] VideoFrameView view(TimestampNs timestamp_ns = 0) const {
        VideoFrameView res{.format = fmt_,
                           .width = width_,
                           .height = height_,
                           .data = {nullptr, nullptr, nullptr, nullptr},
                           .stride = {0, 0, 0, 0},
                           .timestamp_ns = timestamp_ns};
        for (std::size_t i = 0; i < planes_count_; ++i) {
            res.data[i] = frame_data.data() + planes_[i].offset_bytes;
            res.stride[i] = planes_[i].stride_bytes;
        }

        return res;
    }

    [[nodiscard]] std::size_t size_bytes() const { return frame_data.size(); }

    [[nodiscard]] std::uint32_t width() const { return width_; }

    [[nodiscard]] std::uint32_t height() const { return height_; }

    [[nodiscard]] PixelFormat format() const { return fmt_; }

    [[nodiscard]] std::size_t stride_bytes(std::size_t plane = 0) const {
        if (plane >= planes_count_) {
            throw std::out_of_range("invalid plane value");
        }
        return planes_[plane].stride_bytes;
    }

    [[nodiscard]] std::uint8_t *data(std::size_t plane = 0) {
        if (plane >= planes_count_) {
            throw std::out_of_range("invalid plane value");
        }
        return frame_data.data() + planes_[plane].offset_bytes;
    }

    [[nodiscard]] const std::uint8_t *data(std::size_t plane = 0) const {
        if (plane >= planes_count_) {
            throw std::out_of_range("invalid plane value");
        }
        return frame_data.data() + planes_[plane].offset_bytes;
    }

    [[nodiscard]] std::uint8_t *row_data(std::uint32_t row, std::size_t plane = 0) {
        if (plane >= planes_count_) {
            throw std::out_of_range("invalid plane value");
        }
        if (row >= planes_[plane].height_rows) {
            throw std::out_of_range("invalid row value");
        }

        return data(plane) + row * planes_[plane].stride_bytes;
    }

    [[nodiscard]] const std::uint8_t *row_data(std::uint32_t row, std::size_t plane = 0) const {
        if (plane >= planes_count_) {
            throw std::out_of_range("invalid plane value");
        }
        if (row >= planes_[plane].height_rows) {
            throw std::out_of_range("invalid row value");
        }

        return data(plane) + row * planes_[plane].stride_bytes;
    }

    [[nodiscard]] std::size_t plane_count() const { return planes_count_; }

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
        const auto configure_single_plane = [this](const std::size_t active_row_bytes) {
            planes_count_ = 1;
            planes_[0].offset_bytes = 0;
            planes_[0].active_row_bytes = active_row_bytes;
            planes_[0].stride_bytes = active_row_bytes;
            planes_[0].height_rows = height_;
        };

        if (width_ == 0 || height_ == 0) {
            throw std::invalid_argument("Invalid width/height value");
        }

        switch (fmt_) {
        case PixelFormat::UYVY: {
            if (width_ % 2 != 0) {
                throw std::invalid_argument("Invalid width value for UYVY");
            }

            configure_single_plane(static_cast<std::size_t>(width_) * 2uz);
            break;
        }

        case PixelFormat::RGB8: {
            configure_single_plane(static_cast<std::size_t>(width_) * 3uz);
            break;
        }

        case PixelFormat::YUV422RFC4175PG2BE10: {
            if (width_ % 2 != 0) {
                throw std::invalid_argument("Invalid width value for YUV422RFC4175PG2BE10");
            }

            configure_single_plane((static_cast<std::size_t>(width_) / 2uz) * 5uz);
            break;
        }

        case PixelFormat::YUV422RFC4175PG2BE12: {
            if (width_ % 2 != 0) {
                throw std::invalid_argument("Invalid width value for YUV422RFC4175PG2BE12");
            }

            configure_single_plane((static_cast<std::size_t>(width_) / 2uz) * 6uz);
            break;
        }

        case PixelFormat::YUV444RFC4175PG4BE10:
        case PixelFormat::RGBRFC4175PG4BE10: {
            if (width_ % 4 != 0) {
                throw std::invalid_argument("Invalid width value for RFC4175PG4BE10");
            }

            configure_single_plane((static_cast<std::size_t>(width_) / 4uz) * 15uz);
            break;
        }

        case PixelFormat::YUV444RFC4175PG2BE12:
        case PixelFormat::RGBRFC4175PG2BE12: {
            if (width_ % 2 != 0) {
                throw std::invalid_argument("Invalid width value for RFC4175PG2BE12");
            }

            configure_single_plane((static_cast<std::size_t>(width_) / 2uz) * 9uz);
            break;
        }

        default:
            throw std::invalid_argument("Unknown format");
        }
    }

    std::uint32_t width_;
    std::uint32_t height_;
    PixelFormat fmt_;

    std::vector<std::uint8_t> frame_data;

    Plane planes_[4]{};
    std::uint8_t planes_count_{};
};
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_VIDEO_FRAME_HPP

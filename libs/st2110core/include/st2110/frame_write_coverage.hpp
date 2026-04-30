#ifndef ST2110_OBS_PLUGIN_FRAME_WRITE_COVERAGE_HPP
#define ST2110_OBS_PLUGIN_FRAME_WRITE_COVERAGE_HPP

#include "video_frame.hpp"

#include <vector>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace st2110 {
struct PlaneWriteCoverage {
    std::size_t active_row_bytes = 0;
    std::size_t height_rows = 0;
    std::size_t expected_bytes = 0;
    std::size_t written_unique_bytes = 0;
    std::vector<uint8_t> written{};
};

class FrameWriteCoverage {
  public:
    FrameWriteCoverage() = default;
    explicit FrameWriteCoverage(const VideoFrame &frame) { reset_from(frame); }

    void reset_from(const VideoFrame &frame) {
        planes_.clear();
        total_expected_bytes_ = 0;
        total_written_unique_bytes_ = 0;

        std::size_t planes_count = frame.plane_count();
        planes_.reserve(planes_count);

        for (std::size_t i = 0; i < planes_count; ++i) {
            auto active_row_bytes = frame.active_row_bytes(i);
            auto height_rows = frame.plane_height_rows(i);
            auto expected_bytes = active_row_bytes * height_rows;

            total_expected_bytes_ += expected_bytes;
            planes_.push_back(
                {.active_row_bytes = active_row_bytes, .height_rows = height_rows, .expected_bytes = expected_bytes});
            planes_.back().written.resize(expected_bytes);
        }
    }

    void mark_written(std::size_t plane, uint32_t row, std::size_t byte_offset, std::size_t length) {
        if (plane >= planes_.size()) {
            throw std::out_of_range("plane value is invalid");
        }
        if (row >= planes_[plane].height_rows) {
            throw std::out_of_range("row value is invalid");
        }
        auto active_row_bytes = planes_[plane].active_row_bytes;
        if (byte_offset > active_row_bytes) {
            throw std::out_of_range("byte_offset > active_row_bytes");
        }
        if (length > active_row_bytes - byte_offset) {
            throw std::out_of_range("length + byte_offset > active_row_bytes");
        }

        for (std::size_t i = 0; i < length; ++i) {
            if (uint8_t &byte = planes_[plane].written[row * active_row_bytes + byte_offset + i]; byte == uint8_t{}) {
                byte = uint8_t{1};
                ++planes_[plane].written_unique_bytes;
                ++total_written_unique_bytes_;
            }
        }
    }

    [[nodiscard]] bool is_complete() const {
        return total_expected_bytes_ != 0 && total_written_unique_bytes_ == total_expected_bytes_;
    }

    [[nodiscard]] std::size_t total_expected_bytes() const { return total_expected_bytes_; }

    [[nodiscard]] std::size_t total_written_unique_bytes() const { return total_written_unique_bytes_; }

    [[nodiscard]] std::size_t plane_count() const { return planes_.size(); }

    [[nodiscard]] std::size_t plane_expected_bytes(std::size_t plane) const {
        if (plane >= planes_.size()) {
            throw std::out_of_range("plane value is invalid");
        }
        return planes_[plane].expected_bytes;
    }

    [[nodiscard]] std::size_t plane_written_unique_bytes(std::size_t plane) const {
        if (plane >= planes_.size()) {
            throw std::out_of_range("plane value is invalid");
        }
        return planes_[plane].written_unique_bytes;
    }

  private:
    std::vector<PlaneWriteCoverage> planes_{};
    std::size_t total_expected_bytes_ = 0;
    std::size_t total_written_unique_bytes_ = 0;
};
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_FRAME_WRITE_COVERAGE_HPP

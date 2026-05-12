#ifndef ST2110_OBS_PLUGIN_FRAME_WRITE_COVERAGE_HPP
#define ST2110_OBS_PLUGIN_FRAME_WRITE_COVERAGE_HPP

#include <st2110/delivery/video/video_frame.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace st2110 {
struct PlaneWriteCoverage {
    std::size_t active_row_bytes = 0;
    std::size_t height_rows = 0;
    std::size_t expected_bytes = 0;
    std::size_t written_unique_bytes = 0;
    std::vector<std::uint8_t> written{};
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

    void mark_written(std::size_t plane, std::uint32_t row, std::size_t byte_offset, std::size_t length) {
        auto active_row_bytes = planes_[plane].active_row_bytes;

        for (std::size_t i = 0; i < length; ++i) {
            if (std::uint8_t &byte = planes_[plane].written[row * active_row_bytes + byte_offset + i];
                byte == std::uint8_t{}) {
                byte = std::uint8_t{1};
                ++planes_[plane].written_unique_bytes;
                ++total_written_unique_bytes_;
            }
        }
    }

    [[nodiscard]] bool is_complete() const {
        return total_expected_bytes_ != 0 && total_written_unique_bytes_ == total_expected_bytes_;
    }

  private:
    std::vector<PlaneWriteCoverage> planes_{};
    std::size_t total_expected_bytes_ = 0;
    std::size_t total_written_unique_bytes_ = 0;
};
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_FRAME_WRITE_COVERAGE_HPP

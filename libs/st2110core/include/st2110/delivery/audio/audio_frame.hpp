#ifndef ST2110_OBS_PLUGIN_AUDIO_FRAME_HPP
#define ST2110_OBS_PLUGIN_AUDIO_FRAME_HPP

#include "st2110/foundation/timestamp.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace st2110 {
struct AudioFrameView {
    uint32_t sampling_rate_hz = 0;
    uint16_t channel_count = 0;
    uint32_t samples_per_channel = 0;

    const std::int32_t *samples = nullptr;
    std::size_t total_sample_count = 0;
    std::size_t sample_frame_stride = 0;
    std::size_t size_bytes = 0;

    TimestampNs timestamp_ns = 0;
};

class AudioBuffer {
  public:
    AudioBuffer(const std::uint32_t sampling_rate_hz, const std::uint16_t channel_count,
                const std::uint32_t samples_per_channel)
        : sampling_rate_hz_(sampling_rate_hz), samples_per_channel_(samples_per_channel), channel_count_(channel_count),
          samples_(checked_total_sample_count(channel_count, samples_per_channel)) {}

    [[nodiscard]] std::uint32_t sampling_rate_hz() const { return sampling_rate_hz_; }
    [[nodiscard]] std::uint16_t channel_count() const { return channel_count_; }
    [[nodiscard]] std::uint32_t samples_per_channel() const { return samples_per_channel_; }

    [[nodiscard]] std::size_t sample_frame_stride() const { return channel_count_; }

    [[nodiscard]] std::size_t total_sample_count() const { return samples_.size(); }

    [[nodiscard]] std::size_t size_bytes() const { return samples_.size() * sizeof(std::int32_t); }

    [[nodiscard]] std::int32_t *samples() { return samples_.data(); }
    [[nodiscard]] const std::int32_t *samples() const { return samples_.data(); }

    [[nodiscard]] std::int32_t &sample(const std::uint32_t sample_index, const std::uint16_t channel) {
        return samples_.at(linear_sample_index(sample_index, channel));
    }

    [[nodiscard]] const std::int32_t &sample(const std::uint32_t sample_index, const std::uint16_t channel) const {
        return samples_.at(linear_sample_index(sample_index, channel));
    }

    [[nodiscard]] AudioFrameView view(const TimestampNs timestamp_ns = 0) const {
        return AudioFrameView{
            .sampling_rate_hz = sampling_rate_hz_,
            .channel_count = channel_count_,
            .samples_per_channel = samples_per_channel_,
            .samples = samples(),
            .total_sample_count = total_sample_count(),
            .sample_frame_stride = sample_frame_stride(),
            .size_bytes = size_bytes(),
            .timestamp_ns = timestamp_ns,
        };
    }

  private:
    [[nodiscard]] static std::size_t checked_total_sample_count(const std::uint16_t channel_count,
                                                                const std::uint32_t samples_per_channel) {
        const auto channels = static_cast<std::size_t>(channel_count);
        const auto samples = static_cast<std::size_t>(samples_per_channel);

        if (channels != 0 && samples > std::numeric_limits<std::size_t>::max() / channels) {
            throw std::overflow_error("audio buffer sample count overflow");
        }

        return channels * samples;
    }

    [[nodiscard]] std::size_t linear_sample_index(const std::uint32_t sample_index, const std::uint16_t channel) const {
        if (sample_index >= samples_per_channel_) {
            throw std::out_of_range("audio sample index out of range");
        }

        if (channel >= channel_count_) {
            throw std::out_of_range("audio channel index out of range");
        }

        return static_cast<std::size_t>(sample_index) * static_cast<std::size_t>(channel_count_) +
               static_cast<std::size_t>(channel);
    }

    std::uint32_t sampling_rate_hz_{0};
    std::uint32_t samples_per_channel_{0};
    std::uint16_t channel_count_{0};
    std::vector<std::int32_t> samples_{};
};
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_AUDIO_FRAME_HPP
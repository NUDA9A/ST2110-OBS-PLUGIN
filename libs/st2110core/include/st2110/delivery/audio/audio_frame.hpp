#ifndef ST2110_OBS_PLUGIN_AUDIO_FRAME_HPP
#define ST2110_OBS_PLUGIN_AUDIO_FRAME_HPP

#include "st2110/foundation/timestamp.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace st2110 {
enum class AudioSampleStorageFormat { InterleavedS32 };

struct AudioFrameView {
    AudioSampleStorageFormat storage_format = AudioSampleStorageFormat::InterleavedS32;
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
    AudioBuffer(uint32_t sampling_rate_hz, uint16_t channel_count, uint32_t samples_per_channel,
                AudioSampleStorageFormat storage_format = AudioSampleStorageFormat::InterleavedS32)
        : sampling_rate_hz_(sampling_rate_hz), samples_per_channel_(samples_per_channel), channel_count_(channel_count),
          format_(storage_format),
          samples_(checked_total_sample_count(channel_count, samples_per_channel, storage_format)) {}

    explicit AudioBuffer(const RxAudioConfig &cfg,
                         AudioSampleStorageFormat storage_format = AudioSampleStorageFormat::InterleavedS32)
        : AudioBuffer(cfg.sampling_rate_hz, cfg.channel_count, cfg.samples_per_packet, storage_format) {}

    [[nodiscard]] AudioSampleStorageFormat storage_format() const { return format_; }

    [[nodiscard]] uint32_t sampling_rate_hz() const { return sampling_rate_hz_; }

    [[nodiscard]] uint16_t channel_count() const { return channel_count_; }

    [[nodiscard]] uint32_t samples_per_channel() const { return samples_per_channel_; }

    [[nodiscard]] std::size_t sample_frame_stride() const {
        switch (format_) {
        case AudioSampleStorageFormat::InterleavedS32:
            return channel_count_;

        default:
            throw std::invalid_argument("unsupported audio sample storage format");
        }
    }

    [[nodiscard]] std::size_t total_sample_count() const { return samples_.size(); }

    [[nodiscard]] std::size_t size_bytes() const { return total_sample_count() * sizeof(std::int32_t); }

    [[nodiscard]] std::int32_t *samples() { return samples_.data(); }

    [[nodiscard]] const std::int32_t *samples() const { return samples_.data(); }

    [[nodiscard]] std::int32_t &sample(uint32_t sample_index, uint16_t channel) {
        return samples_.at(linear_sample_index(sample_index, channel));
    }

    [[nodiscard]] const std::int32_t &sample(uint32_t sample_index, uint16_t channel) const {
        return samples_.at(linear_sample_index(sample_index, channel));
    }

    [[nodiscard]] AudioFrameView view(TimestampNs timestamp_ns = 0) const {
        return AudioFrameView{.storage_format = format_,
                              .sampling_rate_hz = sampling_rate_hz_,
                              .channel_count = channel_count_,
                              .samples_per_channel = samples_per_channel_,
                              .samples = samples(),
                              .total_sample_count = total_sample_count(),
                              .sample_frame_stride = sample_frame_stride(),
                              .size_bytes = size_bytes(),
                              .timestamp_ns = timestamp_ns};
    }

  private:
    [[nodiscard]] static std::size_t checked_total_sample_count(uint16_t channel_count, uint32_t samples_per_channel,
                                                                AudioSampleStorageFormat storage_format) {
        switch (storage_format) {
        case AudioSampleStorageFormat::InterleavedS32:
            break;

        default:
            throw std::invalid_argument("unsupported audio sample storage format");
        }

        const auto channels = static_cast<std::size_t>(channel_count);
        const auto samples = static_cast<std::size_t>(samples_per_channel);

        if (channels != 0 && samples > (std::numeric_limits<std::size_t>::max() / channels)) {
            throw std::overflow_error("audio buffer sample count overflow");
        }

        return channels * samples;
    }

    [[nodiscard]] std::size_t linear_sample_index(uint32_t sample_index, uint16_t channel) const {
        if (sample_index >= samples_per_channel_) {
            throw std::out_of_range("audio sample index out of range");
        }

        if (channel >= channel_count_) {
            throw std::out_of_range("audio channel index out of range");
        }

        return (static_cast<std::size_t>(sample_index) * static_cast<std::size_t>(channel_count_)) +
               static_cast<std::size_t>(channel);
    }

    uint32_t sampling_rate_hz_{0};
    uint32_t samples_per_channel_{0};
    uint16_t channel_count_{0};
    AudioSampleStorageFormat format_{AudioSampleStorageFormat::InterleavedS32};
    std::vector<std::int32_t> samples_;
};
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_AUDIO_FRAME_HPP
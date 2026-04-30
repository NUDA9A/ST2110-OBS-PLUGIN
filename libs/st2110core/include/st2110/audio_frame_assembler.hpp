#ifndef ST2110_OBS_PLUGIN_AUDIO_FRAME_ASSEMBLER_HPP
#define ST2110_OBS_PLUGIN_AUDIO_FRAME_ASSEMBLER_HPP

#include "audio_frame.hpp"
#include "audio_packet.hpp"
#include "bytes.hpp"
#include "error.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <utility>

namespace st2110 {
struct AssembledAudioBlock {
    AudioBuffer buffer;
    uint32_t rtp_timestamp = 0;
    uint16_t rtp_sequence_number = 0;
    bool rtp_marker = false;
    bool complete = true;
};

struct AudioFrameAssemblerConfig {
    AudioSampleStorageFormat storage_format = AudioSampleStorageFormat::InterleavedS32;
};

struct AudioFrameAssemblerStats {
    uint64_t packets_used = 0;
    uint64_t packets_rejected = 0;
    uint64_t blocks_emitted = 0;
};

[[nodiscard]] inline Error validate_audio_frame_assembler_config(const AudioFrameAssemblerConfig &cfg) {
    switch (cfg.storage_format) {
    case AudioSampleStorageFormat::InterleavedS32:
        return Error::Ok;
    }

    return Error::InvalidValue;
}

[[nodiscard]] inline std::expected<std::size_t, Error>
checked_audio_assembler_payload_size_bytes(uint32_t samples_per_channel, uint16_t channel_count,
                                           std::size_t wire_sample_bytes) {
    if (samples_per_channel == 0 || channel_count == 0 || wire_sample_bytes == 0) {
        return std::unexpected(Error::InvalidValue);
    }

    constexpr std::size_t max = std::numeric_limits<std::size_t>::max();

    const std::size_t samples = static_cast<std::size_t>(samples_per_channel);
    const std::size_t channels = static_cast<std::size_t>(channel_count);

    if (samples > max / channels) {
        return std::unexpected(Error::InvalidValue);
    }

    const std::size_t sample_values = samples * channels;
    if (sample_values > max / wire_sample_bytes) {
        return std::unexpected(Error::InvalidValue);
    }

    return sample_values * wire_sample_bytes;
}

[[nodiscard]] inline std::expected<int32_t, Error> decode_audio_pcm_wire_sample_to_s32(ByteSpan sample_bytes,
                                                                                       AudioPcmWireFormat wire_format) {
    switch (wire_format) {
    case AudioPcmWireFormat::L16: {
        if (sample_bytes.size() != 2) {
            return std::unexpected(Error::InvalidValue);
        }

        const uint32_t raw = (static_cast<uint32_t>(sample_bytes[0]) << 8U) | static_cast<uint32_t>(sample_bytes[1]);

        int32_t value = static_cast<int32_t>(raw);
        if ((raw & 0x8000U) != 0) {
            value -= 0x10000;
        }

        return value;
    }

    case AudioPcmWireFormat::L24: {
        if (sample_bytes.size() != 3) {
            return std::unexpected(Error::InvalidValue);
        }

        const uint32_t raw = (static_cast<uint32_t>(sample_bytes[0]) << 16U) |
                             (static_cast<uint32_t>(sample_bytes[1]) << 8U) | static_cast<uint32_t>(sample_bytes[2]);

        int32_t value = static_cast<int32_t>(raw);
        if ((raw & 0x800000U) != 0) {
            value -= 0x1000000;
        }

        return value;
    }
    }

    return std::unexpected(Error::InvalidValue);
}

class AudioFrameAssembler {
  public:
    explicit AudioFrameAssembler(AudioFrameAssemblerConfig cfg = {}) : cfg_(cfg) {}

    [[nodiscard]] std::expected<AssembledAudioBlock, Error> push(const AudioRtpPacketView &packet) {
        const auto reject = [this](Error error) -> std::expected<AssembledAudioBlock, Error> {
            ++stats_.packets_rejected;
            return std::unexpected(error);
        };

        const Error cfg_error = validate_audio_frame_assembler_config(cfg_);
        if (cfg_error != Error::Ok) {
            return reject(cfg_error);
        }

        if (packet.sampling_rate_hz == 0 || packet.channel_count == 0 || packet.samples_per_channel == 0) {
            return reject(Error::InvalidValue);
        }

        const auto wire_sample_bytes = audio_pcm_wire_sample_bytes(packet.wire_format);
        if (!wire_sample_bytes.has_value()) {
            return reject(wire_sample_bytes.error());
        }

        const auto expected_payload_size = checked_audio_assembler_payload_size_bytes(
            packet.samples_per_channel, packet.channel_count, static_cast<std::size_t>(*wire_sample_bytes));
        if (!expected_payload_size.has_value()) {
            return reject(expected_payload_size.error());
        }

        if (packet.payload.size() != *expected_payload_size) {
            return reject(Error::InvalidValue);
        }

        AudioBuffer buffer{packet.sampling_rate_hz, packet.channel_count, packet.samples_per_channel,
                           cfg_.storage_format};

        const std::size_t bytes_per_sample = static_cast<std::size_t>(*wire_sample_bytes);

        for (uint32_t sample_index = 0; sample_index < packet.samples_per_channel; ++sample_index) {
            for (uint16_t channel = 0; channel < packet.channel_count; ++channel) {
                const std::size_t sample_ordinal =
                    static_cast<std::size_t>(sample_index) * static_cast<std::size_t>(packet.channel_count) +
                    static_cast<std::size_t>(channel);

                const std::size_t payload_offset = sample_ordinal * bytes_per_sample;

                const auto decoded = decode_audio_pcm_wire_sample_to_s32(
                    ByteSpan{packet.payload.data() + payload_offset, bytes_per_sample}, packet.wire_format);

                if (!decoded.has_value()) {
                    return reject(decoded.error());
                }

                buffer.sample(sample_index, channel) = *decoded;
            }
        }

        ++stats_.packets_used;
        ++stats_.blocks_emitted;

        return AssembledAudioBlock{std::move(buffer), packet.rtp.timestamp, packet.rtp.seq_number, packet.rtp.marker,
                                   true};
    }

    void reset() { stats_ = {}; }

    [[nodiscard]] const AudioFrameAssemblerStats &stats() const { return stats_; }

  private:
    AudioFrameAssemblerConfig cfg_{};
    AudioFrameAssemblerStats stats_{};
};
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_AUDIO_FRAME_ASSEMBLER_HPP
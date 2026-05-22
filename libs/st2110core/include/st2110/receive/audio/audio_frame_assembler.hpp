#ifndef ST2110_OBS_PLUGIN_AUDIO_FRAME_ASSEMBLER_HPP
#define ST2110_OBS_PLUGIN_AUDIO_FRAME_ASSEMBLER_HPP

#include <st2110/delivery/audio/audio_frame.hpp>
#include <st2110/foundation/bytes.hpp>
#include <st2110/receive/audio/audio_packet.hpp>

#include <cstddef>
#include <cstdint>
#include <utility>

namespace st2110 {
struct AssembledAudioBlock {
    AudioBuffer buffer;
    std::uint32_t rtp_timestamp = 0;
    TimestampNs receive_timestamp_ns = 0;
    std::uint16_t rtp_sequence_number = 0;
    bool rtp_marker = false;
    bool complete = true;
};

struct AudioFrameAssemblerStats {
    std::uint64_t packets_used = 0;
    std::uint64_t blocks_emitted = 0;
};

[[nodiscard]] inline std::int32_t decode_audio_pcm_wire_sample_to_s32(const ByteSpan sample_bytes,
                                                                      const AudioPcmBitDepth bit_depth) {
    switch (bit_depth) {
    case AudioPcmBitDepth::Bits16: {
        const std::uint32_t raw =
            (static_cast<std::uint32_t>(sample_bytes[0]) << 8U) | static_cast<std::uint32_t>(sample_bytes[1]);

        auto value = static_cast<std::int32_t>(raw);
        if ((raw & 0x8000U) != 0) {
            value -= 0x10000;
        }

        return static_cast<std::int32_t>(static_cast<std::int64_t>(value) * 65536);
    }

    case AudioPcmBitDepth::Bits24: {
        const std::uint32_t raw = (static_cast<std::uint32_t>(sample_bytes[0]) << 16U) |
                                  (static_cast<std::uint32_t>(sample_bytes[1]) << 8U) |
                                  static_cast<std::uint32_t>(sample_bytes[2]);

        auto value = static_cast<std::int32_t>(raw);
        if ((raw & 0x800000U) != 0) {
            value -= 0x1000000;
        }

        return static_cast<std::int32_t>(static_cast<std::int64_t>(value) * 256);
    }

    default:
        std::unreachable();
    }
}

class AudioFrameAssembler {
  public:
    [[nodiscard]] AssembledAudioBlock push(const AudioPacketView &packet) {
        const auto bytes_per_sample = audio_pcm_wire_sample_bytes(packet.pcm_bit_depth);

        AudioBuffer buffer{packet.sampling_rate_hz, packet.channel_count, packet.samples_per_channel};

        for (std::uint32_t sample_index = 0; sample_index < packet.samples_per_channel; ++sample_index) {
            for (std::uint16_t channel = 0; channel < packet.channel_count; ++channel) {
                const std::size_t sample_ordinal =
                    static_cast<std::size_t>(sample_index) * static_cast<std::size_t>(packet.channel_count) +
                    static_cast<std::size_t>(channel);

                const std::size_t payload_offset = sample_ordinal * bytes_per_sample;

                buffer.sample(sample_index, channel) = decode_audio_pcm_wire_sample_to_s32(
                    ByteSpan{packet.payload_data.data() + payload_offset, bytes_per_sample}, packet.pcm_bit_depth);
            }
        }

        ++stats_.packets_used;
        ++stats_.blocks_emitted;

        return AssembledAudioBlock{
            .buffer = std::move(buffer),
            .rtp_timestamp = packet.rtp.timestamp,
            .receive_timestamp_ns = packet.receive_timestamp_ns,
            .rtp_sequence_number = packet.rtp.seq_number,
            .rtp_marker = packet.rtp.marker,
            .complete = true,
        };
    }

    void reset() { stats_ = {}; }

    [[nodiscard]] const AudioFrameAssemblerStats &stats() const { return stats_; }

  private:
    AudioFrameAssemblerStats stats_{};
};
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_AUDIO_FRAME_ASSEMBLER_HPP
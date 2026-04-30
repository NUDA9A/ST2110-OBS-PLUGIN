#ifndef ST2110_OBS_PLUGIN_AUDIO_REORDER_BUFFER_HPP
#define ST2110_OBS_PLUGIN_AUDIO_REORDER_BUFFER_HPP

#include "rtp.hpp"
#include "audio_packet.hpp"
#include "error.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace st2110 {
struct AudioReorderBufferConfig {
    uint16_t window_size_packets = 64;
};

struct AudioReorderBufferStats {
    uint64_t packets_pushed = 0;
    uint64_t packets_popped = 0;
    uint64_t duplicates = 0;
    uint64_t late_packets = 0;
    uint64_t out_of_window = 0;
    uint64_t missing_packets_flushed = 0;
};

struct StoredAudioRtpPacket {
    RtpHeaderView rtp{};
    std::vector<uint8_t> payload{};
    uint32_t sampling_rate_hz = 0;
    uint16_t channel_count = 0;
    uint32_t samples_per_channel = 0;
    AudioPcmWireFormat wire_format = AudioPcmWireFormat::L24;

    [[nodiscard]] AudioRtpPacketView view() const;
};

class AudioFixedWindowReorderBuffer {
  public:
    explicit AudioFixedWindowReorderBuffer(AudioReorderBufferConfig cfg = {});

    [[nodiscard]] Error push(const AudioRtpPacketView &packet);
    [[nodiscard]] std::optional<StoredAudioRtpPacket> pop_next();

    void flush_missing_once();
    void reset();

    [[nodiscard]] bool has_pending() const;
    [[nodiscard]] const AudioReorderBufferStats &stats() const;

  private:
    AudioReorderBufferConfig cfg_{};
    AudioReorderBufferStats stats_{};
    std::optional<uint16_t> expected_seq_{};
    std::map<uint16_t, StoredAudioRtpPacket> pending_{};

    [[nodiscard]] bool config_is_valid() const;
    [[nodiscard]] static uint16_t next_seq(uint16_t seq);
    [[nodiscard]] static uint16_t forward_seq_distance(uint16_t from, uint16_t to);
    [[nodiscard]] static StoredAudioRtpPacket store_packet(const AudioRtpPacketView &packet);
};

[[nodiscard]] inline AudioRtpPacketView StoredAudioRtpPacket::view() const {
    return AudioRtpPacketView{
        rtp,        ByteSpan{payload.data(), payload.size()}, sampling_rate_hz, channel_count, samples_per_channel,
        wire_format};
}

inline AudioFixedWindowReorderBuffer::AudioFixedWindowReorderBuffer(AudioReorderBufferConfig cfg) : cfg_(cfg) {}

[[nodiscard]] inline bool AudioFixedWindowReorderBuffer::config_is_valid() const {
    return cfg_.window_size_packets != 0;
}

[[nodiscard]] inline uint16_t AudioFixedWindowReorderBuffer::next_seq(uint16_t seq) {
    return static_cast<uint16_t>(seq + 1);
}

[[nodiscard]] inline uint16_t AudioFixedWindowReorderBuffer::forward_seq_distance(uint16_t from, uint16_t to) {
    return static_cast<uint16_t>(to - from);
}

[[nodiscard]] inline StoredAudioRtpPacket
AudioFixedWindowReorderBuffer::store_packet(const AudioRtpPacketView &packet) {
    StoredAudioRtpPacket stored{};
    stored.rtp = packet.rtp;
    stored.payload.assign(packet.payload.begin(), packet.payload.end());
    stored.sampling_rate_hz = packet.sampling_rate_hz;
    stored.channel_count = packet.channel_count;
    stored.samples_per_channel = packet.samples_per_channel;
    stored.wire_format = packet.wire_format;
    return stored;
}

[[nodiscard]] inline Error AudioFixedWindowReorderBuffer::push(const AudioRtpPacketView &packet) {
    if (!config_is_valid()) {
        return Error::InvalidValue;
    }

    const uint16_t seq = packet.rtp.seq_number;

    if (!expected_seq_) {
        expected_seq_ = seq;
    }

    if (pending_.find(seq) != pending_.end()) {
        ++stats_.duplicates;
        return Error::InvalidValue;
    }

    if (seq_less(seq, *expected_seq_)) {
        ++stats_.late_packets;
        return Error::InvalidValue;
    }

    const uint16_t distance = forward_seq_distance(*expected_seq_, seq);
    if (distance >= cfg_.window_size_packets) {
        ++stats_.out_of_window;
        return Error::InvalidValue;
    }

    pending_.emplace(seq, store_packet(packet));
    ++stats_.packets_pushed;
    return Error::Ok;
}

[[nodiscard]] inline std::optional<StoredAudioRtpPacket> AudioFixedWindowReorderBuffer::pop_next() {
    if (!expected_seq_) {
        return std::nullopt;
    }

    auto it = pending_.find(*expected_seq_);
    if (it == pending_.end()) {
        return std::nullopt;
    }

    StoredAudioRtpPacket packet = std::move(it->second);
    pending_.erase(it);

    expected_seq_ = next_seq(*expected_seq_);
    ++stats_.packets_popped;

    return packet;
}

inline void AudioFixedWindowReorderBuffer::flush_missing_once() {
    if (!expected_seq_ || pending_.empty()) {
        return;
    }

    if (pending_.find(*expected_seq_) != pending_.end()) {
        return;
    }

    expected_seq_ = next_seq(*expected_seq_);
    ++stats_.missing_packets_flushed;
}

inline void AudioFixedWindowReorderBuffer::reset() {
    expected_seq_.reset();
    pending_.clear();
    stats_ = {};
}

[[nodiscard]] inline bool AudioFixedWindowReorderBuffer::has_pending() const { return !pending_.empty(); }

[[nodiscard]] inline const AudioReorderBufferStats &AudioFixedWindowReorderBuffer::stats() const { return stats_; }
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_AUDIO_REORDER_BUFFER_HPP
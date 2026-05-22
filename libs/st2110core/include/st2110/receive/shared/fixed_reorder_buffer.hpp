#ifndef ST2110_OBS_PLUGIN_FIXED_REORDER_BUFFER_HPP
#define ST2110_OBS_PLUGIN_FIXED_REORDER_BUFFER_HPP

#include <st2110/receive/audio/audio_packet.hpp>
#include <st2110/receive/shared/reorder_buffer.hpp>
#include <st2110/receive/video/video_packet_view.hpp>

#include <cstdint>
#include <map>
#include <stdexcept>

namespace st2110 {
struct VideoStoredPacket final : StoredPacket {
    SrdHeader segment_headers[maxPacketSrdSegments]{};
    std::uint8_t segment_count = 0;

    explicit VideoStoredPacket(const VideoPacketView &packetView)
        : StoredPacket(packetView.rtp, packetView.payload_data, packetView.extended_seq,
                       packetView.receive_timestamp_ns),
          segment_count(packetView.segment_count) {
        for (std::size_t i = 0; i < segment_count; ++i) {
            segment_headers[i] = packetView.segments[i].header;
        }
    }

    [[nodiscard]] std::unique_ptr<PacketView> view() const override {
        VideoPacketView pkt{};
        pkt.rtp = rtp_;
        pkt.extended_seq = extended_seq;
        pkt.segment_count = segment_count;
        pkt.payload_data = ByteSpan(payload_data.data(), payload_data.size());
        pkt.receive_timestamp_ns = receive_timestamp_ns;

        std::size_t offset = 0;
        for (std::size_t i = 0; i < segment_count; ++i) {
            pkt.segments[i].header = segment_headers[i];
            pkt.segments[i].data = pkt.payload_data.subspan(offset, segment_headers[i].length);
            offset += segment_headers[i].length;
        }

        pkt.trailing_padding = pkt.payload_data.subspan(offset);

        return std::make_unique<VideoPacketView>(pkt);
    }

    ~VideoStoredPacket() override = default;
};

struct AudioStoredPacket final : StoredPacket {
    uint32_t sampling_rate_hz = 0;
    uint16_t channel_count = 0;
    uint32_t samples_per_channel = 0;
    AudioPcmBitDepth bit_depth = AudioPcmBitDepth::Bits24;

    explicit AudioStoredPacket(const AudioPacketView &packet)
        : StoredPacket(packet.rtp, packet.payload_data, packet.reorder_sequence(), packet.receive_timestamp_ns),
          sampling_rate_hz(packet.sampling_rate_hz), channel_count(packet.channel_count),
          samples_per_channel(packet.samples_per_channel), bit_depth(packet.pcm_bit_depth) {}
    [[nodiscard]] std::unique_ptr<PacketView> view() const override {
        auto pkt = std::make_unique<AudioPacketView>();
        pkt->rtp = rtp_;
        pkt->payload_data = ByteSpan(payload_data.data(), payload_data.size());
        pkt->sampling_rate_hz = sampling_rate_hz;
        pkt->channel_count = channel_count;
        pkt->samples_per_channel = samples_per_channel;
        pkt->pcm_bit_depth = bit_depth;
        pkt->receive_timestamp_ns = receive_timestamp_ns;
        return pkt;
    }
};

template <bool is_video_ = false> class FixedWindowReorderBuffer final : public IReorderBuffer {
  public:
    explicit FixedWindowReorderBuffer(const std::uint32_t window_size) : window_size_(window_size) {
        if (window_size_ == 0) {
            throw std::invalid_argument("window_size must be greater than 0");
        }
    }

    Error push(std::unique_ptr<StoredPacket> packet) override {
        if constexpr (is_video_) {
            return push_video(std::move(packet));
        } else {
            return push_audio(std::move(packet));
        }
    }

    [[nodiscard]] std::unique_ptr<StoredPacket> pop_next() override {
        if (!initialized_ || packets_.empty()) {
            return nullptr;
        }
        std::map<std::uint32_t, std::unique_ptr<StoredPacket>>::iterator it;
        if constexpr (is_video_) {
            it = packets_.find(next_expected_seq_);
        } else {
            it = packets_.find(next_expected_audio_seq_);
        }

        if (it != packets_.end()) {
            auto res = std::move(it->second);
            packets_.erase(it);
            if constexpr (is_video_) {
                ++next_expected_seq_;
            } else {
                ++next_expected_audio_seq_;
            }
            ++stats_.packets_popped;
            missing_head_accounted_ = false;
            return res;
        } else {
            if (!packets_.empty() && !missing_head_accounted_) {
                ++stats_.missing_seq;
                missing_head_accounted_ = true;
            }
            return nullptr;
        }
    }

    void reset() override {
        packets_.clear();
        next_expected_seq_ = 0;
        next_expected_audio_seq_ = 0;
        stats_ = {};
        missing_head_accounted_ = false;
        initialized_ = false;
    }

    [[nodiscard]] ReorderBufferStats stats() const override { return stats_; }

    [[nodiscard]] bool flush_missing_once() override {
        bool has_seq = false;
        if constexpr (is_video_) {
            has_seq = packets_.contains(next_expected_seq_);
        } else {
            has_seq = packets_.contains(next_expected_audio_seq_);
        }
        if (!initialized_ || packets_.empty() || has_seq) {
            return false;
        }

        if constexpr (is_video_) {
            ++next_expected_seq_;
        } else {
            ++next_expected_audio_seq_;
        }
        missing_head_accounted_ = false;
        ++stats_.missing_seq_flushed;
        return true;
    }

    [[nodiscard]] bool flush_after_n_packets(const std::uint32_t threshold_packets) override {
        if (!initialized_ || packets_.empty()) {
            return false;
        }

        const std::uint32_t effective_threshold = threshold_packets == 0 ? 1 : threshold_packets;

        if (packets_.size() < static_cast<std::size_t>(effective_threshold)) {
            return false;
        }

        bool has_expected_seq = false;
        if constexpr (is_video_) {
            has_expected_seq = packets_.contains(next_expected_seq_);
        } else {
            has_expected_seq = packets_.contains(next_expected_audio_seq_);
        }

        if (has_expected_seq) {
            return false;
        }

        return flush_missing_once();
    }

    [[nodiscard]] bool flush_missing_until_marker_boundary() override {
        if (!initialized_ || packets_.empty()) {
            return false;
        }

        if (has_next_expected_packet()) {
            return false;
        }

        if (!has_marker_boundary_packet()) {
            return false;
        }

        return flush_missing_once();
    }

  private:
    [[nodiscard]] bool has_next_expected_packet() const {
        if constexpr (is_video_) {
            return packets_.contains(next_expected_seq_);
        } else {
            return packets_.contains(next_expected_audio_seq_);
        }
    }

    [[nodiscard]] bool has_marker_boundary_packet() const {
        for (const auto &[seq, packet] : packets_) {
            (void)seq;

            if (packet && packet->rtp_.marker) {
                return true;
            }
        }

        return false;
    }

    [[nodiscard]] std::optional<std::uint32_t>
    find_video_recovery_expected_for_incoming(const std::uint32_t incoming_seq) const {
        std::optional<std::uint32_t> best{};
        std::uint32_t best_distance_from_current = 0;

        for (const auto &[candidate_seq, packet] : packets_) {
            (void)packet;

            const std::uint32_t candidate_from_current = candidate_seq - next_expected_seq_;
            if (candidate_from_current > 0x7FFFFFFFu) {
                continue; // already late relative to current expected seq
            }

            const std::uint32_t incoming_from_candidate = incoming_seq - candidate_seq;
            if (incoming_from_candidate > 0x7FFFFFFFu) {
                continue; // candidate is after incoming seq
            }

            if (incoming_from_candidate >= window_size_) {
                continue; // incoming still would not fit
            }

            if (!best.has_value() || candidate_from_current < best_distance_from_current) {
                best = candidate_seq;
                best_distance_from_current = candidate_from_current;
            }
        }

        return best;
    }

    [[nodiscard]] std::optional<std::uint16_t>
    find_audio_recovery_expected_for_incoming(const std::uint16_t incoming_seq) const {
        std::optional<std::uint16_t> best{};
        std::uint16_t best_distance_from_current = 0;

        for (const auto &[candidate_seq_u32, packet] : packets_) {
            (void)packet;

            const auto candidate_seq = static_cast<std::uint16_t>(candidate_seq_u32);

            const std::uint16_t candidate_from_current = candidate_seq - next_expected_audio_seq_;
            if (candidate_from_current > 0x8000u) {
                continue; // already late relative to current expected seq
            }

            const std::uint16_t incoming_from_candidate = incoming_seq - candidate_seq;
            if (incoming_from_candidate > 0x8000u) {
                continue; // candidate is after incoming seq
            }

            if (incoming_from_candidate >= window_size_) {
                continue; // incoming still would not fit
            }

            if (!best.has_value() || candidate_from_current < best_distance_from_current) {
                best = candidate_seq;
                best_distance_from_current = candidate_from_current;
            }
        }

        return best;
    }

    void erase_packets_outside_video_window() {
        for (auto it = packets_.begin(); it != packets_.end();) {
            const std::uint32_t dist = it->first - next_expected_seq_;

            if (dist > 0x7FFFFFFFu || dist >= window_size_) {
                it = packets_.erase(it);
                continue;
            }

            ++it;
        }
    }

    void erase_packets_outside_audio_window() {
        for (auto it = packets_.begin(); it != packets_.end();) {
            const auto seq = static_cast<std::uint16_t>(it->first);
            const std::uint16_t dist = seq - next_expected_audio_seq_;

            if (dist > 0x8000u || dist >= window_size_) {
                it = packets_.erase(it);
                continue;
            }

            ++it;
        }
    }

    void advance_video_expected_to(const std::uint32_t new_expected_seq) {
        const std::uint32_t skipped = new_expected_seq - next_expected_seq_;
        stats_.missing_seq_flushed += skipped;

        next_expected_seq_ = new_expected_seq;
        missing_head_accounted_ = false;

        erase_packets_outside_video_window();
    }

    void advance_audio_expected_to(const std::uint16_t new_expected_seq) {
        const std::uint16_t skipped = new_expected_seq - next_expected_audio_seq_;
        stats_.missing_seq_flushed += skipped;

        next_expected_audio_seq_ = new_expected_seq;
        missing_head_accounted_ = false;

        erase_packets_outside_audio_window();
    }

    [[nodiscard]] Error push_audio(std::unique_ptr<StoredPacket> packet) {
        const auto seq = static_cast<std::uint16_t>(packet->reorder_sequence());

        if (!initialized_) {
            initialized_ = true;
            next_expected_audio_seq_ = seq;
        }

        ++stats_.packets_pushed;

        std::uint16_t dist = seq - next_expected_audio_seq_;

        if (dist >= window_size_) {
            if (dist > 0x8000u) {
                ++stats_.late_packets;
                return Error::InvalidValue;
            }

            /*
             * Forward out-of-window means the current missing head cannot be
             * recovered within the configured reorder window.
             *
             * Preserve buffered packets when possible; only resync directly to the
             * incoming packet if no buffered sequence can make it fit.
             */
            ++stats_.out_of_window;

            if (auto recovery_expected = find_audio_recovery_expected_for_incoming(seq)) {
                advance_audio_expected_to(*recovery_expected);
            } else {
                advance_audio_expected_to(seq);
            }

            dist = seq - next_expected_audio_seq_;
            if (dist >= window_size_ || dist > 0x8000u) {
                return Error::InvalidValue;
            }
        }

        if (packets_.contains(seq)) {
            ++stats_.duplicates;
            return Error::InvalidValue;
        }

        packets_.emplace(seq, std::move(packet));
        ++stats_.packets_stored;

        return Error::Ok;
    }

    [[nodiscard]] Error push_video(std::unique_ptr<StoredPacket> packet) {
        const auto seq = packet->reorder_sequence();

        if (!initialized_) {
            initialized_ = true;
            next_expected_seq_ = seq;
        }

        ++stats_.packets_pushed;

        std::uint32_t dist = seq - next_expected_seq_;

        if (dist >= window_size_) {
            if (dist > 0x7FFFFFFFu) {
                ++stats_.late_packets;
                return Error::InvalidValue;
            }

            /*
             * Forward out-of-window means the current missing head cannot be
             * recovered within the configured reorder window.
             *
             * Prefer advancing to the oldest buffered packet that makes the incoming
             * packet fit. This preserves already buffered useful packets instead of
             * clearing the entire buffer.
             */
            ++stats_.out_of_window;

            if (auto recovery_expected = find_video_recovery_expected_for_incoming(seq)) {
                advance_video_expected_to(*recovery_expected);
            } else {
                advance_video_expected_to(seq);
            }

            dist = seq - next_expected_seq_;
            if (dist >= window_size_ || dist > 0x7FFFFFFFu) {
                return Error::InvalidValue;
            }
        }

        if (packets_.contains(seq)) {
            ++stats_.duplicates;
            return Error::InvalidValue;
        }

        packets_.emplace(seq, std::move(packet));
        ++stats_.packets_stored;

        return Error::Ok;
    }

    std::uint32_t window_size_;
    std::uint32_t next_expected_seq_ = 0;
    std::uint16_t next_expected_audio_seq_ = 0;
    std::map<std::uint32_t, std::unique_ptr<StoredPacket>> packets_;
    ReorderBufferStats stats_{};
    bool initialized_ = false;
    bool missing_head_accounted_ = false;
};

inline std::unique_ptr<StoredPacket> AudioPacketView::store() const {
    return std::make_unique<AudioStoredPacket>(*this);
}

inline std::unique_ptr<StoredPacket> VideoPacketView::store() const {
    return std::make_unique<VideoStoredPacket>(*this);
}

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_FIXED_REORDER_BUFFER_HPP

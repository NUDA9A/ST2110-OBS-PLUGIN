#ifndef ST2110_OBS_PLUGIN_FIXED_REORDER_BUFFER_HPP
#define ST2110_OBS_PLUGIN_FIXED_REORDER_BUFFER_HPP

#include "reorder_buffer.hpp"
#include <st2110/receive/video/video_packet_view.hpp>
#include <st2110/receive/audio/audio_packet.hpp>

#include <map>
#include <stdexcept>
#include <cstdint>

namespace st2110 {
struct VideoStoredPacket final : StoredPacket {
    SrdHeader segment_headers[maxPacketSrdSegments]{};
    std::uint8_t segment_count = 0;

    explicit VideoStoredPacket(const VideoPacketView &packetView)
        : StoredPacket(packetView.rtp, packetView.payload_data, packetView.extended_seq), segment_count(packetView.segment_count) {
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
    AudioPcmWireFormat wire_format = AudioPcmWireFormat::L24;

    explicit AudioStoredPacket(const AudioPacketView &packet)
        : StoredPacket(packet.rtp, packet.payload_data, packet.reorder_sequence()), sampling_rate_hz(packet.sampling_rate_hz), channel_count(packet.channel_count),
          samples_per_channel(packet.samples_per_channel), wire_format(packet.wire_format) {}
    [[nodiscard]] std::unique_ptr<PacketView> view() const override {
        auto pkt = std::make_unique<AudioPacketView>();
        pkt->rtp = rtp_;
        pkt->payload_data = ByteSpan(payload_data.data(), payload_data.size());
        pkt->sampling_rate_hz = sampling_rate_hz;
        pkt->channel_count = channel_count;
        pkt->samples_per_channel = samples_per_channel;
        pkt->wire_format = wire_format;
        return pkt;
    }
};

template <bool is_video_ = false>
class FixedWindowReorderBuffer final : public IReorderBuffer {
  public:
    explicit FixedWindowReorderBuffer(const std::uint32_t window_size) : window_size_(window_size) {
        if (window_size_ == 0) {
            throw std::invalid_argument("window_size must be greater than 0");
        }
    }

    Error push(const PacketView &packet) override {
        auto err = Error::Ok;
        if constexpr (is_video_) {
            err = push_video(packet);
        } else {
            err = push_audio(packet);
        }
        if (err != Error::Ok) {
            return err;
        }

        return Error::Ok;
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

  private:
    [[nodiscard]] Error push_audio(const PacketView &packet) {
        auto seq = static_cast<std::uint16_t>(packet.reorder_sequence());
        if (!initialized_) {
            initialized_ = true;
            next_expected_audio_seq_ = seq;
        }

        ++stats_.packets_pushed;
        std::uint16_t dist = seq - next_expected_audio_seq_;
        if (dist >= window_size_) {
            if (dist > 0x8000) {
                ++stats_.late_packets;
                return Error::InvalidValue;
            } else {
                ++stats_.out_of_window;
                return Error::InvalidValue;
            }
            return Error::Ok;
        }

        if (packets_.contains(seq)) {
            ++stats_.duplicates;
            return Error::InvalidValue;
        }
        auto stored = packet.store();
        packets_.emplace(seq, std::move(stored));
        ++stats_.packets_stored;

        return Error::Ok;
    }

    [[nodiscard]] Error push_video(const PacketView &packet) {
        auto seq = packet.reorder_sequence();
        if (!initialized_) {
            initialized_ = true;
            next_expected_seq_ = seq;
        }

        ++stats_.packets_pushed;
        auto dist = seq - next_expected_seq_;
        if (dist >= window_size_) {
            if (dist > 0x7FFFFFFFu) {
                ++stats_.late_packets;
                return Error::InvalidValue;
            } else {
                ++stats_.out_of_window;
                return Error::InvalidValue;
            }
            return Error::Ok;
        }

        if (packets_.contains(seq)) {
            ++stats_.duplicates;
            return Error::InvalidValue;
        }
        auto stored = packet.store();
        packets_.emplace(seq, std::move(stored));
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
    //bool is_video_ = false;
};

inline std::unique_ptr<StoredPacket> AudioPacketView::store() const {
    return std::make_unique<AudioStoredPacket>(*this);
}

inline std::unique_ptr<StoredPacket> VideoPacketView::store() const {
    return std::make_unique<VideoStoredPacket>(*this);
}

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_FIXED_REORDER_BUFFER_HPP

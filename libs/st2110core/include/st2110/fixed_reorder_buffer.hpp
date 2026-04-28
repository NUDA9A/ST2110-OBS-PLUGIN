#ifndef ST2110_OBS_PLUGIN_FIXED_REORDER_BUFFER_HPP
#define ST2110_OBS_PLUGIN_FIXED_REORDER_BUFFER_HPP

#include "reorder_buffer.hpp"
#include <map>
#include <stdexcept>
#include <optional>

namespace st2110 {
class FixedWindowReorderBuffer final : public IReorderBuffer {
public:
  explicit FixedWindowReorderBuffer(uint32_t window_size) : window_size_(window_size) {
    if (window_size_ == 0) {
      throw std::invalid_argument("window_size must be greater than 0");
    }
  }

  void push(const PacketView &packet) override {
    ++stats_.packets_pushed;
    if (!initialized_) {
      initialized_ = true;
      next_expected_seq_ = packet.extended_seq;
    }
    auto dist = packet.extended_seq - next_expected_seq_;

    if (dist >= window_size_) {
      if (dist > 0x7FFFFFFFu) {
        ++stats_.late_packets;
      } else {
        ++stats_.out_of_window;
      }
      return;
    }

    if (packets_.find(packet.extended_seq) != packets_.end()) {
      ++stats_.duplicates;
      return;
    }
    packets_.insert({packet.extended_seq, StoredPacket(packet)});
    ++stats_.packets_stored;
  }

  [[nodiscard]] std::optional<StoredPacket> pop_next() override {
    if (!initialized_) {
      return std::nullopt;
    }

    if (auto it = packets_.find(next_expected_seq_); it != packets_.end()) {
      StoredPacket res = std::move(it->second);
      packets_.erase(it);
      ++next_expected_seq_;
      ++stats_.packets_popped;
      missing_head_accounted_ = false;
      return res;
    } else {
      if (!packets_.empty() && !missing_head_accounted_) {
        ++stats_.missing_seq;
        missing_head_accounted_ = true;
      }
      return std::nullopt;
    }
  }

  void reset() override {
    packets_.clear();
    initialized_ = false;
    next_expected_seq_ = 0;
    stats_ = {};
    missing_head_accounted_ = false;
  }

  [[nodiscard]] const ReorderBufferStats &stats() const { return stats_; }

  [[nodiscard]] bool flush_missing_once() {
    if (!initialized_ || packets_.empty() || packets_.find(next_expected_seq_) != packets_.end()) {
      return false;
    }

    ++next_expected_seq_;
    missing_head_accounted_ = false;
    ++stats_.missing_seq_flushed;
    return true;
  }

private:
  uint32_t window_size_;
  bool initialized_ = false;
  uint32_t next_expected_seq_ = 0;
  std::map<uint32_t, StoredPacket> packets_;
  ReorderBufferStats stats_{};
  bool missing_head_accounted_ = false;
};

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_FIXED_REORDER_BUFFER_HPP

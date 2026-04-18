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
        if (!initialized_) {
            initialized_ = true;
            next_expected_seq_ = packet.extended_seq;
        }
        auto dist = packet.extended_seq - next_expected_seq_;

        if (dist >= window_size_) {
            return;
        }

        if (packets_.find(packet.extended_seq) != packets_.end()) {
            return;
        }
        packets_.insert({packet.extended_seq, StoredPacket(packet)});
    }

    [[nodiscard]] std::optional <StoredPacket> pop_next() override {
        if (!initialized_) {
            return std::nullopt;
        }

        if (auto it = packets_.find(next_expected_seq_); it != packets_.end()) {
            StoredPacket res = std::move(it->second);
            packets_.erase(it);
            ++next_expected_seq_;
            return res;
        } else {
            return std::nullopt;
        }
    }

    void reset() override {
        packets_.clear();
        initialized_ = false;
        next_expected_seq_ = 0;
    }

private:
    uint32_t window_size_;
    bool initialized_ = false;
    uint32_t next_expected_seq_ = 0;
    std::map <uint32_t, StoredPacket> packets_;
};

}

#endif //ST2110_OBS_PLUGIN_FIXED_REORDER_BUFFER_HPP

#ifndef ST2110_OBS_PLUGIN_REORDER_BUFFER_HPP
#define ST2110_OBS_PLUGIN_REORDER_BUFFER_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "packet_view.hpp"
#include "stats.hpp"

namespace st2110 {
struct StoredPacket {
    RtpHeaderView rtp{};
    uint32_t extended_seq = 0;

    SrdHeader segment_headers[maxPacketSrdSegments]{};
    uint8_t segment_count = 0;

    std::vector<uint8_t> payload_data{};

    [[nodiscard]] PacketView view() const {
        PacketView pkt{};
        pkt.rtp = rtp;
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

        return pkt;
    }

    StoredPacket() = default;

    explicit StoredPacket(const PacketView &packetView)
        : rtp(packetView.rtp), extended_seq(packetView.extended_seq), segment_count(packetView.segment_count) {
        for (std::size_t i = 0; i < segment_count; ++i) {
            segment_headers[i] = packetView.segments[i].header;
        }
        auto src = packetView.payload_data;
        payload_data.assign(src.begin(), src.end());
    }
};

class IReorderBuffer {
  public:
    virtual void push(const PacketView &packet) = 0;

    [[nodiscard]] virtual std::optional<StoredPacket> pop_next() = 0;

    virtual void reset() = 0;

    [[nodiscard]] virtual ReorderBufferStats stats() const = 0;

    virtual ~IReorderBuffer() = default;
};

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_REORDER_BUFFER_HPP
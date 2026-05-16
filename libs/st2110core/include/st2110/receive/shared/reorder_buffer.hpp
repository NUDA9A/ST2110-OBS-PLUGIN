#ifndef ST2110_OBS_PLUGIN_REORDER_BUFFER_HPP
#define ST2110_OBS_PLUGIN_REORDER_BUFFER_HPP

#include <cstdint>
#include <vector>
#include <memory>

#include <st2110/foundation/error.hpp>
#include <st2110/foundation/timestamp.hpp>
#include <st2110/ingress/shared/packet_view.hpp>
#include <st2110/receive/shared/reorder_stats.hpp>

namespace st2110 {
struct StoredPacket {
    RtpHeaderView rtp_{};
    std::vector<uint8_t> payload_data{};
    std::uint32_t extended_seq = 0;
    TimestampNs receive_timestamp_ns = 0;

    explicit StoredPacket(const RtpHeaderView &rtp, ByteSpan payload, const std::uint32_t seq,
                          const TimestampNs receive_timestamp)
        : rtp_(rtp), payload_data(payload.begin(), payload.end()), extended_seq(seq),
          receive_timestamp_ns(receive_timestamp) {}
    [[nodiscard]] std::uint32_t reorder_sequence() const { return extended_seq; }
    [[nodiscard]] virtual std::unique_ptr<PacketView> view() const = 0;
    virtual ~StoredPacket() = default;
};

class IReorderBuffer {
  public:
    virtual Error push(std::unique_ptr<StoredPacket> packet) = 0;
    virtual Error push(const PacketView &packet) { return push(packet.store()); }

    [[nodiscard]] virtual std::unique_ptr<StoredPacket> pop_next() = 0;

    [[nodiscard]] virtual bool flush_missing_once() = 0;

    [[nodiscard]] virtual bool flush_after_n_packets(std::uint32_t threshold_packets) = 0;

    virtual void reset() = 0;

    [[nodiscard]] virtual ReorderBufferStats stats() const = 0;

    virtual ~IReorderBuffer() = default;
};

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_REORDER_BUFFER_HPP
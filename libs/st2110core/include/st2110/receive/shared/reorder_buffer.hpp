#ifndef ST2110_OBS_PLUGIN_REORDER_BUFFER_HPP
#define ST2110_OBS_PLUGIN_REORDER_BUFFER_HPP

#include <cstdint>
#include <vector>

#include <st2110/receive/shared/reorder_stats.hpp>
#include <st2110/ingress/shared/packet_view.hpp>
#include <st2110/foundation/error.hpp>

namespace st2110 {
struct StoredPacket {
    RtpHeaderView rtp_{};
    std::vector<uint8_t> payload_data{};
    std::uint32_t extended_seq = 0;

    explicit StoredPacket(const RtpHeaderView& rtp, ByteSpan payload, const std::uint32_t seq) : rtp_(rtp), payload_data(payload.begin(), payload.end()), extended_seq(seq) {}
    [[nodiscard]] std::uint32_t reorder_sequence() const { return extended_seq; }
    [[nodiscard]] virtual std::unique_ptr<PacketView> view() const = 0;
    virtual ~StoredPacket() = default;
};

class IReorderBuffer {
public:
    virtual Error push(const PacketView &packet) = 0;

    [[nodiscard]] virtual std::unique_ptr<StoredPacket> pop_next() = 0;

    [[nodiscard]] virtual bool flush_missing_once() = 0;

    virtual void reset() = 0;

    [[nodiscard]] virtual ReorderBufferStats stats() const = 0;

    virtual ~IReorderBuffer() = default;
};

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_REORDER_BUFFER_HPP
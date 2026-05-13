#ifndef ST2110_OBS_PLUGIN_PACKET_VIEW_HPP
#define ST2110_OBS_PLUGIN_PACKET_VIEW_HPP

#include <cstddef>
#include <cstdint>

#include <st2110/foundation/bytes.hpp>
#include <st2110/foundation/timestamp.hpp>
#include <st2110/ingress/shared/rtp.hpp>

#include <memory>

namespace st2110 {
struct StoredPacket;

struct PacketView {
    RtpHeaderView rtp{};
    ByteSpan payload_data{};
    TimestampNs receive_timestamp_ns = 0;

    [[nodiscard]] virtual std::uint32_t reorder_sequence() const = 0;
    [[nodiscard]] virtual std::unique_ptr<StoredPacket> store() const = 0;

    virtual ~PacketView() = default;
};
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_PACKET_VIEW_HPP
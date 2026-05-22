#ifndef ST2110_OBS_VIDEO_PACKET_VIEW_HPP
#define ST2110_OBS_VIDEO_PACKET_VIEW_HPP

#include <st2110/foundation/bytes.hpp>
#include <st2110/ingress/shared/packet_view.hpp>
#include <st2110/ingress/shared/st2110_20.hpp>

namespace st2110 {
inline constexpr std::size_t maxPacketSrdSegments = 3;

struct SrdSegmentView {
    SrdHeader header{};
    ByteSpan data{};
};

struct VideoPacketView final : PacketView {
    SrdSegmentView segments[maxPacketSrdSegments]{};
    std::uint8_t segment_count = 0;
    std::uint32_t extended_seq = 0;
    ByteSpan trailing_padding{};

    [[nodiscard]] std::unique_ptr<StoredPacket> store() const override;
    [[nodiscard]] std::uint32_t reorder_sequence() const override { return extended_seq; }
};

} // namespace st2110

#endif // ST2110_OBS_VIDEO_PACKET_VIEW_HPP

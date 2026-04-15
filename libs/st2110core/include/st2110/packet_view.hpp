#ifndef ST2110_OBS_PLUGIN_PACKET_VIEW_HPP
#define ST2110_OBS_PLUGIN_PACKET_VIEW_HPP

#include <cstddef>
#include <cstdint>

#include "bytes.hpp"
#include "rtp.hpp"
#include "st2110_20.hpp"

namespace st2110 {

    inline constexpr std::size_t maxPacketSrdSegments = 3;

    struct SrdSegmentView {
        SrdHeader header{};
        ByteSpan data{};
    };

    struct PacketView {
        RtpHeaderView rtp{};
        uint32_t extended_seq = 0;

        SrdSegmentView segments[maxPacketSrdSegments]{};
        uint8_t segment_count = 0;

        ByteSpan payload_data{};
    };

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_PACKET_VIEW_HPP
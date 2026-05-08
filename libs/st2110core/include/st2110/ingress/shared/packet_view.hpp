#ifndef ST2110_OBS_PLUGIN_PACKET_VIEW_HPP
#define ST2110_OBS_PLUGIN_PACKET_VIEW_HPP

#include <cstddef>
#include <cstdint>
#include <expected>

#include "st2110/foundation/bytes.hpp"
#include "st2110/foundation/error.hpp"
#include "packet_parse_stats.hpp"
#include "rtp.hpp"
#include "st2110_20.hpp"

namespace st2110 {

inline constexpr std::size_t maxPacketSrdSegments = 3;

struct SrdSegmentView {
    SrdHeader header{};
    ByteSpan data{};
};

struct PacketViewParseFailure {
    Error error = Error::Ok;
    PacketParseStage stage = PacketParseStage::RtpHeader;
};

struct PacketView {
    RtpHeaderView rtp{};
    uint32_t extended_seq = 0;

    SrdSegmentView segments[maxPacketSrdSegments]{};
    uint8_t segment_count = 0;

    ByteSpan payload_data{};
    ByteSpan trailing_padding{};

    static std::expected<PacketView, Error> from_udp_datagram(ByteSpan udp_payload);
};

[[nodiscard]] inline std::expected<PacketView, PacketViewParseFailure> parse_packet_view_staged(ByteSpan udp_payload) {
    PacketView res{};

    std::expected<RtpHeaderView, Error> rtp_header = parse_rtp_header(udp_payload);
    if (!rtp_header.has_value()) {
        return std::unexpected(PacketViewParseFailure{rtp_header.error(), PacketParseStage::RtpHeader});
    }
    res.rtp = *rtp_header;

    ByteSpan rtp_payload = rtp_payload_span(udp_payload, *rtp_header);

    std::expected<St2110PayloadHeaderView, Error> st2110_20_payload_header =
        parse_st2110_20_payload_header(rtp_payload);
    if (!st2110_20_payload_header.has_value()) {
        return std::unexpected(
            PacketViewParseFailure{st2110_20_payload_header.error(), PacketParseStage::St2110PayloadHeaderParse});
    }

    if (Error err = validate_st2110_20_payload_header(*st2110_20_payload_header); err != Error::Ok) {
        return std::unexpected(PacketViewParseFailure{err, PacketParseStage::St2110PayloadHeaderValidate});
    }

    res.extended_seq = combine_extended_seq(st2110_20_payload_header->ext_seq, rtp_header->seq_number);
    res.payload_data = rtp_payload.subspan(st2110_20_payload_header->header_bytes);

    std::size_t sum_length = 0;

    for (std::size_t i = 0; i < st2110_20_payload_header->srd_count; ++i) {
        res.segments[i].header = st2110_20_payload_header->srd[i];
        sum_length += st2110_20_payload_header->srd[i].length;
    }

    if (st2110_20_payload_header->srd_count == 1 && st2110_20_payload_header->srd[0].length == 0) {
        if (res.payload_data.size() != 0) {
            return std::unexpected(PacketViewParseFailure{Error::InvalidValue, PacketParseStage::SrdPayloadSplit});
        }
    }

    if (res.payload_data.size() < sum_length) {
        return std::unexpected(PacketViewParseFailure{Error::ShortPacket, PacketParseStage::SrdPayloadSplit});
    }

    res.trailing_padding = res.payload_data.subspan(sum_length);

    std::size_t segment_size = 0;

    for (std::size_t i = 0; i < st2110_20_payload_header->srd_count; ++i) {
        res.segments[i].data = res.payload_data.subspan(segment_size, st2110_20_payload_header->srd[i].length);
        segment_size += st2110_20_payload_header->srd[i].length;
    }

    res.segment_count = st2110_20_payload_header->srd_count;

    return res;
}

inline std::expected<PacketView, Error> PacketView::from_udp_datagram(ByteSpan udp_payload) {
    auto res = parse_packet_view_staged(udp_payload);
    if (!res.has_value()) {
        return std::unexpected(res.error().error);
    }

    return *res;
}

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_PACKET_VIEW_HPP
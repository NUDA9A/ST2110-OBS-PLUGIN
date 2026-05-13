#ifndef ST2110_OBS_PLUGIN_PACKET_PARSE_HPP
#define ST2110_OBS_PLUGIN_PACKET_PARSE_HPP

#include <st2110/foundation/bytes.hpp>
#include <st2110/foundation/error.hpp>
#include <st2110/ingress/shared/packet_parse_stats.hpp>
#include <st2110/receive/video/video_packet_view.hpp>

#include <cstddef>
#include <expected>

namespace st2110 {
struct PacketViewParseFailure {
    Error error = Error::Ok;
    PacketParseStage stage = PacketParseStage::RtpHeader;
};

inline constexpr std::size_t udpHeaderBytes = 8;

[[nodiscard]] inline std::size_t udp_datagram_size_bytes(const ByteSpan udp_payload) {
    return udp_payload.size() + udpHeaderBytes;
}

[[nodiscard]] inline Error validate_packet_parse_policy(const ByteSpan udp_payload, const std::size_t maxudp) {
    if (udp_datagram_size_bytes(udp_payload) > maxudp) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

[[nodiscard]] inline std::expected<VideoPacketView, PacketViewParseFailure>
parse_packet_view_staged(const ByteSpan udp_payload) {
    VideoPacketView res{};

    std::expected<RtpHeaderView, Error> rtp_header = parse_rtp_header(udp_payload);
    if (!rtp_header.has_value()) {
        return std::unexpected(PacketViewParseFailure{rtp_header.error(), PacketParseStage::RtpHeader});
    }
    res.rtp = *rtp_header;

    const ByteSpan rtp_payload = rtp_payload_span(udp_payload, *rtp_header);

    std::expected<St2110PayloadHeaderView, Error> st2110_20_payload_header =
        parse_st2110_20_payload_header(rtp_payload);
    if (!st2110_20_payload_header.has_value()) {
        return std::unexpected(
            PacketViewParseFailure{st2110_20_payload_header.error(), PacketParseStage::St2110PayloadHeaderParse});
    }

    if (const Error err = validate_st2110_20_payload_header(*st2110_20_payload_header); err != Error::Ok) {
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
        if (!res.payload_data.empty()) {
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

[[nodiscard]] inline std::expected<VideoPacketView, Error> parse_packet_view(const ByteSpan udp_payload) {
    auto staged = parse_packet_view_staged(udp_payload);
    if (!staged.has_value()) {
        return std::unexpected(staged.error().error);
    }
    return *staged;
}

[[nodiscard]] inline std::expected<VideoPacketView, Error>
parse_packet_view(const ByteSpan udp_payload, PacketParseStats &stats, const std::size_t maxudp) {
    if (Error err = validate_packet_parse_policy(udp_payload, maxudp); err != Error::Ok) {
        record_packet_parse_result(stats, err, PacketParseStage::PacketPolicy);
        return std::unexpected(err);
    }

    auto res = parse_packet_view_staged(udp_payload);
    if (!res.has_value()) {
        record_packet_parse_result(stats, res.error().error, res.error().stage);
        return std::unexpected(res.error().error);
    }

    record_packet_parse_result(stats, Error::Ok, PacketParseStage::RtpHeader);

    return *res;
}

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_PACKET_PARSE_HPP

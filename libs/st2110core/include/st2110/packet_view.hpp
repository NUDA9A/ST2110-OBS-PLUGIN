#ifndef ST2110_OBS_PLUGIN_PACKET_VIEW_HPP
#define ST2110_OBS_PLUGIN_PACKET_VIEW_HPP

#include <cstddef>
#include <cstdint>
#include <expected>

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

        inline static std::expected<PacketView, Error> from_udp_datagram(ByteSpan udp_payload) {
            PacketView res{};

            std::expected<RtpHeaderView, Error> rtp_header = parse_rtp_header(udp_payload);
            if (!rtp_header.has_value()) {
                return std::unexpected(rtp_header.error());
            }
            res.rtp = rtp_header.value();
            auto rtp_payload = rtp_payload_span(udp_payload, rtp_header.value());
            auto st2110_20_payload_header = parse_st2110_20_payload_header(rtp_payload);
            if (!st2110_20_payload_header.has_value()) {
                return std::unexpected(st2110_20_payload_header.error());
            }
            auto err = validate_st2110_20_payload_header(st2110_20_payload_header.value());
            if (err != Error::Ok) {
                return std::unexpected(err);
            }
            res.extended_seq = combine_extended_seq(st2110_20_payload_header.value().ext_seq, rtp_header.value().seq_number);

            res.payload_data = rtp_payload.subspan(st2110_20_payload_header.value().header_bytes);

            size_t sum_length = 0;

            for (size_t i = 0; i < st2110_20_payload_header.value().srd_count; ++i) {
                res.segments[i].header = st2110_20_payload_header.value().srd[i];
                sum_length += st2110_20_payload_header.value().srd[i].length;
            }

            if (st2110_20_payload_header.value().srd_count == 1 && st2110_20_payload_header.value().srd[0].length == 0) {
                if (res.payload_data.size() != 0) {
                    return std::unexpected(Error::InvalidValue);
                }
            }

            if (res.payload_data.size() < sum_length) {
                return std::unexpected(Error::ShortPacket);
            }

            size_t segment_size = 0;

            for (size_t i = 0; i < st2110_20_payload_header.value().srd_count; ++i) {
                res.segments[i].data = res.payload_data.subspan(segment_size, st2110_20_payload_header.value().srd[i].length);
                segment_size += st2110_20_payload_header.value().srd[i].length;
            }

            res.segment_count = st2110_20_payload_header.value().srd_count;

            return res;
        }
    };
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_PACKET_VIEW_HPP
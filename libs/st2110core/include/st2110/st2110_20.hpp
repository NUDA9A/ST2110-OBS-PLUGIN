#ifndef ST2110_OBS_PLUGIN_ST2110_20_HPP
#define ST2110_OBS_PLUGIN_ST2110_20_HPP

#include <cstdint>
#include <expected>
#include "error.hpp"
#include "bytes.hpp"
#include "endian.hpp"

namespace st2110 {
    struct ExtendedSequenceNumber {
        uint16_t hi16;
    };

    struct SrdHeader {
        uint16_t length;
        uint16_t row_number;
        uint16_t offset;
        bool field_id;
        bool continuation;
    };

    struct St2110PayloadHeaderView {
        ExtendedSequenceNumber ext_seq;
        SrdHeader srd[3];
        uint8_t srd_count;
        std::size_t header_bytes;
    };

    std::expected<St2110PayloadHeaderView, Error> parse_st2110_20_payload_header(ByteSpan payload) {
        if (payload.size() < 8) {
            return std::unexpected(Error::ShortPacket);
        }
        std::size_t offset = 0;
        ExtendedSequenceNumber ext_seq{};
        ext_seq.hi16 = endian::read_be16(payload.subspan(offset, 2));
        offset += 2;
        St2110PayloadHeaderView res{};
        res.ext_seq = ext_seq;
        uint8_t srd_count = 0;
        while (true) {
            if (payload.size() - offset < 6) {
                return std::unexpected(Error::ShortPacket);
            }
            SrdHeader srdHeader{};

            srdHeader.length = endian::read_be16(payload.subspan(offset, 2));

            uint16_t F_and_row_number = endian::read_be16(payload.subspan(offset + 2, 2));
            srdHeader.field_id = F_and_row_number >> 15;
            srdHeader.row_number = F_and_row_number & 0x7FFF;

            uint16_t C_and_offset = endian::read_be16(payload.subspan(offset + 4, 2));
            srdHeader.continuation = C_and_offset >> 15;
            srdHeader.offset = C_and_offset & 0x7FFF;

            res.srd[srd_count] = srdHeader;

            offset += 6;

            srd_count++;
            if (srd_count == 3) {
                if (srdHeader.continuation) {
                    return std::unexpected(Error::InvalidValue);
                }
                break;
            } else if (!srdHeader.continuation) {
                break;
            }
        }
        res.srd_count = srd_count;
        res.header_bytes = offset;
        return res;
    }
}
#endif //ST2110_OBS_PLUGIN_ST2110_20_HPP

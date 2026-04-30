#ifndef ST2110_OBS_PLUGIN_RTP_HPP
#define ST2110_OBS_PLUGIN_RTP_HPP

#include "bytes.hpp"
#include "endian.hpp"
#include "error.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>

namespace st2110 {
struct RtpHeaderView {
    uint8_t version;
    bool padding_flag;
    bool extension_flag;
    uint8_t csrc_count;
    bool marker;
    uint8_t payload_type;
    uint16_t seq_number;
    uint32_t timestamp;
    uint32_t ssrc;
    size_t payload_offset;
    size_t payload_len;
};

inline std::expected<RtpHeaderView, Error> parse_rtp_header(ByteSpan udp_payload) {
    if (udp_payload.size() < 12) {
        return std::unexpected(Error::ShortPacket);
    }
    uint8_t version = (udp_payload[0] >> 6);
    if (version != 2) {
        return std::unexpected(Error::BadRTPVersion);
    }
    bool padding_flag = ((udp_payload[0] >> 5) & 1);
    bool extension_flag = ((udp_payload[0] >> 4) & 1);
    uint8_t csrc_count = udp_payload[0] & 0x0F;
    bool marker = (udp_payload[1] >> 7);
    uint8_t payload_type = udp_payload[1] & 0x7F;
    uint16_t seq_number = endian::read_be16(udp_payload.subspan(2, 2));
    uint32_t timestamp = endian::read_be32(udp_payload.subspan(4, 4));
    uint32_t ssrc = endian::read_be32(udp_payload.subspan(8, 4));
    size_t offset = 12;
    if (udp_payload.size() < offset + 4 * csrc_count) {
        return std::unexpected(Error::ShortPacket);
    }
    offset += 4 * csrc_count;
    if (extension_flag) {
        if (udp_payload.size() < offset + 4) {
            return std::unexpected(Error::ShortPacket);
        }
        uint16_t len_words = endian::read_be16(udp_payload.subspan(offset + 2, 2));
        size_t ext_bytes = len_words * 4;
        if (udp_payload.size() < offset + 4 + ext_bytes) {
            return std::unexpected(Error::ShortPacket);
        }
        offset += 4 + ext_bytes;
    }
    size_t pad = 0;
    size_t payload_len;
    if (padding_flag) {
        pad = udp_payload.back();
        if (pad == 0 || pad > udp_payload.size() - offset) {
            return std::unexpected(Error::InvalidValue);
        }
        payload_len = udp_payload.size() - pad - offset;
    } else {
        payload_len = udp_payload.size() - offset;
    }

    return RtpHeaderView{version,    padding_flag, extension_flag, csrc_count, marker,     payload_type,
                         seq_number, timestamp,    ssrc,           offset,     payload_len};
}

inline bool seq_less(uint16_t a, uint16_t b) {
    uint16_t forward_dst = b - a;
    return forward_dst != 0 && forward_dst < 32768;
}

inline int32_t seq_distance(uint16_t a, uint16_t b) {
    if (a == b) {
        return 0;
    }
    uint16_t forward_dst = b - a;
    int32_t res = static_cast<int32_t>(forward_dst);
    if (!seq_less(a, b)) {
        res = static_cast<int32_t>(forward_dst - 65536);
    }
    return res;
}

inline ByteSpan rtp_payload_span(ByteSpan udp_payload, const RtpHeaderView &header) {
    return udp_payload.subspan(header.payload_offset, header.payload_len);
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_RTP_HPP

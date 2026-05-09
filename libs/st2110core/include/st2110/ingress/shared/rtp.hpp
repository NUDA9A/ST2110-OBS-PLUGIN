#ifndef ST2110_OBS_PLUGIN_RTP_HPP
#define ST2110_OBS_PLUGIN_RTP_HPP

#include <cstddef>
#include <cstdint>
#include <expected>

#include <st2110/foundation/bytes.hpp>
#include <st2110/foundation/endian.hpp>
#include <st2110/foundation/error.hpp>

namespace st2110 {
struct RtpHeaderView {
    std::uint8_t version;
    bool padding_flag;
    bool extension_flag;
    std::uint8_t csrc_count;
    bool marker;
    std::uint8_t payload_type;
    std::uint16_t seq_number;
    std::uint32_t timestamp;
    std::uint32_t ssrc;
    std::size_t payload_offset;
    std::size_t payload_len;
};

inline std::expected<RtpHeaderView, Error> parse_rtp_header(ByteSpan udp_payload) {
    if (udp_payload.size() < 12) {
        return std::unexpected(Error::ShortPacket);
    }
    const std::uint8_t version = (udp_payload[0] >> 6);
    if (version != 2) {
        return std::unexpected(Error::BadRTPVersion);
    }
    const bool padding_flag = ((udp_payload[0] >> 5) & 1);
    const bool extension_flag = ((udp_payload[0] >> 4) & 1);
    const std::uint8_t csrc_count = udp_payload[0] & 0x0F;
    const bool marker = (udp_payload[1] >> 7);
    const std::uint8_t payload_type = udp_payload[1] & 0x7F;
    const std::uint16_t seq_number = endian::read_be16(udp_payload.subspan(2, 2));
    const std::uint32_t timestamp = endian::read_be32(udp_payload.subspan(4, 4));
    const std::uint32_t ssrc = endian::read_be32(udp_payload.subspan(8, 4));
    std::size_t offset = 12;
    if (udp_payload.size() < offset + 4 * csrc_count) {
        return std::unexpected(Error::ShortPacket);
    }
    offset += 4 * csrc_count;
    if (extension_flag) {
        if (udp_payload.size() < offset + 4) {
            return std::unexpected(Error::ShortPacket);
        }
        const std::uint16_t len_words = endian::read_be16(udp_payload.subspan(offset + 2, 2));
        const std::size_t ext_bytes = len_words * 4;
        if (udp_payload.size() < offset + 4 + ext_bytes) {
            return std::unexpected(Error::ShortPacket);
        }
        offset += 4 + ext_bytes;
    }
    std::size_t pad = 0;
    std::size_t payload_len;
    if (padding_flag) {
        pad = udp_payload.back();
        if (pad == 0 || pad > udp_payload.size() - offset) {
            return std::unexpected(Error::InvalidValue);
        }
        payload_len = udp_payload.size() - pad - offset;
    } else {
        payload_len = udp_payload.size() - offset;
    }

    return RtpHeaderView{.version = version,
                         .padding_flag = padding_flag,
                         .extension_flag = extension_flag,
                         .csrc_count = csrc_count,
                         .marker = marker,
                         .payload_type = payload_type,
                         .seq_number = seq_number,
                         .timestamp = timestamp,
                         .ssrc = ssrc,
                         .payload_offset = offset,
                         .payload_len = payload_len};
}

inline bool seq_less(const std::uint16_t a, const std::uint16_t b) {
    const std::uint16_t forward_dst = b - a;
    return forward_dst != 0 && forward_dst < 32768;
}

inline std::int32_t seq_distance(const std::uint16_t a, const std::uint16_t b) {
    if (a == b) {
        return 0;
    }
    const std::uint16_t forward_dst = b - a;
    auto res = static_cast<std::int32_t>(forward_dst);
    if (!seq_less(a, b)) {
        res = static_cast<std::int32_t>(forward_dst - 65536);
    }
    return res;
}

inline ByteSpan rtp_payload_span(ByteSpan udp_payload, const RtpHeaderView &header) {
    return udp_payload.subspan(header.payload_offset, header.payload_len);
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_RTP_HPP

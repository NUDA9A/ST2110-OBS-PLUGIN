#include <array>
#include <cstdint>

#include <st2110/foundation/bytes.hpp>
#include <st2110/foundation/endian.hpp>
#include <st2110/foundation/error.hpp>
#include <st2110/ingress/shared/st2110_20.hpp>

st2110::Error odr_helper_b() {
    const std::array<uint8_t, 8> payload = {
        0xAB, 0xCD, // ext seq hi16
        0x00, 0x08, // SRD length
        0x00, 0x01, // F=0, row=1
        0x00, 0x04  // C=0, offset=4
    };

    const st2110::ByteSpan bytes(payload.data(), payload.size());

    const auto ext = st2110::endian::read_be16(bytes.subspan(0, 2));
    if (ext != 0xABCDu) {
        return st2110::Error::InvalidValue;
    }

    const auto parsed = st2110::parse_st2110_20_payload_header(bytes);
    if (!parsed.has_value()) {
        return parsed.error();
    }

    if (parsed->ext_seq.hi16 != 0xABCDu) {
        return st2110::Error::InvalidValue;
    }
    if (parsed->srd_count != 1u) {
        return st2110::Error::InvalidValue;
    }
    if (parsed->srd[0].length != 8u) {
        return st2110::Error::InvalidValue;
    }
    if (parsed->srd[0].row_number != 1u) {
        return st2110::Error::InvalidValue;
    }
    if (parsed->srd[0].offset != 4u) {
        return st2110::Error::InvalidValue;
    }

    const auto validate_err = st2110::validate_st2110_20_payload_header(*parsed);
    if (validate_err != st2110::Error::Ok) {
        return validate_err;
    }

    const uint32_t seq = st2110::combine_extended_seq(parsed->ext_seq, 0x0001u);
    if (seq != 0xABCD0001u) {
        return st2110::Error::InvalidValue;
    }

    const auto ext32 = st2110::endian::read_be32(st2110::ByteSpan{payload.data(), 4});
    if (ext32 != 0xABCD0008u) {
        return st2110::Error::InvalidValue;
    }

    return st2110::Error::Ok;
}
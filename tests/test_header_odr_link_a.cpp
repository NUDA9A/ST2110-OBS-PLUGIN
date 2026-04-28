#include <array>
#include <cstdint>

#include <st2110/bytes.hpp>
#include <st2110/endian.hpp>
#include <st2110/error.hpp>
#include <st2110/st2110_20.hpp>

st2110::Error odr_helper_a() {
  const std::array<uint8_t, 8> payload = {
      0x12, 0x34, // ext seq hi16
      0x00, 0x04, // SRD length
      0x00, 0x02, // F=0, row=2
      0x00, 0x06  // C=0, offset=6
  };

  const st2110::ByteSpan bytes(payload.data(), payload.size());

  const auto ext = st2110::endian::read_be16(bytes.subspan(0, 2));
  if (ext != 0x1234u) {
    return st2110::Error::InvalidValue;
  }

  const auto parsed = st2110::parse_st2110_20_payload_header(bytes);
  if (!parsed.has_value()) {
    return parsed.error();
  }

  if (parsed->ext_seq.hi16 != 0x1234u) {
    return st2110::Error::InvalidValue;
  }
  if (parsed->srd_count != 1u) {
    return st2110::Error::InvalidValue;
  }
  if (parsed->srd[0].length != 4u) {
    return st2110::Error::InvalidValue;
  }
  if (parsed->srd[0].row_number != 2u) {
    return st2110::Error::InvalidValue;
  }
  if (parsed->srd[0].offset != 6u) {
    return st2110::Error::InvalidValue;
  }

  const auto validate_err = st2110::validate_st2110_20_payload_header(*parsed);
  if (validate_err != st2110::Error::Ok) {
    return validate_err;
  }

  const uint32_t seq = st2110::combine_extended_seq(parsed->ext_seq, 0x5678u);
  if (seq != 0x12345678u) {
    return st2110::Error::InvalidValue;
  }

  return st2110::Error::Ok;
}
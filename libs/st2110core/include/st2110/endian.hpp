#ifndef ST2110_OBS_PLUGIN_ENDIAN_HPP
#define ST2110_OBS_PLUGIN_ENDIAN_HPP

#include <cassert>
#include <cstdint>
#include <span>

namespace st2110::endian {
inline uint16_t read_be16(const std::span<const uint8_t> &s) {
  assert(s.size() >= 2);
  return (static_cast<uint16_t>(s[0]) << 8) | static_cast<uint16_t>(s[1]);
}

inline uint32_t read_be32(const std::span<const uint8_t> &s) {
  assert(s.size() >= 4);
  return (static_cast<uint32_t>(s[0]) << 24) | (static_cast<uint32_t>(s[1]) << 16) |
         (static_cast<uint32_t>(s[2]) << 8) | static_cast<uint32_t>(s[3]);
}

} // namespace st2110::endian

#endif // ST2110_OBS_PLUGIN_ENDIAN_HPP

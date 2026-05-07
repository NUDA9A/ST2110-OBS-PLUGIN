#ifndef ST2110_OBS_PLUGIN_BYTES_HPP
#define ST2110_OBS_PLUGIN_BYTES_HPP

#include <cstdint>
#include <span>

namespace st2110 {
using ByteSpan = std::span<const uint8_t>;
}

#endif // ST2110_OBS_PLUGIN_BYTES_HPP

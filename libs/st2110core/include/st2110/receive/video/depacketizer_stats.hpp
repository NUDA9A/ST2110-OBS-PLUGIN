#ifndef ST2110_OBS_PLUGIN_DEPACKETIZER_STATS_HPP
#define ST2110_OBS_PLUGIN_DEPACKETIZER_STATS_HPP

#include <cstdint>

namespace st2110 {

struct DepacketizerStats {
    uint64_t packets_in = 0;
    uint64_t packets_used = 0;

    uint64_t units_ok = 0;
    uint64_t units_partial = 0;
    uint64_t units_dropped = 0;
};

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_DEPACKETIZER_STATS_HPP
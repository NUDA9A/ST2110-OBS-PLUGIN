#ifndef ST2110_OBS_PLUGIN_STATS_HPP
#define ST2110_OBS_PLUGIN_STATS_HPP

#include <cstdint>

namespace st2110 {

inline void record_result_counter(uint64_t &total, uint64_t &ok, uint64_t &failed, bool success) noexcept {
    ++total;
    if (success) {
        ++ok;
    } else {
        ++failed;
    }
}

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_STATS_HPP
#ifndef ST2110_OBS_PLUGIN_REORDER_STATS_HPP
#define ST2110_OBS_PLUGIN_REORDER_STATS_HPP

#include <cstdint>

namespace st2110 {

struct ReorderBufferStats {
    uint64_t packets_pushed = 0;
    uint64_t packets_stored = 0;
    uint64_t packets_popped = 0;

    uint64_t duplicates = 0;
    uint64_t out_of_window = 0;
    uint64_t late_packets = 0;
    uint64_t missing_seq = 0;
    uint64_t missing_seq_flushed = 0;
};

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_REORDER_STATS_HPP
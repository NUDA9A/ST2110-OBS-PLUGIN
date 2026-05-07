#ifndef ST2110_OBS_PLUGIN_AUDIO_STATS_HPP
#define ST2110_OBS_PLUGIN_AUDIO_STATS_HPP

#include "st2110/foundation/error.hpp"

#include <cstdint>

namespace st2110 {
struct AudioReceiveStats {
    uint64_t packets_ok = 0;
    uint64_t packets_lost = 0;
    uint64_t packets_rejected = 0;

    uint64_t blocks_ok = 0;
    uint64_t blocks_partial = 0;
    uint64_t blocks_dropped = 0;
};

enum class AudioBlockCompletionStatus { Complete, Partial, Dropped };

[[nodiscard]] inline Error validate_audio_block_completion_status(AudioBlockCompletionStatus status) {
    switch (status) {
    case AudioBlockCompletionStatus::Complete:
    case AudioBlockCompletionStatus::Partial:
    case AudioBlockCompletionStatus::Dropped:
        return Error::Ok;
    }

    return Error::InvalidValue;
}

inline void record_audio_packet_ok(AudioReceiveStats &stats) { ++stats.packets_ok; }

inline void record_audio_packet_lost(AudioReceiveStats &stats) { ++stats.packets_lost; }

inline void record_audio_packet_rejected(AudioReceiveStats &stats) { ++stats.packets_rejected; }

[[nodiscard]] inline Error record_audio_block_result(AudioReceiveStats &stats, AudioBlockCompletionStatus status) {
    const Error status_error = validate_audio_block_completion_status(status);
    if (status_error != Error::Ok) {
        return status_error;
    }

    switch (status) {
    case AudioBlockCompletionStatus::Complete:
        ++stats.blocks_ok;
        return Error::Ok;

    case AudioBlockCompletionStatus::Partial:
        ++stats.blocks_partial;
        return Error::Ok;

    case AudioBlockCompletionStatus::Dropped:
        ++stats.blocks_dropped;
        return Error::Ok;
    }

    return Error::InvalidValue;
}

inline void reset_audio_receive_stats(AudioReceiveStats &stats) { stats = {}; }
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_AUDIO_STATS_HPP
#ifndef ST2110_OBS_PLUGIN_STATS_HPP
#define ST2110_OBS_PLUGIN_STATS_HPP

#include <cstdint>

#include "error.hpp"

namespace st2110 {

    struct ParserStats {
        uint64_t packets_total = 0;
        uint64_t packets_ok = 0;
        uint64_t packets_failed = 0;

        uint64_t short_packet = 0;
        uint64_t bad_rtp_version = 0;
        uint64_t invalid_value = 0;
        uint64_t unsupported = 0;
        uint64_t buffer_too_small = 0;
        uint64_t other_error = 0;
    };

    inline void record_parse_result(ParserStats& stats, Error err) {
        ++stats.packets_total;

        if (err == Error::Ok) {
            ++stats.packets_ok;
            return;
        }

        ++stats.packets_failed;

        switch (err) {
            case Error::ShortPacket:
                ++stats.short_packet;
                break;
            case Error::BadRTPVersion:
                ++stats.bad_rtp_version;
                break;
            case Error::InvalidValue:
                ++stats.invalid_value;
                break;
            case Error::Unsupported:
                ++stats.unsupported;
                break;
            case Error::BufferTooSmall:
                ++stats.buffer_too_small;
                break;
            case Error::Ok:
                break;
            default:
                ++stats.other_error;
                break;
        }
    }

    struct DepacketizerStats {
        uint64_t packets_in = 0;
        uint64_t packets_accepted = 0;
        uint64_t packets_rejected = 0;

        uint64_t units_completed = 0;
        uint64_t units_partial = 0;
        uint64_t units_dropped = 0;
    };

    struct BackendStats {
        uint64_t datagrams_received = 0;
        uint64_t bytes_received = 0;
        uint64_t datagrams_dropped = 0;
        uint64_t media_units_delivered = 0;
    };

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_STATS_HPP
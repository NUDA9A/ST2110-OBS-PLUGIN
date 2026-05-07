#ifndef ST2110_OBS_PLUGIN_STATS_HPP
#define ST2110_OBS_PLUGIN_STATS_HPP

#include <cstdint>

#include "error.hpp"

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

enum class PacketParseStage {
    RtpHeader,
    St2110PayloadHeaderParse,
    St2110PayloadHeaderValidate,
    SrdPayloadSplit,
    PacketPolicy
};

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

struct PacketParseStats {
    ParserStats parser_stats{};

    uint64_t rtp_header_fail = 0;
    uint64_t st2110_header_parse_fail = 0;
    uint64_t bad_srd = 0;
    uint64_t srd_payload_split_fail = 0;
    uint64_t packet_policy_fail = 0;
};

inline void record_parse_result(ParserStats &stats, Error err) {
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
    uint64_t packets_used = 0;

    uint64_t units_ok = 0;
    uint64_t units_partial = 0;
    uint64_t units_dropped = 0;
};

struct BackendStats {
    uint64_t datagrams_received = 0;
    uint64_t bytes_received = 0;

    uint64_t control_datagrams_ignored = 0;
    uint64_t nonmedia_datagrams_ignored = 0;

    uint64_t packets_parsed_ok = 0;
    uint64_t packets_rejected = 0;

    uint64_t frames_delivered = 0;
    uint64_t datagrams_dropped = 0;
    uint64_t media_units_delivered = 0;

    PacketParseStats packet_parse{};
    ReorderBufferStats reorder{};
    DepacketizerStats depacketizer{};
};

inline void record_packet_parse_result(PacketParseStats &stats, Error err, PacketParseStage stage) {
    record_parse_result(stats.parser_stats, err);
    if (err == Error::Ok) {
        return;
    }

    switch (stage) {
    case PacketParseStage::RtpHeader:
        ++stats.rtp_header_fail;
        break;
    case PacketParseStage::St2110PayloadHeaderParse:
        ++stats.st2110_header_parse_fail;
        break;
    case PacketParseStage::St2110PayloadHeaderValidate:
        ++stats.bad_srd;
        break;
    case PacketParseStage::SrdPayloadSplit:
        ++stats.srd_payload_split_fail;
        break;
    case PacketParseStage::PacketPolicy:
        ++stats.packet_policy_fail;
        break;
    }
}

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_STATS_HPP
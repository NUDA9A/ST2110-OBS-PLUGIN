#ifndef ST2110_OBS_PLUGIN_PACKET_PARSE_STATS_HPP
#define ST2110_OBS_PLUGIN_PACKET_PARSE_STATS_HPP

#include <st2110/foundation/error.hpp>

#include <cstdint>

namespace st2110 {

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

inline void record_result_counter(std::uint64_t &total, std::uint64_t &ok, std::uint64_t &failed,
                                  const bool success) noexcept {
    ++total;
    if (success) {
        ++ok;
    } else {
        ++failed;
    }
}

inline void record_parse_result(ParserStats &stats, Error err) {
    record_result_counter(stats.packets_total, stats.packets_ok, stats.packets_failed, err == Error::Ok);

    if (err == Error::Ok) {
        return;
    }

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

struct PacketParseStats {
    ParserStats parser_stats{};

    uint64_t rtp_header_fail = 0;
    uint64_t st2110_header_parse_fail = 0;
    uint64_t bad_srd = 0;
    uint64_t srd_payload_split_fail = 0;
    uint64_t packet_policy_fail = 0;
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

#endif // ST2110_OBS_PLUGIN_PACKET_PARSE_STATS_HPP
#include <cassert>
#include <cstdint>
#include <vector>

#include <st2110/bytes.hpp>
#include <st2110/error.hpp>
#include <st2110/packet_parse.hpp>
#include <st2110/stats.hpp>

static std::vector<uint8_t> make_valid_packet() {
    return {// RTP header
            0x80, 0x70, 0x00, 0x01, 0x00, 0x00, 0x00, 0x2A, 0x12, 0x34, 0x56, 0x78,

            // ST 2110-20 payload header
            0x00, 0x00, // ext seq hi16
            0x00, 0x04, // SRD length = 4
            0x00, 0x00, // F=0, row=0
            0x00, 0x00, // C=0, offset=0

            // payload
            0xAA, 0xBB, 0xCC, 0xDD};
}

static std::vector<uint8_t> make_bad_rtp_version_packet() {
    auto bytes = make_valid_packet();
    bytes[0] = 0x40; // version = 1
    return bytes;
}

static std::vector<uint8_t> make_short_st2110_payload_header_packet() {
    return {// RTP header
            0x80, 0x70, 0x00, 0x01, 0x00, 0x00, 0x00, 0x2A, 0x12, 0x34, 0x56, 0x78,

            // too short for ST 2110-20 payload header
            0x00, 0x00, 0x00, 0x04};
}

static std::vector<uint8_t> make_bad_srd_validation_packet() {
    return {// RTP header
            0x80, 0x70, 0x00, 0x01, 0x00, 0x00, 0x00, 0x2A, 0x12, 0x34, 0x56, 0x78,

            // ext seq
            0x00, 0x00,

            // SRD #1: len=4, row=0, C=1, offset=0
            0x00, 0x04, 0x00, 0x00, 0x80, 0x00,

            // SRD #2: len=0 (invalid for multi-SRD), row=1, C=0, offset=0
            0x00, 0x00, 0x00, 0x01, 0x00, 0x00,

            // payload bytes for SRD #1 only
            0xAA, 0xBB, 0xCC, 0xDD};
}

static std::vector<uint8_t> make_short_srd_payload_packet() {
    return {// RTP header
            0x80, 0x70, 0x00, 0x01, 0x00, 0x00, 0x00, 0x2A, 0x12, 0x34, 0x56, 0x78,

            // ST 2110-20 payload header
            0x00, 0x00, // ext seq hi16
            0x00, 0x08, // SRD length = 8
            0x00, 0x00, // F=0, row=0
            0x00, 0x00, // C=0, offset=0

            // only 4 bytes instead of 8
            0xAA, 0xBB, 0xCC, 0xDD};
}

static void test_success_updates_ok_counters_once() {
    const auto bytes = make_valid_packet();

    st2110::PacketParseStats stats{};
    const auto parsed = st2110::parse_packet_view(st2110::ByteSpan(bytes.data(), bytes.size()), stats);

    assert(parsed.has_value());

    assert(stats.parser_stats.packets_total == 1u);
    assert(stats.parser_stats.packets_ok == 1u);
    assert(stats.parser_stats.packets_failed == 0u);

    assert(stats.packet_policy_fail == 0u);
    assert(stats.rtp_header_fail == 0u);
    assert(stats.st2110_header_parse_fail == 0u);
    assert(stats.bad_srd == 0u);
    assert(stats.srd_payload_split_fail == 0u);
}

static void test_policy_failure_is_accounted_separately() {
    const auto bytes = make_valid_packet();

    st2110::PacketParseStats stats{};
    st2110::PacketParsePolicy policy{};
    policy.max_udp_datagram_bytes = bytes.size() - 1;

    const auto parsed = st2110::parse_packet_view(st2110::ByteSpan(bytes.data(), bytes.size()), stats, policy);

    assert(!parsed.has_value());
    assert(parsed.error() == st2110::Error::InvalidValue);

    assert(stats.parser_stats.packets_total == 1u);
    assert(stats.parser_stats.packets_ok == 0u);
    assert(stats.parser_stats.packets_failed == 1u);
    assert(stats.parser_stats.invalid_value == 1u);

    assert(stats.packet_policy_fail == 1u);
    assert(stats.rtp_header_fail == 0u);
    assert(stats.st2110_header_parse_fail == 0u);
    assert(stats.bad_srd == 0u);
    assert(stats.srd_payload_split_fail == 0u);
}

static void test_rtp_header_failure_is_accounted() {
    const auto bytes = make_bad_rtp_version_packet();

    st2110::PacketParseStats stats{};
    const auto parsed = st2110::parse_packet_view(st2110::ByteSpan(bytes.data(), bytes.size()), stats);

    assert(!parsed.has_value());
    assert(parsed.error() == st2110::Error::BadRTPVersion);

    assert(stats.parser_stats.packets_total == 1u);
    assert(stats.parser_stats.packets_failed == 1u);
    assert(stats.parser_stats.bad_rtp_version == 1u);

    assert(stats.packet_policy_fail == 0u);
    assert(stats.rtp_header_fail == 1u);
    assert(stats.st2110_header_parse_fail == 0u);
    assert(stats.bad_srd == 0u);
    assert(stats.srd_payload_split_fail == 0u);
}

static void test_st2110_payload_header_parse_failure_is_accounted() {
    const auto bytes = make_short_st2110_payload_header_packet();

    st2110::PacketParseStats stats{};
    const auto parsed = st2110::parse_packet_view(st2110::ByteSpan(bytes.data(), bytes.size()), stats);

    assert(!parsed.has_value());
    assert(parsed.error() == st2110::Error::ShortPacket);

    assert(stats.parser_stats.packets_total == 1u);
    assert(stats.parser_stats.packets_failed == 1u);
    assert(stats.parser_stats.short_packet == 1u);

    assert(stats.packet_policy_fail == 0u);
    assert(stats.rtp_header_fail == 0u);
    assert(stats.st2110_header_parse_fail == 1u);
    assert(stats.bad_srd == 0u);
    assert(stats.srd_payload_split_fail == 0u);
}

static void test_st2110_payload_header_validation_failure_is_accounted() {
    const auto bytes = make_bad_srd_validation_packet();

    st2110::PacketParseStats stats{};
    const auto parsed = st2110::parse_packet_view(st2110::ByteSpan(bytes.data(), bytes.size()), stats);

    assert(!parsed.has_value());
    assert(parsed.error() == st2110::Error::InvalidValue);

    assert(stats.parser_stats.packets_total == 1u);
    assert(stats.parser_stats.packets_failed == 1u);
    assert(stats.parser_stats.invalid_value == 1u);

    assert(stats.packet_policy_fail == 0u);
    assert(stats.rtp_header_fail == 0u);
    assert(stats.st2110_header_parse_fail == 0u);
    assert(stats.bad_srd == 1u);
    assert(stats.srd_payload_split_fail == 0u);
}

static void test_srd_payload_split_failure_is_accounted() {
    const auto bytes = make_short_srd_payload_packet();

    st2110::PacketParseStats stats{};
    const auto parsed = st2110::parse_packet_view(st2110::ByteSpan(bytes.data(), bytes.size()), stats);

    assert(!parsed.has_value());
    assert(parsed.error() == st2110::Error::ShortPacket);

    assert(stats.parser_stats.packets_total == 1u);
    assert(stats.parser_stats.packets_failed == 1u);
    assert(stats.parser_stats.short_packet == 1u);

    assert(stats.packet_policy_fail == 0u);
    assert(stats.rtp_header_fail == 0u);
    assert(stats.st2110_header_parse_fail == 0u);
    assert(stats.bad_srd == 0u);
    assert(stats.srd_payload_split_fail == 1u);
}

static void test_default_policy_oversize_is_recorded_as_packet_policy_failure() {
    std::vector<uint8_t> oversized_payload(1453, 0x00); // 1453 + 8 = 1461 > 1460
    st2110::PacketParseStats stats{};

    auto result =
        st2110::parse_packet_view(st2110::ByteSpan(oversized_payload.data(), oversized_payload.size()), stats);

    assert(!result.has_value());
    assert(result.error() == st2110::Error::InvalidValue);

    assert(stats.parser_stats.packets_total == 1);
    assert(stats.parser_stats.packets_ok == 0);
    assert(stats.parser_stats.packets_failed == 1);
    assert(stats.parser_stats.invalid_value == 1);

    assert(stats.packet_policy_fail == 1);
    assert(stats.rtp_header_fail == 0);
    assert(stats.st2110_header_parse_fail == 0);
    assert(stats.bad_srd == 0);
    assert(stats.srd_payload_split_fail == 0);
}

static void test_invalid_policy_config_is_recorded_as_packet_policy_failure() {
    st2110::PacketParsePolicy policy{};
    policy.max_udp_datagram_bytes = 1;

    const uint8_t payload[] = {0x00};
    st2110::PacketParseStats stats{};

    auto result = st2110::parse_packet_view(st2110::ByteSpan(payload, sizeof(payload)), stats, policy);

    assert(!result.has_value());
    assert(result.error() == st2110::Error::InvalidValue);

    assert(stats.parser_stats.packets_total == 1);
    assert(stats.parser_stats.packets_ok == 0);
    assert(stats.parser_stats.packets_failed == 1);
    assert(stats.parser_stats.invalid_value == 1);

    assert(stats.packet_policy_fail == 1);
    assert(stats.rtp_header_fail == 0);
    assert(stats.st2110_header_parse_fail == 0);
    assert(stats.bad_srd == 0);
    assert(stats.srd_payload_split_fail == 0);
}

int main() {
    test_success_updates_ok_counters_once();
    test_policy_failure_is_accounted_separately();
    test_rtp_header_failure_is_accounted();
    test_st2110_payload_header_parse_failure_is_accounted();
    test_st2110_payload_header_validation_failure_is_accounted();
    test_srd_payload_split_failure_is_accounted();
    test_default_policy_oversize_is_recorded_as_packet_policy_failure();
    test_invalid_policy_config_is_recorded_as_packet_policy_failure();
    return 0;
}
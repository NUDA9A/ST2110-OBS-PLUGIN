#include <cassert>
#include <cstdint>
#include <type_traits>

#include <st2110/error.hpp>
#include <st2110/stats.hpp>

static_assert(std::is_standard_layout_v<st2110::ParserStats>);
static_assert(std::is_standard_layout_v<st2110::PacketParseStats>);

static void test_default_zero_init_packet_parse_stats() {
    st2110::PacketParseStats stats{};

    assert(stats.parser_stats.packets_total == 0);
    assert(stats.parser_stats.packets_ok == 0);
    assert(stats.parser_stats.packets_failed == 0);

    assert(stats.parser_stats.short_packet == 0);
    assert(stats.parser_stats.bad_rtp_version == 0);
    assert(stats.parser_stats.invalid_value == 0);
    assert(stats.parser_stats.unsupported == 0);
    assert(stats.parser_stats.buffer_too_small == 0);
    assert(stats.parser_stats.other_error == 0);

    assert(stats.rtp_header_fail == 0);
    assert(stats.st2110_header_parse_fail == 0);
    assert(stats.bad_srd == 0);
    assert(stats.srd_payload_split_fail == 0);
}

static void test_record_packet_parse_result_ok() {
    st2110::PacketParseStats stats{};

    st2110::record_packet_parse_result(
            stats,
            st2110::Error::Ok,
            st2110::PacketParseStage::RtpHeader);

    assert(stats.parser_stats.packets_total == 1);
    assert(stats.parser_stats.packets_ok == 1);
    assert(stats.parser_stats.packets_failed == 0);

    assert(stats.parser_stats.short_packet == 0);
    assert(stats.parser_stats.bad_rtp_version == 0);
    assert(stats.parser_stats.invalid_value == 0);
    assert(stats.parser_stats.unsupported == 0);
    assert(stats.parser_stats.buffer_too_small == 0);
    assert(stats.parser_stats.other_error == 0);

    assert(stats.rtp_header_fail == 0);
    assert(stats.st2110_header_parse_fail == 0);
    assert(stats.bad_srd == 0);
    assert(stats.srd_payload_split_fail == 0);
}

static void test_record_packet_parse_result_rtp_header_error() {
    st2110::PacketParseStats stats{};

    st2110::record_packet_parse_result(
            stats,
            st2110::Error::BadRTPVersion,
            st2110::PacketParseStage::RtpHeader);

    assert(stats.parser_stats.packets_total == 1);
    assert(stats.parser_stats.packets_ok == 0);
    assert(stats.parser_stats.packets_failed == 1);

    assert(stats.parser_stats.bad_rtp_version == 1);
    assert(stats.rtp_header_fail == 1);

    assert(stats.st2110_header_parse_fail == 0);
    assert(stats.bad_srd == 0);
    assert(stats.srd_payload_split_fail == 0);
}

static void test_record_packet_parse_result_st2110_header_parse_error() {
    st2110::PacketParseStats stats{};

    st2110::record_packet_parse_result(
            stats,
            st2110::Error::ShortPacket,
            st2110::PacketParseStage::St2110PayloadHeaderParse);

    assert(stats.parser_stats.packets_total == 1);
    assert(stats.parser_stats.packets_failed == 1);

    assert(stats.parser_stats.short_packet == 1);
    assert(stats.st2110_header_parse_fail == 1);

    assert(stats.rtp_header_fail == 0);
    assert(stats.bad_srd == 0);
    assert(stats.srd_payload_split_fail == 0);
}

static void test_record_packet_parse_result_bad_srd() {
    st2110::PacketParseStats stats{};

    st2110::record_packet_parse_result(
            stats,
            st2110::Error::InvalidValue,
            st2110::PacketParseStage::St2110PayloadHeaderValidate);

    st2110::record_packet_parse_result(
            stats,
            st2110::Error::Unsupported,
            st2110::PacketParseStage::St2110PayloadHeaderValidate);

    assert(stats.parser_stats.packets_total == 2);
    assert(stats.parser_stats.packets_failed == 2);

    assert(stats.parser_stats.invalid_value == 1);
    assert(stats.parser_stats.unsupported == 1);
    assert(stats.bad_srd == 2);

    assert(stats.rtp_header_fail == 0);
    assert(stats.st2110_header_parse_fail == 0);
    assert(stats.srd_payload_split_fail == 0);
}

static void test_record_packet_parse_result_srd_payload_split_fail() {
    st2110::PacketParseStats stats{};

    st2110::record_packet_parse_result(
            stats,
            st2110::Error::ShortPacket,
            st2110::PacketParseStage::SrdPayloadSplit);

    assert(stats.parser_stats.packets_total == 1);
    assert(stats.parser_stats.packets_failed == 1);
    assert(stats.parser_stats.short_packet == 1);
    assert(stats.srd_payload_split_fail == 1);
}

static void test_record_packet_parse_result_unknown_error_goes_to_other_error() {
    st2110::PacketParseStats stats{};

    st2110::record_packet_parse_result(
            stats,
            static_cast<st2110::Error>(999),
            st2110::PacketParseStage::SrdPayloadSplit);

    assert(stats.parser_stats.packets_total == 1);
    assert(stats.parser_stats.packets_failed == 1);
    assert(stats.parser_stats.other_error == 1);
    assert(stats.srd_payload_split_fail == 1);
}

int main() {
    test_default_zero_init_packet_parse_stats();
    test_record_packet_parse_result_ok();
    test_record_packet_parse_result_rtp_header_error();
    test_record_packet_parse_result_st2110_header_parse_error();
    test_record_packet_parse_result_bad_srd();
    test_record_packet_parse_result_srd_payload_split_fail();
    test_record_packet_parse_result_unknown_error_goes_to_other_error();
    return 0;
}
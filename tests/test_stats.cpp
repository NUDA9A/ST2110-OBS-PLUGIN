#include <cassert>
#include <cstdint>
#include <type_traits>

#include <st2110/contracts/backend/backend.hpp>
#include <st2110/ingress/shared/packet_parse_stats.hpp>
#include <st2110/receive/shared/reorder_stats.hpp>
#include <st2110/receive/video/depacketizer_stats.hpp>

static_assert(std::is_standard_layout_v<st2110::ParserStats>);
static_assert(std::is_standard_layout_v<st2110::ReorderBufferStats>);
static_assert(std::is_standard_layout_v<st2110::DepacketizerStats>);
static_assert(std::is_standard_layout_v<st2110::BackendStats>);

static void test_default_zero_init_parser_stats() {
    st2110::ParserStats stats{};

    assert(stats.packets_total == 0);
    assert(stats.packets_ok == 0);
    assert(stats.packets_failed == 0);
    assert(stats.short_packet == 0);
    assert(stats.bad_rtp_version == 0);
    assert(stats.invalid_value == 0);
    assert(stats.unsupported == 0);
    assert(stats.buffer_too_small == 0);
    assert(stats.other_error == 0);
}

static void test_record_parse_result_ok_and_known_errors() {
    st2110::ParserStats stats{};

    st2110::record_parse_result(stats, st2110::Error::Ok);
    st2110::record_parse_result(stats, st2110::Error::ShortPacket);
    st2110::record_parse_result(stats, st2110::Error::BadRTPVersion);
    st2110::record_parse_result(stats, st2110::Error::InvalidValue);
    st2110::record_parse_result(stats, st2110::Error::Unsupported);
    st2110::record_parse_result(stats, st2110::Error::BufferTooSmall);

    assert(stats.packets_total == 6);
    assert(stats.packets_ok == 1);
    assert(stats.packets_failed == 5);

    assert(stats.short_packet == 1);
    assert(stats.bad_rtp_version == 1);
    assert(stats.invalid_value == 1);
    assert(stats.unsupported == 1);
    assert(stats.buffer_too_small == 1);
    assert(stats.other_error == 0);
}

static void test_record_parse_result_unknown_error_goes_to_other_error() {
    st2110::ParserStats stats{};

    st2110::record_parse_result(stats, static_cast<st2110::Error>(999));

    assert(stats.packets_total == 1);
    assert(stats.packets_ok == 0);
    assert(stats.packets_failed == 1);
    assert(stats.other_error == 1);
}

static void test_default_zero_init_reorder_stats() {
    st2110::ReorderBufferStats stats{};

    assert(stats.packets_pushed == 0);
    assert(stats.packets_stored == 0);
    assert(stats.packets_popped == 0);
    assert(stats.duplicates == 0);
    assert(stats.out_of_window == 0);
    assert(stats.late_packets == 0);
    assert(stats.missing_seq == 0);
    assert(stats.missing_seq_flushed == 0);
}

static void test_default_zero_init_depacketizer_stats() {
    st2110::DepacketizerStats stats{};

    assert(stats.packets_in == 0);
    assert(stats.packets_used == 0);
    assert(stats.units_ok == 0);
    assert(stats.units_partial == 0);
    assert(stats.units_dropped == 0);
}

static void test_default_zero_init_backend_stats() {
    st2110::BackendStats stats{};

    assert(stats.datagrams_received == 0);
    assert(stats.bytes_received == 0);
    assert(stats.datagrams_dropped == 0);
    assert(stats.media_units_delivered == 0);
}

static void test_stats_are_plain_mutable_counters() {
    st2110::ReorderBufferStats reorder{};
    reorder.packets_pushed += 10;
    reorder.packets_stored += 8;
    reorder.packets_popped += 7;
    reorder.duplicates += 1;
    reorder.missing_seq += 2;

    assert(reorder.packets_pushed == 10);
    assert(reorder.packets_stored == 8);
    assert(reorder.packets_popped == 7);
    assert(reorder.duplicates == 1);
    assert(reorder.missing_seq == 2);

    st2110::DepacketizerStats dep{};
    dep.packets_in += 10;
    dep.packets_used += 8;
    dep.units_ok += 3;
    dep.units_partial += 1;
    dep.units_dropped += 2;

    assert(dep.packets_in == 10);
    assert(dep.packets_used == 8);
    assert(dep.units_ok == 3);
    assert(dep.units_partial == 1);
    assert(dep.units_dropped == 2);

    st2110::BackendStats backend{};
    backend.datagrams_received += 100;
    backend.bytes_received += 1200;
    backend.datagrams_dropped += 4;
    backend.media_units_delivered += 7;

    assert(backend.datagrams_received == 100);
    assert(backend.bytes_received == 1200);
    assert(backend.datagrams_dropped == 4);
    assert(backend.media_units_delivered == 7);
}

int main() {
    test_default_zero_init_parser_stats();
    test_record_parse_result_ok_and_known_errors();
    test_record_parse_result_unknown_error_goes_to_other_error();
    test_default_zero_init_reorder_stats();
    test_default_zero_init_depacketizer_stats();
    test_default_zero_init_backend_stats();
    test_stats_are_plain_mutable_counters();
    return 0;
}
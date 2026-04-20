#include <cassert>
#include <cstdint>
#include <type_traits>

#include <st2110/error.hpp>
#include <st2110/stats.hpp>

static_assert(std::is_standard_layout_v<st2110::ParserStats>);
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

static void test_default_zero_init_depacketizer_stats() {
    st2110::DepacketizerStats stats{};

    assert(stats.packets_in == 0);
    assert(stats.packets_used == 0);
    assert(stats.frames_ok == 0);
    assert(stats.frames_partial == 0);
    assert(stats.frames_dropped == 0);
}

static void test_default_zero_init_backend_stats() {
    st2110::BackendStats stats{};

    assert(stats.datagrams_received == 0);
    assert(stats.bytes_received == 0);
    assert(stats.datagrams_dropped == 0);
    assert(stats.media_units_delivered == 0);
}

static void test_stats_are_plain_mutable_counters() {
    st2110::DepacketizerStats dep{};
    dep.packets_in += 10;
    dep.packets_used += 8;
    dep.frames_ok += 3;
    dep.frames_partial += 1;
    dep.frames_dropped += 2;

    assert(dep.packets_in == 10);
    assert(dep.packets_used == 8);
    assert(dep.frames_ok == 3);
    assert(dep.frames_partial == 1);
    assert(dep.frames_dropped == 2);

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
    test_default_zero_init_depacketizer_stats();
    test_default_zero_init_backend_stats();
    test_stats_are_plain_mutable_counters();
    return 0;
}
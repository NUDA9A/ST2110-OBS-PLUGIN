#include <cassert>
#include <cstdint>

#include <st2110/st2110_20.hpp>
#include <st2110/error.hpp>

static st2110::St2110PayloadHeaderView make_valid_header() {
    st2110::St2110PayloadHeaderView h{};
    h.ext_seq.hi16 = 0x1111;
    h.srd_count = 2;
    h.header_bytes = 14;

    h.srd[0].length = 16;
    h.srd[0].row_number = 3;
    h.srd[0].offset = 0;
    h.srd[0].field_id = false;
    h.srd[0].continuation = true;

    h.srd[1].length = 8;
    h.srd[1].row_number = 4;
    h.srd[1].offset = 12;
    h.srd[1].field_id = false;
    h.srd[1].continuation = false;

    return h;
}

static void test_valid_header() {
    auto h = make_valid_header();
    assert(st2110::validate_st2110_20_payload_header(h) == st2110::Error::Ok);
}

static void test_zero_srd_count() {
    auto h = make_valid_header();
    h.srd_count = 0;
    assert(st2110::validate_st2110_20_payload_header(h) == st2110::Error::InvalidValue);
}

static void test_bad_header_bytes() {
    auto h = make_valid_header();
    h.header_bytes = 8;
    assert(st2110::validate_st2110_20_payload_header(h) == st2110::Error::InvalidValue);
}

static void test_zero_length() {
    auto h = make_valid_header();
    h.srd[0].length = 0;
    assert(st2110::validate_st2110_20_payload_header(h) == st2110::Error::InvalidValue);
}

static void test_bad_continuation_chain() {
    auto h = make_valid_header();
    h.srd[0].continuation = false;
    assert(st2110::validate_st2110_20_payload_header(h) == st2110::Error::InvalidValue);
}

static void test_field_id_not_supported_in_mvp() {
    auto h = make_valid_header();
    h.srd[1].field_id = true;
    assert(st2110::validate_st2110_20_payload_header(h) == st2110::Error::Unsupported);
}

int main() {
    test_valid_header();
    test_zero_srd_count();
    test_bad_header_bytes();
    test_zero_length();
    test_bad_continuation_chain();
    test_field_id_not_supported_in_mvp();
    return 0;
}
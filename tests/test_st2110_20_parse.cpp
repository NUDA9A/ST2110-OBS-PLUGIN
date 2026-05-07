#include <cassert>
#include <cstdint>

#include <st2110/foundation/bytes.hpp>
#include <st2110/foundation/error.hpp>
#include <st2110/ingress/shared/st2110_20.hpp>

static void test_single_srd() {
    // ext_seq = 0x1234
    // length = 20
    // row_number = 10
    // offset = 4
    // field_id = false
    // continuation = false
    const uint8_t payload[] = {0x12, 0x34, 0x00, 0x14, 0x00, 0x0A, 0x00, 0x04};

    auto res = st2110::parse_st2110_20_payload_header(st2110::ByteSpan{payload});
    assert(res.has_value());

    const auto &h = res.value();
    assert(h.ext_seq.hi16 == 0x1234);
    assert(h.srd_count == 1);

    assert(h.srd[0].length == 20);
    assert(h.srd[0].row_number == 10);
    assert(h.srd[0].offset == 4);
    assert(h.srd[0].field_id == false);
    assert(h.srd[0].continuation == false);
}

static void test_two_srd() {
    // ext_seq = 0x2222
    // SRD #1: length=16, row=3, offset=0, field_id=false, continuation=true
    // SRD #2: length=8,  row=4, offset=12, field_id=true,  continuation=false
    const uint8_t payload[] = {0x22, 0x22,

                               0x00, 0x10, 0x00, 0x03, 0x80, 0x00,

                               0x00, 0x08, 0x80, 0x04, 0x00, 0x0C};

    auto res = st2110::parse_st2110_20_payload_header(st2110::ByteSpan{payload});
    assert(res.has_value());

    const auto &h = res.value();
    assert(h.ext_seq.hi16 == 0x2222);
    assert(h.srd_count == 2);

    assert(h.srd[0].length == 16);
    assert(h.srd[0].row_number == 3);
    assert(h.srd[0].offset == 0);
    assert(h.srd[0].field_id == false);
    assert(h.srd[0].continuation == true);

    assert(h.srd[1].length == 8);
    assert(h.srd[1].row_number == 4);
    assert(h.srd[1].offset == 12);
    assert(h.srd[1].field_id == true);
    assert(h.srd[1].continuation == false);
}

static void test_too_short() {
    const uint8_t payload[] = {0x12, 0x34, 0x56};
    auto res = st2110::parse_st2110_20_payload_header(st2110::ByteSpan{payload});
    assert(!res.has_value());
}

static void test_continuation_but_no_next_srd() {
    // continuation=true, but second SRD is missing
    const uint8_t payload[] = {0x11, 0x11, 0x00, 0x08, 0x00, 0x01, 0x80, 0x00};

    auto res = st2110::parse_st2110_20_payload_header(st2110::ByteSpan{payload});
    assert(!res.has_value());
}

int main() {
    test_single_srd();
    test_two_srd();
    test_too_short();
    test_continuation_but_no_next_srd();
    return 0;
}
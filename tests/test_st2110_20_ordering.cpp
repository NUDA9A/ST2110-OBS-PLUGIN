#include <cassert>
#include <cstdint>
#include <initializer_list>

#include <st2110/foundation/error.hpp>
#include <st2110/ingress/shared/st2110_20.hpp>

static st2110::SrdHeader make_srd(uint16_t length, uint16_t row, uint16_t offset, bool field_id = false) {
    st2110::SrdHeader h{};
    h.length = length;
    h.row_number = row;
    h.offset = offset;
    h.field_id = field_id;
    h.continuation = false;
    return h;
}

static st2110::St2110PayloadHeaderView make_payload_header(std::initializer_list<st2110::SrdHeader> srd_list) {
    st2110::St2110PayloadHeaderView h{};
    h.ext_seq.hi16 = 0x1234;
    h.srd_count = static_cast<uint8_t>(srd_list.size());
    h.header_bytes = 2 + 6 * static_cast<std::size_t>(h.srd_count);

    std::size_t i = 0;
    for (const auto &src : srd_list) {
        h.srd[i] = src;
        h.srd[i].continuation = (i + 1 != h.srd_count);
        ++i;
    }

    return h;
}

static void test_single_srd_is_valid() {
    const auto h = make_payload_header({make_srd(4, 10, 20)});

    assert(st2110::validate_st2110_20_payload_header(h) == st2110::Error::Ok);
}

static void test_same_row_strictly_increasing_offset_is_valid() {
    const auto h = make_payload_header({make_srd(4, 5, 0), make_srd(4, 5, 4)});

    assert(st2110::validate_st2110_20_payload_header(h) == st2110::Error::Ok);
}

static void test_next_row_may_restart_offset() {
    const auto h = make_payload_header({make_srd(4, 5, 100), make_srd(4, 6, 0)});

    assert(st2110::validate_st2110_20_payload_header(h) == st2110::Error::Ok);
}

static void test_row_number_going_backwards_is_invalid() {
    const auto h = make_payload_header({make_srd(4, 6, 0), make_srd(4, 5, 4)});

    assert(st2110::validate_st2110_20_payload_header(h) == st2110::Error::InvalidValue);
}

static void test_same_row_offset_going_backwards_is_invalid() {
    const auto h = make_payload_header({make_srd(4, 7, 8), make_srd(4, 7, 4)});

    assert(st2110::validate_st2110_20_payload_header(h) == st2110::Error::InvalidValue);
}

static void test_same_row_equal_offset_is_invalid() {
    const auto h = make_payload_header({make_srd(4, 7, 8), make_srd(4, 7, 8)});

    assert(st2110::validate_st2110_20_payload_header(h) == st2110::Error::InvalidValue);
}

static void test_three_srd_progression_is_valid() {
    const auto h = make_payload_header({make_srd(4, 0, 0), make_srd(4, 0, 4), make_srd(4, 1, 0)});

    assert(st2110::validate_st2110_20_payload_header(h) == st2110::Error::Ok);
}

static void test_field_id_does_not_affect_generic_ordering_validation() {
    const auto h = make_payload_header({make_srd(4, 0, 0, false), make_srd(4, 1, 0, true)});

    assert(st2110::validate_st2110_20_payload_header(h) == st2110::Error::Ok);
}

int main() {
    test_single_srd_is_valid();
    test_same_row_strictly_increasing_offset_is_valid();
    test_next_row_may_restart_offset();
    test_row_number_going_backwards_is_invalid();
    test_same_row_offset_going_backwards_is_invalid();
    test_same_row_equal_offset_is_invalid();
    test_three_srd_progression_is_valid();
    test_field_id_does_not_affect_generic_ordering_validation();
    return 0;
}
#include <cassert>
#include <cstdint>

#include <st2110/st2110_20.hpp>

static void test_basic_combine() {
    st2110::ExtendedSequenceNumber ext{};
    ext.hi16 = 0x1234;

    const uint32_t v = st2110::combine_extended_seq(ext, 0xABCD);
    assert(v == 0x1234ABCDu);
}

static void test_zero_low() {
    st2110::ExtendedSequenceNumber ext{};
    ext.hi16 = 0x2222;

    const uint32_t v = st2110::combine_extended_seq(ext, 0x0000);
    assert(v == 0x22220000u);
}

static void test_zero_high() {
    st2110::ExtendedSequenceNumber ext{};
    ext.hi16 = 0x0000;

    const uint32_t v = st2110::combine_extended_seq(ext, 0xBEEF);
    assert(v == 0x0000BEEFu);
}

static void test_all_ones() {
    st2110::ExtendedSequenceNumber ext{};
    ext.hi16 = 0xFFFF;

    const uint32_t v = st2110::combine_extended_seq(ext, 0xFFFF);
    assert(v == 0xFFFFFFFFu);
}

int main() {
    test_basic_combine();
    test_zero_low();
    test_zero_high();
    test_all_ones();
    return 0;
}
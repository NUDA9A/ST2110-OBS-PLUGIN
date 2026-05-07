#include <cassert>
#include <cstdint>
#include <span>

#include <st2110/foundation/endian.hpp>
#include <iostream>

static void test_be16_basic() {
    const uint8_t b[] = {0x12, 0x34};
    assert(st2110::endian::read_be16(std::span{b}) == 0x1234);
}

static void test_be16_subspan() {
    const uint8_t b[] = {0x00, 0xAB, 0xCD, 0x00};
    auto s = std::span{b};
    assert(st2110::endian::read_be16(s.subspan(1, 2)) == 0xABCD);
}

static void test_be32_basic() {
    const uint8_t b[] = {0xDE, 0xAD, 0xBE, 0xEF};
    assert(st2110::endian::read_be32(std::span{b}) == 0xDEADBEEF);
}

static void test_be32_subspan() {
    const uint8_t b[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x00};
    auto s = std::span{b};
    assert(st2110::endian::read_be32(s.subspan(1, 4)) == 0x01020304);
}

int main() {
    test_be16_basic();
    test_be16_subspan();
    test_be32_basic();
    test_be32_subspan();
    return 0;
}
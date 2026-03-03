#include <cassert>
#include <cstring>

#include <st2110/error.hpp>

static void test_non_empty_strings() {
    assert(std::strlen(st2110::to_string(st2110::Error::Ok)) > 0);
    assert(std::strlen(st2110::to_string(st2110::Error::BufferTooSmall)) > 0);
    assert(std::strlen(st2110::to_string(st2110::Error::InvalidValue)) > 0);
    assert(std::strlen(st2110::to_string(st2110::Error::Unsupported)) > 0);
}

static void test_distinct_strings() {
    const char* a = st2110::to_string(st2110::Error::Ok);
    const char* b = st2110::to_string(st2110::Error::BufferTooSmall);
    const char* c = st2110::to_string(st2110::Error::InvalidValue);
    const char* d = st2110::to_string(st2110::Error::Unsupported);

    // at least ensure not all the same text
    assert(std::strcmp(a, b) != 0);
    assert(std::strcmp(b, c) != 0);
    assert(std::strcmp(c, d) != 0);
}

int main() {
    test_non_empty_strings();
    test_distinct_strings();
    return 0;
}
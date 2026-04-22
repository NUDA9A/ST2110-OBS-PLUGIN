#include <cassert>
#include <cstdint>
#include <type_traits>

#include <st2110/timestamp.hpp>

static_assert(std::is_same_v<st2110::TimestampNs, std::uint64_t>);

static void test_timestamp_ns_is_uint64_alias() {
    st2110::TimestampNs ts = 123456789ull;
    assert(ts == 123456789ull);
}

int main() {
    test_timestamp_ns_is_uint64_alias();
    return 0;
}
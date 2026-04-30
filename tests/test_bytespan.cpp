#include <cassert>
#include <cstdint>

#include <st2110/bytes.hpp>

int main() {
    uint8_t buf[4] = {1, 2, 3, 4};
    st2110::ByteSpan s{buf};
    assert(s.size() == 4);
    return 0;
}
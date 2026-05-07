#include <cassert>

#include <st2110/foundation/error.hpp>

st2110::Error odr_helper_a();
st2110::Error odr_helper_b();

int main() {
    assert(odr_helper_a() == st2110::Error::Ok);
    assert(odr_helper_b() == st2110::Error::Ok);
    return 0;
}
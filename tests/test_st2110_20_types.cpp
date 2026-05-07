#include <cassert>
#include <cstdint>

#include <st2110/ingress/shared/st2110_20.hpp>

int main() {
    st2110::ExtendedSequenceNumber ext{};
    ext.hi16 = 0x1234;
    assert(ext.hi16 == 0x1234);

    st2110::SrdHeader srd{};
    srd.length = 120;
    srd.row_number = 42;
    srd.offset = 16;
    srd.field_id = false;
    srd.continuation = true;

    assert(srd.length == 120);
    assert(srd.row_number == 42);
    assert(srd.offset == 16);
    assert(srd.field_id == false);
    assert(srd.continuation == true);

    st2110::St2110PayloadHeaderView view{};
    view.ext_seq = ext;
    view.srd_count = 1;
    view.srd[0] = srd;

    assert(view.ext_seq.hi16 == 0x1234);
    assert(view.srd_count == 1);
    assert(view.srd[0].row_number == 42);

    return 0;
}
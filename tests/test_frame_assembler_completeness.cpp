#include <cassert>
#include <cstdint>

#include <st2110/frame_assembler.hpp>

static void test_marker_seen_full_coverage_is_complete() {
    st2110::FrameAssembler assembler(4, 2, st2110::PixelFormat::UYVY);
    // active row bytes = 8

    const uint8_t row0[] = {0,1,2,3,4,5,6,7};
    const uint8_t row1[] = {10,11,12,13,14,15,16,17};

    assembler.begin(100);
    assembler.write_segment(0, 0, 0, st2110::ByteSpan(row0, sizeof(row0)));
    assembler.write_segment(0, 1, 0, st2110::ByteSpan(row1, sizeof(row1)));

    st2110::FrameAssemblerEndResult result = assembler.end(true);
    assert(result.status == st2110::FrameAssemblerEndStatus::EmittedComplete);
    assert(result.frame.has_value());

    const st2110::AssembledVideoFrame& out = *result.frame;
    assert(out.marker_seen == true);
    assert(out.can_emit == true);
    assert(out.complete == true);
    assert(out.partial() == false);
    assert(out.rtp_timestamp == 100u);
}

static void test_marker_seen_hole_inside_row_is_partial() {
    st2110::FrameAssembler assembler(4, 1, st2110::PixelFormat::UYVY);
    // active row bytes = 8

    const uint8_t left[]  = {0,1};
    const uint8_t right[] = {4,5,6,7};

    assembler.begin(101);
    assembler.write_segment(0, 0, 0, st2110::ByteSpan(left, sizeof(left)));   // [0,2)
    assembler.write_segment(0, 0, 4, st2110::ByteSpan(right, sizeof(right))); // [4,8), hole [2,4)

    st2110::FrameAssemblerEndResult result = assembler.end(true);
    assert(result.status == st2110::FrameAssemblerEndStatus::EmittedPartial);
    assert(result.frame.has_value());

    const st2110::AssembledVideoFrame& out = *result.frame;
    assert(out.marker_seen == true);
    assert(out.can_emit == true);
    assert(out.complete == false);
    assert(out.partial() == true);
    assert(out.rtp_timestamp == 101u);
}

static void test_marker_seen_overlap_still_can_be_complete() {
    st2110::FrameAssembler assembler(4, 1, st2110::PixelFormat::UYVY);
    // active row bytes = 8

    const uint8_t first[]  = {0,1,2,3,4,5}; // [0,6)
    const uint8_t second[] = {4,5,6,7};     // [4,8), overlap on [4,6)

    assembler.begin(102);
    assembler.write_segment(0, 0, 0, st2110::ByteSpan(first, sizeof(first)));
    assembler.write_segment(0, 0, 4, st2110::ByteSpan(second, sizeof(second)));

    st2110::FrameAssemblerEndResult result = assembler.end(true);
    assert(result.status == st2110::FrameAssemblerEndStatus::EmittedComplete);
    assert(result.frame.has_value());

    const st2110::AssembledVideoFrame& out = *result.frame;
    assert(out.marker_seen == true);
    assert(out.can_emit == true);
    assert(out.complete == true);
    assert(out.partial() == false);
    assert(out.rtp_timestamp == 102u);
}

static void test_marker_false_not_emittable_even_if_full() {
    st2110::FrameAssembler assembler(4, 1, st2110::PixelFormat::UYVY);

    const uint8_t full[] = {0,1,2,3,4,5,6,7};

    assembler.begin(103);
    assembler.write_segment(0, 0, 0, st2110::ByteSpan(full, sizeof(full)));

    st2110::FrameAssemblerEndResult result = assembler.end(false);
    assert(result.status == st2110::FrameAssemblerEndStatus::NotEmittable);
    assert(!result.frame.has_value());
}

static void test_new_begin_resets_exact_coverage_tracking() {
    st2110::FrameAssembler assembler(4, 1, st2110::PixelFormat::UYVY);

    const uint8_t partial[] = {0,1,2,3};
    const uint8_t full[]    = {0,1,2,3,4,5,6,7};

    assembler.begin(1);
    assembler.write_segment(0, 0, 0, st2110::ByteSpan(partial, sizeof(partial)));
    st2110::FrameAssemblerEndResult first_result = assembler.end(true);
    assert(first_result.status == st2110::FrameAssemblerEndStatus::EmittedPartial);
    assert(first_result.frame.has_value());

    const st2110::AssembledVideoFrame& first = *first_result.frame;
    assert(first.can_emit == true);
    assert(first.complete == false);
    assert(first.partial() == true);

    assembler.begin(2);
    assembler.write_segment(0, 0, 0, st2110::ByteSpan(full, sizeof(full)));
    st2110::FrameAssemblerEndResult second_result = assembler.end(true);
    assert(second_result.status == st2110::FrameAssemblerEndStatus::EmittedComplete);
    assert(second_result.frame.has_value());

    const st2110::AssembledVideoFrame& second = *second_result.frame;
    assert(second.can_emit == true);
    assert(second.complete == true);
    assert(second.partial() == false);
    assert(second.rtp_timestamp == 2u);
}

int main() {
    test_marker_seen_full_coverage_is_complete();
    test_marker_seen_hole_inside_row_is_partial();
    test_marker_seen_overlap_still_can_be_complete();
    test_marker_false_not_emittable_even_if_full();
    test_new_begin_resets_exact_coverage_tracking();
    return 0;
}
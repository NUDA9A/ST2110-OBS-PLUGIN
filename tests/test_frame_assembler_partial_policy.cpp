#include <cassert>
#include <cstdint>

#include <st2110/frame_assembler.hpp>

static void test_emit_with_flag_policy_emits_partial_frame() {
    st2110::FrameAssembler assembler(
            4, 1, st2110::PixelFormat::UYVY,
            st2110::PartialFramePolicy::EmitWithFlag);

    const uint8_t partial[] = {0, 1, 2, 3}; // only half of active row (8 bytes)

    assembler.begin(100);
    assembler.write_segment(0, 0, 0, st2110::ByteSpan(partial, sizeof(partial)));

    st2110::FrameAssemblerEndResult result = assembler.end(true);

    assert(result.status == st2110::FrameAssemblerEndStatus::EmittedPartial);
    assert(result.frame.has_value());

    const st2110::AssembledVideoFrame& out = *result.frame;
    assert(out.marker_seen == true);
    assert(out.can_emit == true);
    assert(out.complete == false);
    assert(out.partial() == true);
    assert(out.rtp_timestamp == 100u);
}

static void test_drop_policy_drops_partial_frame() {
    st2110::FrameAssembler assembler(
            4, 1, st2110::PixelFormat::UYVY,
            st2110::PartialFramePolicy::Drop);

    const uint8_t partial[] = {0, 1, 2, 3};

    assembler.begin(101);
    assembler.write_segment(0, 0, 0, st2110::ByteSpan(partial, sizeof(partial)));

    st2110::FrameAssemblerEndResult result = assembler.end(true);

    assert(result.status == st2110::FrameAssemblerEndStatus::DroppedPartial);
    assert(!result.frame.has_value());
    assert(!assembler.in_progress());
}

static void test_drop_policy_still_emits_complete_frame() {
    st2110::FrameAssembler assembler(
            4, 1, st2110::PixelFormat::UYVY,
            st2110::PartialFramePolicy::Drop);

    const uint8_t full[] = {0,1,2,3,4,5,6,7};

    assembler.begin(102);
    assembler.write_segment(0, 0, 0, st2110::ByteSpan(full, sizeof(full)));

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

static void test_marker_false_is_not_emittable_in_any_policy() {
    st2110::FrameAssembler assembler(
            4, 1, st2110::PixelFormat::UYVY,
            st2110::PartialFramePolicy::EmitWithFlag);

    const uint8_t full[] = {0,1,2,3,4,5,6,7};

    assembler.begin(103);
    assembler.write_segment(0, 0, 0, st2110::ByteSpan(full, sizeof(full)));

    st2110::FrameAssemblerEndResult result = assembler.end(false);

    assert(result.status == st2110::FrameAssemblerEndStatus::NotEmittable);
    assert(!result.frame.has_value());
    assert(!assembler.in_progress());
}

static void test_lifecycle_recovers_after_dropped_partial_frame() {
    st2110::FrameAssembler assembler(
            4, 1, st2110::PixelFormat::UYVY,
            st2110::PartialFramePolicy::Drop);

    const uint8_t partial[] = {0,1,2,3};
    const uint8_t full[] = {0,1,2,3,4,5,6,7};

    assembler.begin(200);
    assembler.write_segment(0, 0, 0, st2110::ByteSpan(partial, sizeof(partial)));
    st2110::FrameAssemblerEndResult first = assembler.end(true);

    assert(first.status == st2110::FrameAssemblerEndStatus::DroppedPartial);
    assert(!first.frame.has_value());

    assembler.begin(201);
    assembler.write_segment(0, 0, 0, st2110::ByteSpan(full, sizeof(full)));
    st2110::FrameAssemblerEndResult second = assembler.end(true);

    assert(second.status == st2110::FrameAssemblerEndStatus::EmittedComplete);
    assert(second.frame.has_value());
    assert(second.frame->rtp_timestamp == 201u);
    assert(second.frame->complete == true);
}
int main() {
    test_emit_with_flag_policy_emits_partial_frame();
    test_drop_policy_drops_partial_frame();
    test_drop_policy_still_emits_complete_frame();
    test_marker_false_is_not_emittable_in_any_policy();
    test_lifecycle_recovers_after_dropped_partial_frame();
    return 0;
}
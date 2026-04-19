#include <cassert>
#include <cstdint>
#include <stdexcept>

#include <st2110/frame_assembler.hpp>

static void test_write_segment_allows_write_ending_exactly_at_row_boundary() {
    st2110::FrameAssembler assembler(4, 1, st2110::PixelFormat::UYVY);
    // UYVY active row bytes = 8 for width=4
    const uint8_t seg[] = {0x10, 0x11, 0x12, 0x13};

    assembler.begin(1);
    assembler.write_segment(0, 0, 4, st2110::ByteSpan(seg, sizeof(seg)));

    st2110::FrameAssemblerEndResult result = assembler.end(true);
    assert(result.status == st2110::FrameAssemblerEndStatus::EmittedPartial);
    assert(result.frame.has_value());

    const st2110::AssembledVideoFrame& out = *result.frame;
    const uint8_t* row0 = out.frame.row_data(0, 0);
    assert(row0[4] == 0x10);
    assert(row0[5] == 0x11);
    assert(row0[6] == 0x12);
    assert(row0[7] == 0x13);
}

static void test_write_segment_rejects_offset_past_stride() {
    st2110::FrameAssembler assembler(4, 1, st2110::PixelFormat::UYVY);
    const uint8_t seg[] = {0xAA};

    assembler.begin(1);

    bool thrown = false;
    try {
        assembler.write_segment(0, 0, 9, st2110::ByteSpan(seg, sizeof(seg))); // active row bytes is 8
    } catch (const std::out_of_range&) {
        thrown = true;
    }
    assert(thrown);
}

static void test_write_segment_rejects_non_empty_write_at_exact_end_of_row() {
    st2110::FrameAssembler assembler(4, 1, st2110::PixelFormat::UYVY);
    const uint8_t seg[] = {0xAA};

    assembler.begin(1);

    bool thrown = false;
    try {
        assembler.write_segment(0, 0, 8, st2110::ByteSpan(seg, sizeof(seg))); // active row bytes is 8
    } catch (const std::out_of_range&) {
        thrown = true;
    }
    assert(thrown);
}

static void test_write_segment_allows_zero_length_write_at_exact_end_of_row() {
    st2110::FrameAssembler assembler(4, 1, st2110::PixelFormat::UYVY);

    assembler.begin(1);
    assembler.write_segment(0, 0, 8, st2110::ByteSpan{}); // legal no-op

    st2110::FrameAssemblerEndResult result = assembler.end(true);
    assert(result.status == st2110::FrameAssemblerEndStatus::EmittedPartial);
    assert(result.frame.has_value());

    const st2110::AssembledVideoFrame& out = *result.frame;
    assert(out.frame.width() == 4);
    assert(out.frame.height() == 1);
}

static void test_write_segment_rejects_write_crossing_row_boundary() {
    st2110::FrameAssembler assembler(4, 1, st2110::PixelFormat::UYVY);
    const uint8_t seg[] = {0x01, 0x02, 0x03};

    assembler.begin(1);

    bool thrown = false;
    try {
        assembler.write_segment(0, 0, 6, st2110::ByteSpan(seg, sizeof(seg))); // active row bytes 8, needs 9
    } catch (const std::out_of_range&) {
        thrown = true;
    }
    assert(thrown);
}

static void test_write_segment_row_out_of_range_still_rejected() {
    st2110::FrameAssembler assembler(4, 1, st2110::PixelFormat::UYVY);
    const uint8_t seg[] = {0x10};

    assembler.begin(1);

    bool thrown = false;
    try {
        assembler.write_segment(0, 1, 0, st2110::ByteSpan(seg, sizeof(seg)));
    } catch (const std::out_of_range&) {
        thrown = true;
    }
    assert(thrown);
}

static void test_write_segment_plane_out_of_range_rejected() {
    st2110::FrameAssembler assembler(4, 1, st2110::PixelFormat::UYVY);
    const uint8_t seg[] = {0x10};

    assembler.begin(1);

    bool thrown = false;
    try {
        assembler.write_segment(1, 0, 0, st2110::ByteSpan(seg, sizeof(seg)));
    } catch (const std::out_of_range&) {
        thrown = true;
    }
    assert(thrown);
}

int main() {
    test_write_segment_allows_write_ending_exactly_at_row_boundary();
    test_write_segment_rejects_offset_past_stride();
    test_write_segment_rejects_non_empty_write_at_exact_end_of_row();
    test_write_segment_allows_zero_length_write_at_exact_end_of_row();
    test_write_segment_rejects_write_crossing_row_boundary();
    test_write_segment_row_out_of_range_still_rejected();
    test_write_segment_plane_out_of_range_rejected();
    return 0;
}
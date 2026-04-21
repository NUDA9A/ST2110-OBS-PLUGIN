#include <cassert>
#include <cstdint>

#include <st2110/video_segment_placement.hpp>
#include <st2110/video_scan_mode.hpp>
#include <st2110/pixel_format.hpp>
#include <st2110/st2110_20.hpp>
#include <st2110/bytes.hpp>

static st2110::SrdSegmentView make_segment(uint16_t row, uint16_t offset, const uint8_t* data, std::size_t size) {
    st2110::SrdSegmentView seg{};
    seg.header.length = static_cast<uint16_t>(size);
    seg.header.row_number = row;
    seg.header.offset = offset;
    seg.header.field_id = false;
    seg.header.continuation = false;
    seg.data = st2110::ByteSpan(data, size);
    return seg;
}

static void test_progressive_uyvy_offset_zero_maps_to_byte_zero() {
    static const uint8_t bytes[] = {0x10, 0x11, 0x12, 0x13};
    auto seg = make_segment(3, 0, bytes, sizeof(bytes));

    auto op = st2110::map_video_segment_to_frame_write(
            st2110::PixelFormat::UYVY,
            st2110::VideoScanMode::Progressive,
            seg);

    assert(op.has_value());
    assert(op->plane == 0u);
    assert(op->row == 3u);
    assert(op->byte_offset == 0u);
    assert(op->bytes.size() == sizeof(bytes));
    assert(op->bytes.data() == bytes);
}

static void test_progressive_uyvy_offset_two_maps_to_byte_four() {
    static const uint8_t bytes[] = {0x20, 0x21, 0x22, 0x23};
    auto seg = make_segment(1, 2, bytes, sizeof(bytes));

    auto op = st2110::map_video_segment_to_frame_write(
            st2110::PixelFormat::UYVY,
            st2110::VideoScanMode::Progressive,
            seg);

    assert(op.has_value());
    assert(op->plane == 0u);
    assert(op->row == 1u);
    assert(op->byte_offset == 4u);
    assert(op->bytes.size() == sizeof(bytes));
    assert(op->bytes.data() == bytes);
}

static void test_progressive_uyvy_offset_six_maps_to_byte_twelve() {
    static const uint8_t bytes[] = {0x30, 0x31, 0x32, 0x33};
    auto seg = make_segment(0, 6, bytes, sizeof(bytes));

    auto op = st2110::map_video_segment_to_frame_write(
            st2110::PixelFormat::UYVY,
            st2110::VideoScanMode::Progressive,
            seg);

    assert(op.has_value());
    assert(op->byte_offset == 12u);
}

static void test_interlaced_mapping_is_unsupported_for_now() {
    static const uint8_t bytes[] = {0x40, 0x41, 0x42, 0x43};
    auto seg = make_segment(0, 0, bytes, sizeof(bytes));

    auto op = st2110::map_video_segment_to_frame_write(
            st2110::PixelFormat::UYVY,
            st2110::VideoScanMode::Interlaced,
            seg);

    assert(!op.has_value());
    assert(op.error() == st2110::Error::Unsupported);
}

static void test_psf_mapping_is_unsupported_for_now() {
    static const uint8_t bytes[] = {0x50, 0x51, 0x52, 0x53};
    auto seg = make_segment(0, 0, bytes, sizeof(bytes));

    auto op = st2110::map_video_segment_to_frame_write(
            st2110::PixelFormat::UYVY,
            st2110::VideoScanMode::PsF,
            seg);

    assert(!op.has_value());
    assert(op.error() == st2110::Error::Unsupported);
}

int main() {
    test_progressive_uyvy_offset_zero_maps_to_byte_zero();
    test_progressive_uyvy_offset_two_maps_to_byte_four();
    test_progressive_uyvy_offset_six_maps_to_byte_twelve();
    test_interlaced_mapping_is_unsupported_for_now();
    test_psf_mapping_is_unsupported_for_now();
    return 0;
}
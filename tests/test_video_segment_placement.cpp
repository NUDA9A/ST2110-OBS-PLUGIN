#include <cassert>
#include <cstdint>
#include <stdexcept>

#include <st2110/video_segment_placement.hpp>
#include <st2110/video_scan_mode.hpp>
#include <st2110/pixel_format.hpp>

static st2110::SrdSegmentView make_segment(
        uint16_t row,
        uint16_t offset,
        uint16_t length,
        const uint8_t* data,
        std::size_t size) {
    st2110::SrdSegmentView seg{};
    seg.header.length = length;
    seg.header.row_number = row;
    seg.header.offset = offset;
    seg.header.field_id = false;
    seg.header.continuation = false;
    seg.data = st2110::ByteSpan(data, size);
    return seg;
}

static void test_progressive_uyvy_aligned_segment_maps_successfully() {
    static const uint8_t bytes[] = {0x10, 0x11, 0x12, 0x13};

    auto seg = make_segment(2, 2, 4, bytes, sizeof(bytes));
    auto op = st2110::map_video_segment_to_frame_write(
            st2110::PixelFormat::UYVY,
            st2110::VideoScanMode::Progressive,
            seg);

    assert(op.has_value());
    assert(op->plane == 0u);
    assert(op->row == 2u);
    assert(op->byte_offset == 4u);
    assert(op->bytes.size() == sizeof(bytes));
    assert(op->bytes.data() == bytes);
}

static void test_progressive_uyvy_rejects_odd_sample_offset() {
    static const uint8_t bytes[] = {0x20, 0x21, 0x22, 0x23};

    auto seg = make_segment(0, 1, 4, bytes, sizeof(bytes));
    auto op = st2110::map_video_segment_to_frame_write(
            st2110::PixelFormat::UYVY,
            st2110::VideoScanMode::Progressive,
            seg);

    assert(!op.has_value());
    assert(op.error() == st2110::Error::InvalidValue);
}

static void test_progressive_uyvy_rejects_length_not_multiple_of_pgroup() {
    static const uint8_t bytes[] = {0x30, 0x31};

    auto seg = make_segment(0, 0, 2, bytes, sizeof(bytes));
    auto op = st2110::map_video_segment_to_frame_write(
            st2110::PixelFormat::UYVY,
            st2110::VideoScanMode::Progressive,
            seg);

    assert(!op.has_value());
    assert(op.error() == st2110::Error::InvalidValue);
}

static void test_progressive_uyvy_rejects_header_length_payload_size_mismatch() {
    static const uint8_t bytes[] = {0x40, 0x41, 0x42, 0x43};

    auto seg = make_segment(0, 0, 8, bytes, sizeof(bytes));
    auto op = st2110::map_video_segment_to_frame_write(
            st2110::PixelFormat::UYVY,
            st2110::VideoScanMode::Progressive,
            seg);

    assert(!op.has_value());
    assert(op.error() == st2110::Error::InvalidValue);
}

static void test_interlaced_mapping_is_still_unsupported() {
    static const uint8_t bytes[] = {0x50, 0x51, 0x52, 0x53};

    auto seg = make_segment(0, 0, 4, bytes, sizeof(bytes));
    auto op = st2110::map_video_segment_to_frame_write(
            st2110::PixelFormat::UYVY,
            st2110::VideoScanMode::Interlaced,
            seg);

    assert(!op.has_value());
    assert(op.error() == st2110::Error::Unsupported);
}

static void test_psf_mapping_is_still_unsupported() {
    static const uint8_t bytes[] = {0x60, 0x61, 0x62, 0x63};

    auto seg = make_segment(0, 0, 4, bytes, sizeof(bytes));
    auto op = st2110::map_video_segment_to_frame_write(
            st2110::PixelFormat::UYVY,
            st2110::VideoScanMode::PsF,
            seg);

    assert(!op.has_value());
    assert(op.error() == st2110::Error::Unsupported);
}

int main() {
    test_progressive_uyvy_aligned_segment_maps_successfully();
    test_progressive_uyvy_rejects_odd_sample_offset();
    test_progressive_uyvy_rejects_length_not_multiple_of_pgroup();
    test_progressive_uyvy_rejects_header_length_payload_size_mismatch();
    test_interlaced_mapping_is_still_unsupported();
    test_psf_mapping_is_still_unsupported();
    return 0;
}
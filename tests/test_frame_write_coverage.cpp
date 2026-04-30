#include <cassert>
#include <cstdint>
#include <stdexcept>

#include <st2110/frame_write_coverage.hpp>

static void test_reset_from_video_frame_initializes_expected_sizes() {
    st2110::VideoFrame frame(4, 2, st2110::PixelFormat::UYVY);
    st2110::FrameWriteCoverage cov(frame);

    assert(cov.plane_count() == 1);
    assert(cov.plane_expected_bytes(0) == 8u * 2u);
    assert(cov.plane_written_unique_bytes(0) == 0);
    assert(cov.total_expected_bytes() == 16);
    assert(cov.total_written_unique_bytes() == 0);
    assert(!cov.is_complete());
}

static void test_mark_written_tracks_exact_bytes() {
    st2110::VideoFrame frame(4, 2, st2110::PixelFormat::UYVY);
    st2110::FrameWriteCoverage cov(frame);

    cov.mark_written(0, 0, 0, 4); // first half of row 0
    assert(cov.total_written_unique_bytes() == 4);
    assert(!cov.is_complete());

    cov.mark_written(0, 0, 4, 4); // second half of row 0
    assert(cov.total_written_unique_bytes() == 8);
    assert(!cov.is_complete());

    cov.mark_written(0, 1, 0, 8); // full row 1
    assert(cov.total_written_unique_bytes() == 16);
    assert(cov.is_complete());
}

static void test_hole_inside_row_keeps_frame_incomplete() {
    st2110::VideoFrame frame(4, 1, st2110::PixelFormat::UYVY);
    st2110::FrameWriteCoverage cov(frame);

    cov.mark_written(0, 0, 0, 2); // [0,2)
    cov.mark_written(0, 0, 4, 4); // [4,8), hole [2,4)

    assert(cov.total_expected_bytes() == 8);
    assert(cov.total_written_unique_bytes() == 6);
    assert(!cov.is_complete());
}

static void test_overlap_counts_only_unique_bytes() {
    st2110::VideoFrame frame(4, 1, st2110::PixelFormat::UYVY);
    st2110::FrameWriteCoverage cov(frame);

    cov.mark_written(0, 0, 0, 6); // [0,6)
    assert(cov.total_written_unique_bytes() == 6);

    cov.mark_written(0, 0, 4, 4); // [4,8), overlap on [4,6)
    assert(cov.total_written_unique_bytes() == 8);
    assert(cov.is_complete());
}

static void test_zero_length_mark_is_noop() {
    st2110::VideoFrame frame(4, 1, st2110::PixelFormat::UYVY);
    st2110::FrameWriteCoverage cov(frame);

    cov.mark_written(0, 0, 8, 0);

    assert(cov.total_written_unique_bytes() == 0);
    assert(!cov.is_complete());
}

static void test_invalid_plane_rejected() {
    st2110::VideoFrame frame(4, 1, st2110::PixelFormat::UYVY);
    st2110::FrameWriteCoverage cov(frame);

    bool thrown = false;
    try {
        cov.mark_written(1, 0, 0, 1);
    } catch (const std::out_of_range &) {
        thrown = true;
    }
    assert(thrown);
}

static void test_invalid_row_rejected() {
    st2110::VideoFrame frame(4, 1, st2110::PixelFormat::UYVY);
    st2110::FrameWriteCoverage cov(frame);

    bool thrown = false;
    try {
        cov.mark_written(0, 1, 0, 1);
    } catch (const std::out_of_range &) {
        thrown = true;
    }
    assert(thrown);
}

static void test_write_crossing_active_row_boundary_rejected() {
    st2110::VideoFrame frame(4, 1, st2110::PixelFormat::UYVY);
    st2110::FrameWriteCoverage cov(frame);

    bool thrown1 = false;
    try {
        cov.mark_written(0, 0, 9, 1);
    } catch (const std::out_of_range &) {
        thrown1 = true;
    }
    assert(thrown1);

    bool thrown2 = false;
    try {
        cov.mark_written(0, 0, 6, 3); // active row bytes = 8
    } catch (const std::out_of_range &) {
        thrown2 = true;
    }
    assert(thrown2);
}

static void test_reset_from_clears_previous_coverage() {
    st2110::VideoFrame frame(4, 1, st2110::PixelFormat::UYVY);
    st2110::FrameWriteCoverage cov(frame);

    cov.mark_written(0, 0, 0, 8);
    assert(cov.is_complete());

    cov.reset_from(frame);

    assert(cov.total_expected_bytes() == 8);
    assert(cov.total_written_unique_bytes() == 0);
    assert(!cov.is_complete());
}

int main() {
    test_reset_from_video_frame_initializes_expected_sizes();
    test_mark_written_tracks_exact_bytes();
    test_hole_inside_row_keeps_frame_incomplete();
    test_overlap_counts_only_unique_bytes();
    test_zero_length_mark_is_noop();
    test_invalid_plane_rejected();
    test_invalid_row_rejected();
    test_write_crossing_active_row_boundary_rejected();
    test_reset_from_clears_previous_coverage();
    return 0;
}
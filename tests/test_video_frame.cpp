#include <cassert>
#include <cstdint>

#include <st2110/delivery/video/pixel_format.hpp>
#include <st2110/delivery/video/video_frame.hpp>

int main() {
    const uint32_t w = 1280;
    const uint32_t h = 720;

    st2110::VideoFrame f(w, h, st2110::PixelFormat::UYVY);

    // Size/stride expectations for UYVY: 2 bytes per pixel, single plane
    assert(f.size_bytes() == static_cast<size_t>(w) * h * 2);

    auto v = f.view(123456789);
    assert(v.format == st2110::PixelFormat::UYVY);
    assert(v.width == w);
    assert(v.height == h);
    assert(v.timestamp_ns == 123456789);

    assert(v.data[0] != nullptr);
    assert(v.stride[0] == w * 2);

    // Write through the returned pointer and ensure memory is stable
    auto p = const_cast<uint8_t *>(v.data[0]);
    p[0] = 0xAA;
    p[1] = 0x55;

    auto v2 = f.view();
    assert(v2.data[0][0] == 0xAA);
    assert(v2.data[0][1] == 0x55);

    return 0;
}
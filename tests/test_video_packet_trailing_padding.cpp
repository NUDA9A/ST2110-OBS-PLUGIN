#include <cassert>
#include <cstdint>
#include <vector>

#include <st2110/packet_view.hpp>
#include <st2110/video_packet_padding.hpp>

static std::vector<uint8_t> make_packet(bool marker,
                                        const std::vector<uint8_t>& segment_bytes,
                                        const std::vector<uint8_t>& trailing_bytes) {
    std::vector<uint8_t> p;

    p.push_back(0x80);
    p.push_back(static_cast<uint8_t>((marker ? 0x80 : 0x00) | 96));
    p.push_back(0x00); p.push_back(0x01);
    p.push_back(0x00); p.push_back(0x00); p.push_back(0x00); p.push_back(0x2A);
    p.push_back(0x00); p.push_back(0x00); p.push_back(0x00); p.push_back(0x01);

    p.push_back(0x00); p.push_back(0x00);
    p.push_back(0x00); p.push_back(0x04);
    p.push_back(0x00); p.push_back(0x00);
    p.push_back(0x00); p.push_back(0x00);

    p.insert(p.end(), segment_bytes.begin(), segment_bytes.end());
    p.insert(p.end(), trailing_bytes.begin(), trailing_bytes.end());
    return p;
}

static st2110::PacketView parse_packet(bool marker,
                                       const std::vector<uint8_t>& segment_bytes,
                                       const std::vector<uint8_t>& trailing_bytes) {
    const std::vector<uint8_t> bytes = make_packet(marker, segment_bytes, trailing_bytes);
    auto parsed = st2110::PacketView::from_udp_datagram(
            st2110::ByteSpan(bytes.data(), bytes.size()));
    assert(parsed.has_value());
    return *parsed;
}

static void test_progressive_packet_without_trailing_padding_is_ok() {
    const st2110::PacketView pkt = parse_packet(true, {1,2,3,4}, {});
    assert(st2110::validate_video_packet_trailing_padding(
            st2110::VideoScanMode::Progressive, pkt) == st2110::Error::Ok);
}

static void test_progressive_marker_packet_with_zero_trailing_padding_is_ok() {
    const st2110::PacketView pkt = parse_packet(true, {1,2,3,4}, {0,0,0});
    assert(st2110::validate_video_packet_trailing_padding(
            st2110::VideoScanMode::Progressive, pkt) == st2110::Error::Ok);
}

static void test_progressive_non_marker_packet_with_trailing_padding_is_invalid() {
    const st2110::PacketView pkt = parse_packet(false, {1,2,3,4}, {0,0});
    assert(st2110::validate_video_packet_trailing_padding(
            st2110::VideoScanMode::Progressive, pkt) == st2110::Error::InvalidValue);
}

static void test_progressive_marker_packet_with_non_zero_trailing_padding_is_invalid() {
    const st2110::PacketView pkt = parse_packet(true, {1,2,3,4}, {0,7});
    assert(st2110::validate_video_packet_trailing_padding(
            st2110::VideoScanMode::Progressive, pkt) == st2110::Error::InvalidValue);
}

int main() {
    test_progressive_packet_without_trailing_padding_is_ok();
    test_progressive_marker_packet_with_zero_trailing_padding_is_ok();
    test_progressive_non_marker_packet_with_trailing_padding_is_invalid();
    test_progressive_marker_packet_with_non_zero_trailing_padding_is_invalid();
    return 0;
}
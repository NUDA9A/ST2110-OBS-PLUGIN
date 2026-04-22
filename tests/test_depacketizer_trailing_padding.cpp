#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <st2110/depacketizer.hpp>
#include <st2110/packet_view.hpp>

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

static st2110::Depacketizer make_depacketizer() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 2;   // UYVY => 4 bytes per row
    cfg.height = 1;
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.partial_frame_policy = st2110::PartialFramePolicy::EmitWithFlag;
    cfg.scan_mode = st2110::VideoScanMode::Progressive;
    return st2110::Depacketizer(cfg);
}

static void test_progressive_marker_packet_with_zero_trailing_padding_is_accepted() {
    st2110::Depacketizer dep = make_depacketizer();
    const st2110::PacketView pkt = parse_packet(true, {1,2,3,4}, {0,0});

    auto out = dep.push(pkt);

    assert(out.size() == 1);
    assert(out[0].complete);
    assert(out[0].marker_seen);
    assert(out[0].rtp_timestamp == 42u);
}

static void test_progressive_non_marker_packet_with_trailing_padding_is_rejected() {
    st2110::Depacketizer dep = make_depacketizer();
    const st2110::PacketView pkt = parse_packet(false, {1,2,3,4}, {0,0});

    bool thrown = false;
    try {
        (void)dep.push(pkt);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }

    assert(thrown);
}

static void test_progressive_marker_packet_with_non_zero_trailing_padding_is_rejected() {
    st2110::Depacketizer dep = make_depacketizer();
    const st2110::PacketView pkt = parse_packet(true, {1,2,3,4}, {0,5});

    bool thrown = false;
    try {
        (void)dep.push(pkt);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }

    assert(thrown);
}

int main() {
    test_progressive_marker_packet_with_zero_trailing_padding_is_accepted();
    test_progressive_non_marker_packet_with_trailing_padding_is_rejected();
    test_progressive_marker_packet_with_non_zero_trailing_padding_is_rejected();
    return 0;
}
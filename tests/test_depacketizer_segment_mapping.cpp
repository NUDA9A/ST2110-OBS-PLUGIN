#include <cassert>
#include <cstdint>

#include <st2110/depacketizer.hpp>

static st2110::PacketView make_packet_with_one_segment(
        uint32_t rtp_timestamp,
        uint32_t ext_seq,
        bool marker,
        uint16_t row,
        uint16_t offset,
        const uint8_t* data,
        std::size_t size) {
    st2110::PacketView pkt{};
    pkt.rtp.version = 2;
    pkt.rtp.payload_type = 112;
    pkt.rtp.seq_number = static_cast<uint16_t>(ext_seq & 0xFFFFu);
    pkt.rtp.timestamp = rtp_timestamp;
    pkt.rtp.marker = marker;
    pkt.rtp.ssrc = 0x12345678;
    pkt.extended_seq = ext_seq;

    pkt.segment_count = 1;
    pkt.payload_data = st2110::ByteSpan(data, size);

    pkt.segments[0].header.length = static_cast<uint16_t>(size);
    pkt.segments[0].header.row_number = row;
    pkt.segments[0].header.offset = offset;
    pkt.segments[0].header.field_id = false;
    pkt.segments[0].header.continuation = false;
    pkt.segments[0].data = pkt.payload_data;

    return pkt;
}

static void test_depacketizer_uses_segment_placement_mapper_not_raw_byte_offset() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 4;
    cfg.height = 1;
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.scan_mode = st2110::VideoScanMode::Progressive;
    cfg.partial_frame_policy = st2110::PartialFramePolicy::EmitWithFlag;

    st2110::Depacketizer dep(cfg);

    static const uint8_t seg[] = {0xAA, 0xBB, 0xCC, 0xDD};

    auto pkt = make_packet_with_one_segment(
            1000, 1, true,
            0, 2,   // offset is in full-bandwidth samples, not bytes
            seg, sizeof(seg));

    auto out = dep.push(pkt);

    assert(out.size() == 1u);
    assert(out[0].unit_kind == st2110::VideoAssemblyUnitKind::Frame);
    assert(out[0].partial());

    const uint8_t* row0 = out[0].frame.row_data(0);

    assert(row0[0] == 0x00);
    assert(row0[1] == 0x00);
    assert(row0[2] == 0x00);
    assert(row0[3] == 0x00);

    assert(row0[4] == 0xAA);
    assert(row0[5] == 0xBB);
    assert(row0[6] == 0xCC);
    assert(row0[7] == 0xDD);
}

int main() {
    test_depacketizer_uses_segment_placement_mapper_not_raw_byte_offset();
    return 0;
}
#include <cassert>
#include <cstdint>
#include <stdexcept>

#include <st2110/ingress/shared/st2110_20.hpp>
#include <st2110/receive/video/depacketizer.hpp>

static st2110::St2110PayloadHeaderView make_payload_header_with_field_id(bool field_id) {
    st2110::St2110PayloadHeaderView h{};
    h.srd_count = 1;
    h.header_bytes = 8;

    h.srd[0].length = 4;
    h.srd[0].row_number = 0;
    h.srd[0].offset = 0;
    h.srd[0].field_id = field_id;
    h.srd[0].continuation = false;

    return h;
}

static st2110::PacketView make_packet_with_field_id(bool field_id) {
    static const uint8_t bytes[] = {0x10, 0x11, 0x12, 0x13};

    st2110::PacketView pkt{};
    pkt.rtp.version = 2;
    pkt.rtp.padding_flag = false;
    pkt.rtp.extension_flag = false;
    pkt.rtp.csrc_count = 0;
    pkt.rtp.marker = true;
    pkt.rtp.payload_type = 112;
    pkt.rtp.seq_number = 1;
    pkt.rtp.timestamp = 100;
    pkt.rtp.ssrc = 1;
    pkt.rtp.payload_offset = 0;
    pkt.rtp.payload_len = sizeof(bytes);

    pkt.extended_seq = 1;
    pkt.segment_count = 1;

    pkt.segments[0].header.length = 4;
    pkt.segments[0].header.row_number = 0;
    pkt.segments[0].header.offset = 0;
    pkt.segments[0].header.field_id = field_id;
    pkt.segments[0].header.continuation = false;
    pkt.segments[0].data = st2110::ByteSpan(bytes, sizeof(bytes));

    pkt.payload_data = st2110::ByteSpan(bytes, sizeof(bytes));
    pkt.trailing_padding = st2110::ByteSpan();

    return pkt;
}

static void test_generic_payload_header_validation_accepts_field_id() {
    const auto h = make_payload_header_with_field_id(true);

    assert(st2110::validate_st2110_20_payload_header(h) == st2110::Error::Ok);
}

static void test_progressive_runtime_accepts_field_id_zero() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 2;
    cfg.height = 1;
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.partial_frame_policy = st2110::PartialFramePolicy::EmitWithFlag;
    cfg.scan_mode = st2110::VideoScanMode::Progressive;
    cfg.packing_mode = st2110::VideoPackingMode::Gpm;

    st2110::Depacketizer dep(cfg);
    const auto pkt = make_packet_with_field_id(false);

    const auto out = dep.push(pkt);
    assert(out.size() == 1);
    assert(out[0].unit_kind == st2110::VideoAssemblyUnitKind::Frame);
    assert(out[0].marker_seen);
    assert(out[0].can_emit);
}

static void test_progressive_runtime_rejects_field_id_one_through_mode_aware_boundary() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 2;
    cfg.height = 1;
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.partial_frame_policy = st2110::PartialFramePolicy::EmitWithFlag;
    cfg.scan_mode = st2110::VideoScanMode::Progressive;
    cfg.packing_mode = st2110::VideoPackingMode::Gpm;

    st2110::Depacketizer dep(cfg);
    const auto pkt = make_packet_with_field_id(true);

    bool threw = false;
    try {
        (void)dep.push(pkt);
    } catch (const std::invalid_argument &) {
        threw = true;
    }

    assert(threw);
}

int main() {
    test_generic_payload_header_validation_accepts_field_id();
    test_progressive_runtime_accepts_field_id_zero();
    test_progressive_runtime_rejects_field_id_one_through_mode_aware_boundary();
    return 0;
}
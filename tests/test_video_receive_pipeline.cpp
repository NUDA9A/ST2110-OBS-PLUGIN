#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <st2110/video_receive_pipeline.hpp>

static st2110::PacketView make_packet_header(uint32_t rtp_timestamp, uint32_t ext_seq, bool marker) {
    st2110::PacketView pkt{};
    pkt.rtp.version = 2;
    pkt.rtp.payload_type = 112;
    pkt.rtp.seq_number = static_cast<uint16_t>(ext_seq & 0xFFFFu);
    pkt.rtp.timestamp = rtp_timestamp;
    pkt.rtp.marker = marker;
    pkt.rtp.ssrc = 0x12345678;
    pkt.extended_seq = ext_seq;
    return pkt;
}

static st2110::VideoReceivePipelineConfig make_cfg() {
    st2110::VideoReceivePipelineConfig cfg{};
    cfg.depacketizer.width = 4;
    cfg.depacketizer.height = 1;
    cfg.depacketizer.format = st2110::PixelFormat::UYVY;
    cfg.depacketizer.partial_frame_policy = st2110::PartialFramePolicy::EmitWithFlag;
    cfg.depacketizer.scan_mode = st2110::VideoScanMode::Progressive;

    cfg.reconstructor.format = st2110::PixelFormat::UYVY;
    cfg.reconstructor.scan_mode = st2110::VideoScanMode::Progressive;
    return cfg;
}

static void test_constructor_accepts_matching_progressive_configs() {
    st2110::VideoReceivePipeline pipeline(make_cfg());
    (void)pipeline;
}

static void test_constructor_rejects_format_mismatch() {
    auto cfg = make_cfg();
    cfg.reconstructor.format = static_cast<st2110::PixelFormat>(999);

    bool threw = false;
    try {
        st2110::VideoReceivePipeline pipeline(cfg);
        (void)pipeline;
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    assert(threw);
}

static void test_constructor_rejects_scan_mode_mismatch() {
    auto cfg = make_cfg();
    cfg.reconstructor.scan_mode = st2110::VideoScanMode::Interlaced;

    bool threw = false;
    try {
        st2110::VideoReceivePipeline pipeline(cfg);
        (void)pipeline;
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    assert(threw);
}

static void test_constructor_rejects_non_progressive_runtime_boundary_locally() {
    auto cfg = make_cfg();
    cfg.depacketizer.scan_mode = st2110::VideoScanMode::Interlaced;
    cfg.reconstructor.scan_mode = st2110::VideoScanMode::Interlaced;

    bool threw = false;
    try {
        st2110::VideoReceivePipeline pipeline(cfg);
        (void)pipeline;
    } catch (const std::logic_error&) {
        threw = true;
    }

    assert(threw);
}

static void test_single_packet_full_frame_flows_through_pipeline() {
    st2110::VideoReceivePipeline pipeline(make_cfg());

    static const uint8_t row0[] = {0,1,2,3,4,5,6,7};

    st2110::PacketView pkt = make_packet_header(1000, 1, true);
    pkt.segment_count = 1;
    pkt.payload_data = st2110::ByteSpan(row0, sizeof(row0));
    pkt.segments[0].header.length = 8;
    pkt.segments[0].header.row_number = 0;
    pkt.segments[0].header.offset = 0;
    pkt.segments[0].header.field_id = false;
    pkt.segments[0].header.continuation = false;
    pkt.segments[0].data = pkt.payload_data;

    auto out = pipeline.push(pkt);

    assert(out.size() == 1u);
    assert(out[0].rtp_timestamp == 1000u);
    assert(out[0].complete == true);
    assert(out[0].partial() == false);

    const uint8_t* row = out[0].frame.row_data(0);
    for (int i = 0; i < 8; ++i) {
        assert(row[i] == static_cast<uint8_t>(i));
    }
}

static void test_two_packets_same_unit_are_reconstructed_once_complete() {
    st2110::VideoReceivePipeline pipeline(make_cfg());

    static const uint8_t left[]  = {0,1,2,3};
    static const uint8_t right[] = {4,5,6,7};

    st2110::PacketView p1 = make_packet_header(2000, 10, false);
    p1.segment_count = 1;
    p1.payload_data = st2110::ByteSpan(left, sizeof(left));
    p1.segments[0].header.length = 4;
    p1.segments[0].header.row_number = 0;
    p1.segments[0].header.offset = 0;
    p1.segments[0].header.field_id = false;
    p1.segments[0].header.continuation = false;
    p1.segments[0].data = p1.payload_data;

    st2110::PacketView p2 = make_packet_header(2000, 11, true);
    p2.segment_count = 1;
    p2.payload_data = st2110::ByteSpan(right, sizeof(right));
    p2.segments[0].header.length = 4;
    p2.segments[0].header.row_number = 0;
    p2.segments[0].header.offset = 2;
    p2.segments[0].header.field_id = false;
    p2.segments[0].header.continuation = false;
    p2.segments[0].data = p2.payload_data;

    auto out1 = pipeline.push(p1);
    auto out2 = pipeline.push(p2);

    assert(out1.empty());
    assert(out2.size() == 1u);
    assert(out2[0].complete == true);

    const uint8_t* row = out2[0].frame.row_data(0);
    for (int i = 0; i < 8; ++i) {
        assert(row[i] == static_cast<uint8_t>(i));
    }
}

static void test_reset_clears_depacketizer_and_reconstructor_state() {
    st2110::VideoReceivePipeline pipeline(make_cfg());

    static const uint8_t partial[] = {0,1,2,3};
    static const uint8_t full[]    = {10,11,12,13,14,15,16,17};

    st2110::PacketView p1 = make_packet_header(3000, 1, false);
    p1.segment_count = 1;
    p1.payload_data = st2110::ByteSpan(partial, sizeof(partial));
    p1.segments[0].header.length = 4;
    p1.segments[0].header.row_number = 0;
    p1.segments[0].header.offset = 0;
    p1.segments[0].header.field_id = false;
    p1.segments[0].header.continuation = false;
    p1.segments[0].data = p1.payload_data;

    auto out1 = pipeline.push(p1);
    assert(out1.empty());

    pipeline.reset();

    st2110::PacketView p2 = make_packet_header(3001, 2, true);
    p2.segment_count = 1;
    p2.payload_data = st2110::ByteSpan(full, sizeof(full));
    p2.segments[0].header.length = 8;
    p2.segments[0].header.row_number = 0;
    p2.segments[0].header.offset = 0;
    p2.segments[0].header.field_id = false;
    p2.segments[0].header.continuation = false;
    p2.segments[0].data = p2.payload_data;

    auto out2 = pipeline.push(p2);

    assert(out2.size() == 1u);
    assert(out2[0].rtp_timestamp == 3001u);
    assert(out2[0].complete == true);

    const uint8_t* row = out2[0].frame.row_data(0);
    for (int i = 0; i < 8; ++i) {
        assert(row[i] == static_cast<uint8_t>(10 + i));
    }
}

int main() {
    test_constructor_accepts_matching_progressive_configs();
    test_constructor_rejects_format_mismatch();
    test_constructor_rejects_scan_mode_mismatch();
    test_constructor_rejects_non_progressive_runtime_boundary_locally();
    test_single_packet_full_frame_flows_through_pipeline();
    test_two_packets_same_unit_are_reconstructed_once_complete();
    test_reset_clears_depacketizer_and_reconstructor_state();
    return 0;
}
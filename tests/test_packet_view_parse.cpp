#include <cassert>
#include <cstdint>

#include <st2110/ingress/shared/packet_view.hpp>

static void test_from_udp_datagram_parses_single_segment_packet() {
    const uint8_t udp_payload[] = {0x80, 0xF0, 0x34, 0x56, 0x01, 0x02, 0x03, 0x04, 0x11, 0x22, 0x33, 0x44,

                                   0x12, 0x34, 0x00, 0x04, 0x00, 0x0A, 0x00, 0x08,

                                   0xAA, 0xBB, 0xCC, 0xDD};

    auto res = st2110::PacketView::from_udp_datagram(st2110::ByteSpan(udp_payload, sizeof(udp_payload)));
    assert(res.has_value());

    const st2110::PacketView &pkt = res.value();

    assert(pkt.rtp.version == 2);
    assert(pkt.rtp.marker == true);
    assert(pkt.rtp.payload_type == 112);
    assert(pkt.rtp.seq_number == 0x3456);
    assert(pkt.rtp.timestamp == 0x01020304);
    assert(pkt.rtp.ssrc == 0x11223344);

    assert(pkt.extended_seq == 0x12343456u);

    assert(pkt.segment_count == 1);
    assert(pkt.payload_data.size() == 4);

    assert(pkt.segments[0].header.length == 4);
    assert(pkt.segments[0].header.row_number == 10);
    assert(pkt.segments[0].header.offset == 8);
    assert(pkt.segments[0].header.field_id == false);
    assert(pkt.segments[0].header.continuation == false);

    assert(pkt.segments[0].data.size() == 4);
    assert(pkt.segments[0].data[0] == 0xAA);
    assert(pkt.segments[0].data[1] == 0xBB);
    assert(pkt.segments[0].data[2] == 0xCC);
    assert(pkt.segments[0].data[3] == 0xDD);
}

static void test_from_udp_datagram_parses_two_segment_packet() {
    const uint8_t udp_payload[] = {0x80, 0x70, 0x00, 0x22, 0x00, 0x00, 0x00, 0x99, 0xDE, 0xAD, 0xBE, 0xEF,

                                   0x00, 0x01,

                                   0x00, 0x04, 0x00, 0x05, 0x80, 0x00,

                                   0x00, 0x06, 0x00, 0x06, 0x00, 0x08,

                                   0x10, 0x11, 0x12, 0x13, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25};

    auto res = st2110::PacketView::from_udp_datagram(st2110::ByteSpan(udp_payload, sizeof(udp_payload)));
    assert(res.has_value());

    const st2110::PacketView &pkt = res.value();

    assert(pkt.rtp.version == 2);
    assert(pkt.rtp.marker == false);
    assert(pkt.rtp.payload_type == 112);
    assert(pkt.rtp.seq_number == 0x0022);
    assert(pkt.rtp.timestamp == 0x00000099);
    assert(pkt.rtp.ssrc == 0xDEADBEEF);

    assert(pkt.extended_seq == 0x00010022u);

    assert(pkt.segment_count == 2);
    assert(pkt.payload_data.size() == 10);

    assert(pkt.segments[0].header.length == 4);
    assert(pkt.segments[0].header.row_number == 5);
    assert(pkt.segments[0].header.offset == 0);
    assert(pkt.segments[0].header.continuation == true);
    assert(pkt.segments[0].data.size() == 4);
    assert(pkt.segments[0].data[0] == 0x10);
    assert(pkt.segments[0].data[3] == 0x13);

    assert(pkt.segments[1].header.length == 6);
    assert(pkt.segments[1].header.row_number == 6);
    assert(pkt.segments[1].header.offset == 8);
    assert(pkt.segments[1].header.continuation == false);
    assert(pkt.segments[1].data.size() == 6);
    assert(pkt.segments[1].data[0] == 0x20);
    assert(pkt.segments[1].data[5] == 0x25);
}

static void test_from_udp_datagram_allows_trailing_padding_after_srd_data() {
    const uint8_t udp_payload[] = {0x80, 0x70, 0x12, 0x34, 0x00, 0x00, 0x00, 0x55, 0x00, 0x00, 0x00, 0x01,

                                   0x00, 0x02, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00,

                                   0xCA, 0xFE, 0xBA, 0xBE, 0x00, 0x00, 0x00};

    auto res = st2110::PacketView::from_udp_datagram(st2110::ByteSpan(udp_payload, sizeof(udp_payload)));
    assert(res.has_value());

    const st2110::PacketView &pkt = res.value();

    assert(pkt.segment_count == 1);
    assert(pkt.payload_data.size() == 7);
    assert(pkt.segments[0].data.size() == 4);

    assert(pkt.segments[0].data[0] == 0xCA);
    assert(pkt.segments[0].data[1] == 0xFE);
    assert(pkt.segments[0].data[2] == 0xBA);
    assert(pkt.segments[0].data[3] == 0xBE);
}

static void test_from_udp_datagram_rejects_when_srd_lengths_exceed_payload_data() {
    const uint8_t udp_payload[] = {0x80, 0x70, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02,

                                   0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00,

                                   0x11, 0x22, 0x33, 0x44};

    auto res = st2110::PacketView::from_udp_datagram(st2110::ByteSpan(udp_payload, sizeof(udp_payload)));
    assert(!res.has_value());
    assert(res.error() == st2110::Error::ShortPacket);
}

int main() {
    test_from_udp_datagram_parses_single_segment_packet();
    test_from_udp_datagram_parses_two_segment_packet();
    test_from_udp_datagram_allows_trailing_padding_after_srd_data();
    test_from_udp_datagram_rejects_when_srd_lengths_exceed_payload_data();
    return 0;
}
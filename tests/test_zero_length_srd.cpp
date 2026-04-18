#include <cassert>

#include <st2110/packet_view.hpp>
#include <st2110/st2110_20.hpp>

static void test_validate_allows_single_zero_length_srd() {
    st2110::St2110PayloadHeaderView h{};
    h.ext_seq.hi16 = 0x1234;
    h.srd_count = 1;
    h.header_bytes = 8;

    h.srd[0].length = 0;
    h.srd[0].row_number = 0;
    h.srd[0].offset = 0;
    h.srd[0].field_id = false;
    h.srd[0].continuation = false;

    assert(st2110::validate_st2110_20_payload_header(h) == st2110::Error::Ok);
}

static void test_validate_rejects_zero_length_srd_in_multi_srd_header() {
    st2110::St2110PayloadHeaderView h{};
    h.ext_seq.hi16 = 0x1234;
    h.srd_count = 2;
    h.header_bytes = 14;

    h.srd[0].length = 0;
    h.srd[0].row_number = 0;
    h.srd[0].offset = 0;
    h.srd[0].field_id = false;
    h.srd[0].continuation = true;

    h.srd[1].length = 4;
    h.srd[1].row_number = 1;
    h.srd[1].offset = 0;
    h.srd[1].field_id = false;
    h.srd[1].continuation = false;

    assert(st2110::validate_st2110_20_payload_header(h) == st2110::Error::InvalidValue);
}

static void test_packet_view_accepts_single_zero_length_srd_without_payload_data() {
    const uint8_t udp_payload[] = {
            0x80, 0x70,
            0x00, 0x01,
            0x00, 0x00, 0x00, 0x11,
            0x00, 0x00, 0x00, 0x22,

            0x12, 0x34,
            0x00, 0x00,
            0x00, 0x00,
            0x00, 0x00
    };

    auto res = st2110::PacketView::from_udp_datagram(
            st2110::ByteSpan(udp_payload, sizeof(udp_payload)));

    assert(res.has_value());

    const st2110::PacketView& pkt = res.value();
    assert(pkt.extended_seq == 0x12340001u);
    assert(pkt.segment_count == 1);
    assert(pkt.payload_data.size() == 0);
    assert(pkt.segments[0].header.length == 0);
    assert(pkt.segments[0].data.size() == 0);
}

static void test_packet_view_rejects_single_zero_length_srd_with_extra_payload_bytes() {
    const uint8_t udp_payload[] = {
            0x80, 0x70,
            0x00, 0x01,
            0x00, 0x00, 0x00, 0x11,
            0x00, 0x00, 0x00, 0x22,

            0x12, 0x34,
            0x00, 0x00,
            0x00, 0x00,
            0x00, 0x00,

            0xAA, 0xBB
    };

    auto res = st2110::PacketView::from_udp_datagram(
            st2110::ByteSpan(udp_payload, sizeof(udp_payload)));

    assert(!res.has_value());
    assert(res.error() == st2110::Error::InvalidValue);
}

int main() {
    test_validate_allows_single_zero_length_srd();
    test_validate_rejects_zero_length_srd_in_multi_srd_header();
    test_packet_view_accepts_single_zero_length_srd_without_payload_data();
    test_packet_view_rejects_single_zero_length_srd_with_extra_payload_bytes();
    return 0;
}
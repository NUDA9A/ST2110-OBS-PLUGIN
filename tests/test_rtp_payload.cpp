#include <cassert>
#include <cstdint>

#include <st2110/foundation/bytes.hpp>
#include <st2110/ingress/shared/rtp.hpp>

static void test_basic_payload_span() {
    const uint8_t pkt[] = {0x80, 0xF0, 0x12, 0x34, 0x01, 0x02, 0x03, 0x04, 0x0A, 0x0B, 0x0C, 0x0D, 0x11, 0x22, 0x33};

    auto res = st2110::parse_rtp_header(st2110::ByteSpan{pkt});
    assert(res.has_value());

    auto payload = st2110::rtp_payload_span(st2110::ByteSpan{pkt}, res.value());
    assert(payload.size() == 3);
    assert(payload[0] == 0x11);
    assert(payload[1] == 0x22);
    assert(payload[2] == 0x33);
}

static void test_payload_span_with_padding() {
    const uint8_t pkt[] = {0xA0, 0x70, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
                           0x00, 0x00, 0x00, 0x02, 0xAA, 0xBB, 0x00, 0x02};

    auto res = st2110::parse_rtp_header(st2110::ByteSpan{pkt});
    assert(res.has_value());

    auto payload = st2110::rtp_payload_span(st2110::ByteSpan{pkt}, res.value());
    assert(payload.size() == 2);
    assert(payload[0] == 0xAA);
    assert(payload[1] == 0xBB);
}

static void test_payload_span_with_extension() {
    const uint8_t pkt[] = {0x90, 0x60, 0x00, 0x02, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x20,
                           0xBE, 0xDE, 0x00, 0x01, 0xDE, 0xAD, 0xBE, 0xEF, 0x55, 0x66, 0x77};

    auto res = st2110::parse_rtp_header(st2110::ByteSpan{pkt});
    assert(res.has_value());

    auto payload = st2110::rtp_payload_span(st2110::ByteSpan{pkt}, res.value());
    assert(payload.size() == 3);
    assert(payload[0] == 0x55);
    assert(payload[1] == 0x66);
    assert(payload[2] == 0x77);
}

static void test_payload_span_with_csrc_and_extension() {
    const uint8_t pkt[] = {0x92, 0x60, 0x00, 0x03, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x22,

                           0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x33, 0x44,

                           0x10, 0x00, 0x00, 0x01, 0x01, 0x02, 0x03, 0x04,

                           0x99, 0x88};

    auto res = st2110::parse_rtp_header(st2110::ByteSpan{pkt});
    assert(res.has_value());

    auto payload = st2110::rtp_payload_span(st2110::ByteSpan{pkt}, res.value());
    assert(payload.size() == 2);
    assert(payload[0] == 0x99);
    assert(payload[1] == 0x88);
}

int main() {
    test_basic_payload_span();
    test_payload_span_with_padding();
    test_payload_span_with_extension();
    test_payload_span_with_csrc_and_extension();
    return 0;
}
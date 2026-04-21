#include <cassert>
#include <cstdint>
#include <vector>

#include <st2110/bytes.hpp>
#include <st2110/error.hpp>
#include <st2110/packet_parse.hpp>
#include <st2110/packet_view.hpp>

static std::vector<uint8_t> make_valid_packet() {
    return {
            // RTP header (12 bytes)
            0x80, 0x70,             // V=2, P=0, X=0, CC=0, M=0, PT=112
            0x00, 0x01,             // seq
            0x00, 0x00, 0x00, 0x2A, // timestamp
            0x12, 0x34, 0x56, 0x78, // ssrc

            // ST 2110-20 payload header (8 bytes total)
            0x00, 0x00,             // ext seq hi16
            0x00, 0x04,             // SRD length = 4
            0x00, 0x00,             // F=0, row=0
            0x00, 0x00,             // C=0, offset=0

            // SRD payload (4 bytes)
            0xAA, 0xBB, 0xCC, 0xDD
    };
}

static std::vector<uint8_t> make_structurally_invalid_packet_short_srd_data() {
    return {
            // RTP header (12 bytes)
            0x80, 0x70,             // V=2, P=0, X=0, CC=0, M=0, PT=112
            0x00, 0x01,             // seq
            0x00, 0x00, 0x00, 0x2A, // timestamp
            0x12, 0x34, 0x56, 0x78, // ssrc

            // ST 2110-20 payload header
            0x00, 0x00,             // ext seq hi16
            0x00, 0x08,             // SRD length = 8, but actual payload below is only 4 bytes
            0x00, 0x00,             // F=0, row=0
            0x00, 0x00,             // C=0, offset=0

            // only 4 bytes instead of 8
            0xAA, 0xBB, 0xCC, 0xDD
    };
}

static void test_pure_packet_view_parse_accepts_valid_packet() {
    const auto bytes = make_valid_packet();

    const auto parsed = st2110::PacketView::from_udp_datagram(
            st2110::ByteSpan(bytes.data(), bytes.size()));

    assert(parsed.has_value());
    assert(parsed->segment_count == 1u);
    assert(parsed->segments[0].header.length == 4u);
    assert(parsed->payload_data.size() == 4u);
}

static void test_parse_packet_view_without_policy_accepts_valid_packet() {
    const auto bytes = make_valid_packet();

    const auto parsed = st2110::parse_packet_view(
            st2110::ByteSpan(bytes.data(), bytes.size()));

    assert(parsed.has_value());
    assert(parsed->segment_count == 1u);
}

static void test_parse_packet_view_accepts_packet_at_exact_limit() {
    const auto bytes = make_valid_packet();

    st2110::PacketParsePolicy policy{};
    policy.max_udp_payload_bytes = bytes.size();

    const auto parsed = st2110::parse_packet_view(
            st2110::ByteSpan(bytes.data(), bytes.size()), policy);

    assert(parsed.has_value());
}

static void test_parse_packet_view_rejects_packet_over_limit() {
    const auto bytes = make_valid_packet();

    st2110::PacketParsePolicy policy{};
    policy.max_udp_payload_bytes = bytes.size() - 1;

    const auto parsed = st2110::parse_packet_view(
            st2110::ByteSpan(bytes.data(), bytes.size()), policy);

    assert(!parsed.has_value());
    assert(parsed.error() == st2110::Error::InvalidValue);
}

static void test_pure_packet_view_parse_rejects_inconsistent_payload_shape() {
    const auto bytes = make_structurally_invalid_packet_short_srd_data();

    const auto parsed = st2110::PacketView::from_udp_datagram(
            st2110::ByteSpan(bytes.data(), bytes.size()));

    assert(!parsed.has_value());
    assert(parsed.error() == st2110::Error::ShortPacket);
}

static void test_policy_wrapper_does_not_hide_structural_parse_error() {
    const auto bytes = make_structurally_invalid_packet_short_srd_data();

    st2110::PacketParsePolicy policy{};
    policy.max_udp_payload_bytes = bytes.size();

    const auto parsed = st2110::parse_packet_view(
            st2110::ByteSpan(bytes.data(), bytes.size()), policy);

    assert(!parsed.has_value());
    assert(parsed.error() == st2110::Error::ShortPacket);
}

int main() {
    test_pure_packet_view_parse_accepts_valid_packet();
    test_parse_packet_view_without_policy_accepts_valid_packet();
    test_parse_packet_view_accepts_packet_at_exact_limit();
    test_parse_packet_view_rejects_packet_over_limit();
    test_pure_packet_view_parse_rejects_inconsistent_payload_shape();
    test_policy_wrapper_does_not_hide_structural_parse_error();
    return 0;
}
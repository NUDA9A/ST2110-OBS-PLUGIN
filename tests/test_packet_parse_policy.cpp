#include <cassert>
#include <cstdint>
#include <vector>

#include <st2110/packet_parse.hpp>

static void test_udp_datagram_size_helper_adds_udp_header_bytes() {
    std::vector<uint8_t> payload(1452, 0x00);
    assert(st2110::udp_datagram_size_bytes(st2110::ByteSpan(payload.data(), payload.size())) == 1460u);
}

static void test_default_policy_uses_standard_udp_datagram_size_limit_boundary() {
    std::vector<uint8_t> ok_payload(1452, 0x00);   // 1452 + 8 = 1460
    std::vector<uint8_t> bad_payload(1453, 0x00);  // 1453 + 8 = 1461

    st2110::PacketParsePolicy policy{};

    assert(
            st2110::validate_packet_parse_policy(
                    st2110::ByteSpan(ok_payload.data(), ok_payload.size()),
                    policy
            ) == st2110::Error::Ok
    );

    assert(
            st2110::validate_packet_parse_policy(
                    st2110::ByteSpan(bad_payload.data(), bad_payload.size()),
                    policy
            ) == st2110::Error::InvalidValue
    );
}

static void test_explicit_custom_datagram_limit_uses_datagram_semantics() {
    st2110::PacketParsePolicy policy{};
    policy.max_udp_datagram_bytes = 1200;

    std::vector<uint8_t> ok_payload(1192, 0x00);   // 1192 + 8 = 1200
    std::vector<uint8_t> bad_payload(1193, 0x00);  // 1193 + 8 = 1201

    assert(
            st2110::validate_packet_parse_policy(
                    st2110::ByteSpan(ok_payload.data(), ok_payload.size()),
                    policy
            ) == st2110::Error::Ok
    );

    assert(
            st2110::validate_packet_parse_policy(
                    st2110::ByteSpan(bad_payload.data(), bad_payload.size()),
                    policy
            ) == st2110::Error::InvalidValue
    );
}

static void test_explicit_extended_limit_accepts_up_to_extended_boundary() {
    st2110::PacketParsePolicy policy{};
    policy.max_udp_datagram_bytes = st2110::extendedUdpDatagramSizeLimitBytes;

    std::vector<uint8_t> ok_payload(8952, 0x00);   // 8952 + 8 = 8960
    std::vector<uint8_t> bad_payload(8953, 0x00);  // 8953 + 8 = 8961

    assert(
            st2110::validate_packet_parse_policy(
                    st2110::ByteSpan(ok_payload.data(), ok_payload.size()),
                    policy
            ) == st2110::Error::Ok
    );

    assert(
            st2110::validate_packet_parse_policy(
                    st2110::ByteSpan(bad_payload.data(), bad_payload.size()),
                    policy
            ) == st2110::Error::InvalidValue
    );
}

static void test_parse_packet_view_rejects_oversized_packet_at_policy_stage_before_wire_parse() {
    std::vector<uint8_t> oversized_payload(1453, 0x00); // default policy => 1461 datagram bytes

    auto result = st2110::parse_packet_view(
            st2110::ByteSpan(oversized_payload.data(), oversized_payload.size())
    );

    assert(!result.has_value());
    assert(result.error() == st2110::Error::InvalidValue);
}

static void test_policy_config_rejects_too_small_max_udp_datagram_size() {
    st2110::PacketParsePolicy policy{};
    policy.max_udp_datagram_bytes = 1;

    assert(st2110::validate_packet_parse_policy_config(policy) == st2110::Error::InvalidValue);
}

static void test_parse_packet_view_rejects_invalid_policy_config_before_packet_checks() {
    st2110::PacketParsePolicy policy{};
    policy.max_udp_datagram_bytes = 1;

    const uint8_t payload[] = {0x00};

    auto result = st2110::parse_packet_view(
            st2110::ByteSpan(payload, sizeof(payload)),
            policy);

    assert(!result.has_value());
    assert(result.error() == st2110::Error::InvalidValue);
}

int main() {
    test_udp_datagram_size_helper_adds_udp_header_bytes();
    test_default_policy_uses_standard_udp_datagram_size_limit_boundary();
    test_explicit_custom_datagram_limit_uses_datagram_semantics();
    test_explicit_extended_limit_accepts_up_to_extended_boundary();
    test_parse_packet_view_rejects_oversized_packet_at_policy_stage_before_wire_parse();
    test_policy_config_rejects_too_small_max_udp_datagram_size();
    test_parse_packet_view_rejects_invalid_policy_config_before_packet_checks();
    return 0;
}
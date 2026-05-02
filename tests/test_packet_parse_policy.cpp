#include <array>
#include <cassert>
#include <cstdint>
#include <vector>

#include <st2110/packet_parse.hpp>

static void test_udp_datagram_size_helper_adds_udp_header_bytes() {
    std::vector<uint8_t> payload(1452, 0x00);
    assert(st2110::udp_datagram_size_bytes(st2110::ByteSpan(payload.data(), payload.size())) == 1460u);
}

static void test_absent_policy_defaults_to_standard_udp_datagram_size_limit() {
    st2110::PacketParsePolicy policy{};

    assert(st2110::validate_packet_parse_policy_config(policy) == st2110::Error::Ok);
    assert(st2110::effective_max_udp_datagram_bytes(policy) == st2110::standardUdpDatagramSizeLimitBytes);

    std::vector<uint8_t> ok_payload(1452, 0x00);  // 1452 + 8 = 1460
    std::vector<uint8_t> bad_payload(1453, 0x00); // 1453 + 8 = 1461

    assert(st2110::validate_packet_parse_policy(st2110::ByteSpan(ok_payload.data(), ok_payload.size()), policy) ==
           st2110::Error::Ok);
    assert(st2110::validate_packet_parse_policy(st2110::ByteSpan(bad_payload.data(), bad_payload.size()), policy) ==
           st2110::Error::InvalidValue);
}

static void test_explicit_standard_limit_is_accepted_and_enforced() {
    st2110::PacketParsePolicy policy{};
    policy.max_udp_datagram_bytes = st2110::standardUdpDatagramSizeLimitBytes;

    assert(st2110::validate_packet_parse_policy_config(policy) == st2110::Error::Ok);
    assert(st2110::effective_max_udp_datagram_bytes(policy) == st2110::standardUdpDatagramSizeLimitBytes);

    std::vector<uint8_t> ok_payload(1452, 0x00);  // 1452 + 8 = 1460
    std::vector<uint8_t> bad_payload(1453, 0x00); // 1453 + 8 = 1461

    assert(st2110::validate_packet_parse_policy(st2110::ByteSpan(ok_payload.data(), ok_payload.size()), policy) ==
           st2110::Error::Ok);
    assert(st2110::validate_packet_parse_policy(st2110::ByteSpan(bad_payload.data(), bad_payload.size()), policy) ==
           st2110::Error::InvalidValue);
}

static void test_explicit_extended_limit_is_accepted_and_enforced() {
    st2110::PacketParsePolicy policy{};
    policy.max_udp_datagram_bytes = st2110::extendedUdpDatagramSizeLimitBytes;

    assert(st2110::validate_packet_parse_policy_config(policy) == st2110::Error::Ok);
    assert(st2110::effective_max_udp_datagram_bytes(policy) == st2110::extendedUdpDatagramSizeLimitBytes);

    std::vector<uint8_t> ok_payload(8952, 0x00);  // 8952 + 8 = 8960
    std::vector<uint8_t> bad_payload(8953, 0x00); // 8953 + 8 = 8961

    assert(st2110::validate_packet_parse_policy(st2110::ByteSpan(ok_payload.data(), ok_payload.size()), policy) ==
           st2110::Error::Ok);
    assert(st2110::validate_packet_parse_policy(st2110::ByteSpan(bad_payload.data(), bad_payload.size()), policy) ==
           st2110::Error::InvalidValue);
}

static void test_non_boundary_policy_values_are_rejected() {
    constexpr std::array<std::size_t, 6> invalid_limits{
        1u,
        1200u,
        1461u,
        4096u,
        8959u,
        8961u,
    };

    for (std::size_t bytes : invalid_limits) {
        st2110::PacketParsePolicy policy{};
        policy.max_udp_datagram_bytes = bytes;
        assert(st2110::validate_packet_parse_policy_config(policy) == st2110::Error::InvalidValue);
    }
}

static void test_parse_packet_view_rejects_oversized_packet_at_policy_stage_before_wire_parse() {
    std::vector<uint8_t> oversized_payload(1453, 0x00); // default policy => 1461 datagram bytes

    auto result = st2110::parse_packet_view(st2110::ByteSpan(oversized_payload.data(), oversized_payload.size()));

    assert(!result.has_value());
    assert(result.error() == st2110::Error::InvalidValue);
}

static void test_parse_packet_view_rejects_invalid_policy_config_before_packet_checks() {
    st2110::PacketParsePolicy policy{};
    policy.max_udp_datagram_bytes = 4096;

    const uint8_t payload[] = {0x00};

    auto result = st2110::parse_packet_view(st2110::ByteSpan(payload, sizeof(payload)), policy);

    assert(!result.has_value());
    assert(result.error() == st2110::Error::InvalidValue);
}

int main() {
    test_udp_datagram_size_helper_adds_udp_header_bytes();
    test_absent_policy_defaults_to_standard_udp_datagram_size_limit();
    test_explicit_standard_limit_is_accepted_and_enforced();
    test_explicit_extended_limit_is_accepted_and_enforced();
    test_non_boundary_policy_values_are_rejected();
    test_parse_packet_view_rejects_oversized_packet_at_policy_stage_before_wire_parse();
    test_parse_packet_view_rejects_invalid_policy_config_before_packet_checks();
    return 0;
}
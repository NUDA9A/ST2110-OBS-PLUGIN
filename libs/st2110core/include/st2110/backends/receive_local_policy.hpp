#ifndef ST2110_OBS_RECEIVE_LOCAL_POLICY_HPP
#define ST2110_OBS_RECEIVE_LOCAL_POLICY_HPP

#include <st2110/foundation/error.hpp>
#include <st2110/receive/shared/receive_bootstrap.hpp>

#include <charconv>
#include <expected>
#include <string>
#include <vector>
#include <cstdint>
#include <string_view>

namespace st2110 {
enum class SocketAddressFamily {
    IPv4,
    IPv6,
};

struct ReceiveLocalLegPolicy {
    SocketAddressFamily family = SocketAddressFamily::IPv4;
    std::string local_ip{};
};

struct ReceiveLocalPolicy {
    std::vector<ReceiveLocalLegPolicy> legs{};
};

struct ReceiveRouteLookupTarget {
    SocketAddressFamily family = SocketAddressFamily::IPv4;
    std::string remote_ip{};
};

[[nodiscard]] inline std::expected<uint8_t, Error> parse_ipv4_block(std::string_view block) {
    if (block.empty() || block.size() > 3) {
        return std::unexpected(Error::InvalidValue);
    }

    unsigned int value = 0;

    const char *first = block.data();
    const char *last = block.data() + block.size();

    const auto [ptr, ec] = std::from_chars(first, last, value);

    if (ec != std::errc{} || ptr != last || value > 255) {
        return std::unexpected(Error::InvalidValue);
    }

    return static_cast<uint8_t>(value);
}

[[nodiscard]] inline bool is_valid_ipv4_address(std::string_view address) noexcept {
    int part_count = 0;

    while (true) {
        if (part_count == 4) {
            return false;
        }

        const std::size_t dot = address.find('.');
        const std::string_view block = (dot == std::string_view::npos) ? address : address.substr(0, dot);

        const auto parsed = parse_ipv4_block(block);
        if (!parsed) {
            return false;
        }

        ++part_count;

        if (dot == std::string_view::npos) {
            break;
        }

        address.remove_prefix(dot + 1);
    }

    return part_count == 4;
}

[[nodiscard]] inline bool is_ascii_hex_digit(char c) noexcept {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

[[nodiscard]] inline bool is_valid_ipv6_hextet(std::string_view hextet) noexcept {
    if (hextet.empty() || hextet.size() > 4) {
        return false;
    }

    for (char c : hextet) {
        if (!is_ascii_hex_digit(c)) {
            return false;
        }
    }

    return true;
}

[[nodiscard]] inline bool parse_ipv6_side(std::string_view side, bool allow_ipv4_tail, int &group_count) noexcept {
    group_count = 0;

    if (side.empty()) {
        return true;
    }

    while (!side.empty()) {
        const std::size_t colon = side.find(':');
        const std::string_view token = (colon == std::string_view::npos) ? side : side.substr(0, colon);

        if (token.empty()) {
            return false;
        }

        if (allow_ipv4_tail && colon == std::string_view::npos && token.find('.') != std::string_view::npos) {
            if (!is_valid_ipv4_address(token)) {
                return false;
            }

            group_count += 2;
            return true;
        }

        if (!is_valid_ipv6_hextet(token)) {
            return false;
        }

        ++group_count;

        if (colon == std::string_view::npos) {
            return true;
        }

        if (colon + 1 >= side.size()) {
            return false;
        }

        side.remove_prefix(colon + 1);
    }

    return true;
}

[[nodiscard]] inline bool is_valid_ipv6_address(std::string_view address) noexcept {
    if (address.empty()) {
        return false;
    }

    // zone id вида "%eth0" здесь специально не поддерживается
    if (address.find('%') != std::string_view::npos) {
        return false;
    }

    const std::size_t double_colon = address.find("::");

    if (double_colon == std::string_view::npos) {
        int groups = 0;
        return parse_ipv6_side(address, true, groups) && groups == 8;
    }

    if (address.find("::", double_colon + 2) != std::string_view::npos) {
        return false;
    }

    const std::string_view left = address.substr(0, double_colon);
    const std::string_view right = address.substr(double_colon + 2);

    int left_groups = 0;
    int right_groups = 0;

    if (!parse_ipv6_side(left, false, left_groups)) {
        return false;
    }

    if (!parse_ipv6_side(right, true, right_groups)) {
        return false;
    }

    // "::" должно заменять хотя бы одну группу
    return (left_groups + right_groups) < 8;
}

[[nodiscard]] inline std::expected<SocketAddressFamily, Error>
determine_receive_remote_leg_family(const ReceiveRemoteLeg &leg) {
    if (leg.destination.address_type == "IP4") {
        return SocketAddressFamily::IPv4;
    }

    if (leg.destination.address_type == "IP6") {
        return SocketAddressFamily::IPv6;
    }

    if (is_valid_ipv4_address(leg.destination.destination_address)) {
        return SocketAddressFamily::IPv4;
    }

    if (is_valid_ipv6_address(leg.destination.destination_address)) {
        return SocketAddressFamily::IPv6;
    }

    return std::unexpected(Error::InvalidValue);
}

[[nodiscard]] inline bool is_valid_address(std::string_view address, SocketAddressFamily family) noexcept {
    switch (family) {
    case SocketAddressFamily::IPv4:
        return is_valid_ipv4_address(address);
    case SocketAddressFamily::IPv6:
        return is_valid_ipv6_address(address);
    }

    return false;
}

[[nodiscard]] inline std::expected<std::string, Error>
determine_receive_remote_leg_route_lookup_target_ip(const ReceiveRemoteLeg &leg,
                                                    const SocketAddressFamily family) {
    if (!leg.source_filter.source_addresses.empty()) {
        const std::string &source_ip = leg.source_filter.source_addresses.front();
        if (!is_valid_address(source_ip, family)) {
            return std::unexpected(Error::InvalidValue);
        }

        return source_ip;
    }

    if (!is_valid_address(leg.destination.destination_address, family)) {
        return std::unexpected(Error::InvalidValue);
    }

    return leg.destination.destination_address;
}

[[nodiscard]] inline std::expected<ReceiveRouteLookupTarget, Error>
determine_receive_route_lookup_target(const ReceiveRemoteLeg &leg) {
    auto family = determine_receive_remote_leg_family(leg);
    if (!family.has_value()) {
        return std::unexpected(family.error());
    }

    auto remote_ip = determine_receive_remote_leg_route_lookup_target_ip(leg, *family);
    if (!remote_ip.has_value()) {
        return std::unexpected(remote_ip.error());
    }

    return ReceiveRouteLookupTarget{
        .family = *family,
        .remote_ip = std::move(*remote_ip),
    };
}

[[nodiscard]] std::expected<std::string, Error>
resolve_preferred_local_ip_for_remote_target(SocketAddressFamily family, const std::string &remote_ip);

[[nodiscard]] std::expected<ReceiveLocalPolicy, Error>
auto_select_receive_local_policy(const ReceiveBootstrap &bootstrap);

} // namespace st2110

#endif // ST2110_OBS_RECEIVE_LOCAL_POLICY_HPP
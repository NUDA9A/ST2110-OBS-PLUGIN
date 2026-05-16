#ifndef ST2110_OBS_PLUGIN_MTL_RUNTIME_RESOLVER_HPP
#define ST2110_OBS_PLUGIN_MTL_RUNTIME_RESOLVER_HPP

#include <st2110/backends/mtl/mtl_runtime_config.hpp>
#include <st2110/backends/receive_local_policy.hpp>
#include <st2110/foundation/error.hpp>
#include <st2110/receive/shared/receive_bootstrap.hpp>

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

#ifndef ST2110_MTL_DEV_KERNEL_SOCKET
#define ST2110_MTL_DEV_KERNEL_SOCKET 0
#endif

namespace st2110 {

/*
 * Common MTL runtime resolver.
 *
 * This boundary owns only source / SDP / receive-bootstrap / local-policy-derived
 * values that affect mtl_init:
 *
 * - selected local MTL port identity;
 * - SIP address assigned to that local port;
 * - primary vs redundant port topology.
 *
 * Fixed project runtime policy remains outside MtlRuntimeConfig and is projected
 * inside the worker when building mtl_init_params:
 *
 * - product/dev PMD selection;
 * - net_proto;
 * - queue counts;
 * - init flags.
 */
[[nodiscard]] inline std::expected<std::uint8_t, Error> parse_mtl_ipv4_octet(std::string_view value) {
    if (value.empty() || value.size() > 3) {
        return std::unexpected(Error::InvalidValue);
    }

    unsigned int parsed = 0;
    const char *first = value.data();
    const char *last = value.data() + value.size();

    const auto [ptr, ec] = std::from_chars(first, last, parsed);
    if (ec != std::errc{} || ptr != last || parsed > 255) {
        return std::unexpected(Error::InvalidValue);
    }

    return static_cast<std::uint8_t>(parsed);
}

[[nodiscard]] inline std::expected<std::array<std::uint8_t, 4>, Error>
parse_mtl_ipv4_address(std::string_view address) {
    std::array<std::uint8_t, 4> result{};

    for (std::size_t i = 0; i < result.size(); ++i) {
        const std::size_t dot = address.find('.');
        const std::string_view token = (dot == std::string_view::npos) ? address : address.substr(0, dot);

        auto octet = parse_mtl_ipv4_octet(token);
        if (!octet.has_value()) {
            return std::unexpected(octet.error());
        }

        result[i] = *octet;

        if (i + 1 == result.size()) {
            if (dot != std::string_view::npos) {
                return std::unexpected(Error::InvalidValue);
            }

            return result;
        }

        if (dot == std::string_view::npos) {
            return std::unexpected(Error::InvalidValue);
        }

        address.remove_prefix(dot + 1);
    }

    return result;
}

[[nodiscard]] inline std::expected<MtlRuntimePortConfig, Error>
project_receive_local_leg_to_mtl_runtime_port(const ReceiveLocalLegPolicy &local_leg) {
    if (local_leg.family != SocketAddressFamily::IPv4) {
        return std::unexpected(Error::Unsupported);
    }

    auto sip_addr = parse_mtl_ipv4_address(local_leg.local_ip);
    if (!sip_addr.has_value()) {
        return std::unexpected(sip_addr.error());
    }

#if ST2110_MTL_DEV_KERNEL_SOCKET
    if (!local_leg.local_interface_name.has_value()) {
        return std::unexpected(Error::Unsupported);
    }

    if (local_leg.local_interface_name->empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    return MtlRuntimePortConfig{
        .port_name = std::string("kernel:") + *local_leg.local_interface_name,
        .sip_addr = *sip_addr,
    };
#else
    if (!local_leg.local_pci_bdf.has_value()) {
        return std::unexpected(Error::Unsupported);
    }

    if (local_leg.local_pci_bdf->empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    return MtlRuntimePortConfig{
        .port_name = *local_leg.local_pci_bdf,
        .sip_addr = *sip_addr,
    };
#endif
}

[[nodiscard]] inline std::expected<MtlRuntimeConfig, Error>
project_receive_local_policy_to_mtl_runtime_config(const ReceiveBootstrap &bootstrap,
                                                   const ReceiveLocalPolicy &local_policy) {
    if (bootstrap.legs.empty() || local_policy.legs.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    if (bootstrap.legs.size() != local_policy.legs.size()) {
        return std::unexpected(Error::InvalidValue);
    }

    auto primary = project_receive_local_leg_to_mtl_runtime_port(local_policy.legs[0]);
    if (!primary.has_value()) {
        return std::unexpected(primary.error());
    }

    MtlRuntimeConfig result{
        .primary_port = *primary,
        .redundant_port = std::nullopt,
    };

    switch (bootstrap.topology) {
    case ReceiveTopologyKind::SingleStream:
        if (local_policy.legs.size() != 1) {
            return std::unexpected(Error::InvalidValue);
        }

        return result;

    case ReceiveTopologyKind::RedundantPair: {
        if (local_policy.legs.size() != 2) {
            return std::unexpected(Error::InvalidValue);
        }

        auto redundant = project_receive_local_leg_to_mtl_runtime_port(local_policy.legs[1]);
        if (!redundant.has_value()) {
            return std::unexpected(redundant.error());
        }

        result.redundant_port = *redundant;
        return result;
    }
    }

    return std::unexpected(Error::InvalidValue);
}

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_MTL_RUNTIME_RESOLVER_HPP
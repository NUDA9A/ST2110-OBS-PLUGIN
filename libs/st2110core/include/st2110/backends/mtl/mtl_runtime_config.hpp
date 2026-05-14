#ifndef ST2110_OBS_PLUGIN_MTL_RUNTIME_CONFIG_HPP
#define ST2110_OBS_PLUGIN_MTL_RUNTIME_CONFIG_HPP

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace st2110 {

struct MtlRuntimePortConfig {
    /*
     * MTL device port identifier.
     *
     * For the intended Linux/DPDK installation path this is the PCI BDF,
     * for example "0000:af:01.0".
     *
     * Projection to mtl_init_params::port[...] is owned by the MTL backend.
     */
    std::string port_name{};

    /*
     * IPv4 address assigned to this MTL device port.
     *
     * Projection target:
     *   mtl_init_params::sip_addr[...]
     */
    std::array<std::uint8_t, 4> sip_addr{};
};

struct MtlRuntimeConfig {
    /*
     * Primary MTL device port.
     *
     * Projection target:
     *   mtl_init_params::port[MTL_PORT_P]
     *   mtl_init_params::sip_addr[MTL_PORT_P]
     */
    MtlRuntimePortConfig primary_port{};

    /*
     * Optional redundant MTL device port.
     *
     * If present, backend projection may initialize MTL_PORT_R and build
     * two-port ST20P/ST30P sessions.
     *
     * If absent, backend starts a one-port MTL runtime.
     */
    std::optional<MtlRuntimePortConfig> redundant_port{};
};

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_MTL_RUNTIME_CONFIG_HPP
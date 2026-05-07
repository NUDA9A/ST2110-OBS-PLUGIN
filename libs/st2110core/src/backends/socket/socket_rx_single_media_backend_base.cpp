#include "st2110/backends/socket/socket_rx_single_media_backend_base.hpp"
#include "st2110/backends/socket/platform/socket_stub_rx_port.hpp"

#if defined(__linux__)
#include "st2110/backends/socket/platform/linux_socket_rx_port.hpp"
#endif

namespace st2110 {
std::unique_ptr<ISocketRxPortFactory> SocketRxSingleMediaBackendBase::make_default_port_factory() {
#if defined(__linux__)
    return make_linux_socket_rx_port_factory();
#else
    return make_socket_stub_rx_port_factory();
#endif
}
}

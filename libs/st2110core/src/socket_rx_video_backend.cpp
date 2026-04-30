#include "st2110/socket_rx_video_backend.hpp"
#include "st2110/socket_stub_rx_port.hpp"

#if defined(__linux__)
#include "st2110/linux_socket_rx_port.hpp"
#endif

namespace st2110 {
std::unique_ptr<ISocketRxPortFactory> SocketRxVideoBackend::make_default_port_factory() {
#if defined(__linux__)
    return make_linux_socket_rx_port_factory();
#else
    return make_socket_stub_rx_port_factory();
#endif
}
} // namespace st2110
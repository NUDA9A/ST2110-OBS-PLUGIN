#include <cassert>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <st2110/backends/socket/platform/socket_runtime.hpp>
#include <st2110/foundation/error.hpp>
#include <st2110/backends/socket/platform/linux_socket_rx_port.hpp>

#if defined(__linux__)

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

static_assert(std::is_final_v<st2110::LinuxSocketRxPort>);
static_assert(std::is_base_of_v<st2110::ISocketRxPort, st2110::LinuxSocketRxPort>);
static_assert(std::is_final_v<st2110::LinuxSocketRxPortFactory>);
static_assert(std::is_base_of_v<st2110::ISocketRxPortFactory, st2110::LinuxSocketRxPortFactory>);

static_assert(std::is_same_v<decltype(std::declval<const st2110::LinuxSocketRxPort &>().is_open()), bool>);
static_assert(std::is_same_v<decltype(std::declval<st2110::LinuxSocketRxPort &>().open(
                                 std::declval<const st2110::SocketRxOpenConfig &>())),
                             st2110::Error>);
static_assert(std::is_same_v<decltype(std::declval<st2110::LinuxSocketRxPort &>().close()), st2110::Error>);
static_assert(std::is_same_v<decltype(std::declval<st2110::LinuxSocketRxPort &>().receive(
                                 std::declval<std::span<std::uint8_t>>())),
                             std::expected<st2110::SocketReceiveResult, st2110::Error>>);

namespace {
constexpr std::string_view kNonLocalIpv4Interface = "203.0.113.10";

class ReservedUdpSocket {
  public:
    ReservedUdpSocket() = default;

    ReservedUdpSocket(int fd, uint16_t port) : fd_(fd), port_(port) {}

    ReservedUdpSocket(const ReservedUdpSocket &) = delete;
    ReservedUdpSocket &operator=(const ReservedUdpSocket &) = delete;

    ReservedUdpSocket(ReservedUdpSocket &&other) noexcept : fd_(other.fd_), port_(other.port_) {
        other.fd_ = -1;
        other.port_ = 0;
    }

    ReservedUdpSocket &operator=(ReservedUdpSocket &&other) noexcept {
        if (this != &other) {
            close_now();
            fd_ = other.fd_;
            port_ = other.port_;
            other.fd_ = -1;
            other.port_ = 0;
        }
        return *this;
    }

    ~ReservedUdpSocket() { close_now(); }

    [[nodiscard]] bool valid() const noexcept { return fd_ >= 0; }

    [[nodiscard]] uint16_t port() const noexcept { return port_; }

    void close_now() noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
            port_ = 0;
        }
    }

  private:
    int fd_ = -1;
    uint16_t port_ = 0;
};

ReservedUdpSocket reserve_ipv4_loopback_udp_socket() {
    const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    assert(fd >= 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(0);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    const int bind_rc = ::bind(fd, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr));
    assert(bind_rc == 0);

    sockaddr_in bound{};
    socklen_t bound_len = sizeof(bound);
    const int name_rc = ::getsockname(fd, reinterpret_cast<sockaddr *>(&bound), &bound_len);
    assert(name_rc == 0);

    return ReservedUdpSocket(fd, ntohs(bound.sin_port));
}

bool host_supports_ipv6_loopback() {
    const int fd = ::socket(AF_INET6, SOCK_DGRAM, 0);
    if (fd < 0) {
        return false;
    }

    sockaddr_in6 addr{};
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(0);
    addr.sin6_addr = in6addr_loopback;

    const int bind_rc = ::bind(fd, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr));
    ::close(fd);
    return bind_rc == 0;
}

ReservedUdpSocket reserve_ipv6_loopback_udp_socket() {
    const int fd = ::socket(AF_INET6, SOCK_DGRAM, 0);
    assert(fd >= 0);

    sockaddr_in6 addr{};
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(0);
    addr.sin6_addr = in6addr_loopback;

    const int bind_rc = ::bind(fd, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr));
    assert(bind_rc == 0);

    sockaddr_in6 bound{};
    socklen_t bound_len = sizeof(bound);
    const int name_rc = ::getsockname(fd, reinterpret_cast<sockaddr *>(&bound), &bound_len);
    assert(name_rc == 0);

    return ReservedUdpSocket(fd, ntohs(bound.sin6_port));
}

void send_ipv4_loopback_datagram(uint16_t port, std::span<const std::uint8_t> payload) {
    const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    assert(fd >= 0);

    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(port);
    dst.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    const auto sent = ::sendto(fd,
                               payload.data(),
                               payload.size(),
                               0,
                               reinterpret_cast<const sockaddr *>(&dst),
                               sizeof(dst));

    assert(sent == static_cast<ssize_t>(payload.size()));
    ::close(fd);
}

st2110::SocketRxOpenConfig make_ipv4_unicast_open_config(uint16_t port) {
    st2110::SocketRxOpenConfig cfg{};
    cfg.bind_endpoint.family = st2110::SocketAddressFamily::IPv4;
    cfg.bind_endpoint.address = "127.0.0.1";
    cfg.bind_endpoint.port = port;
    cfg.reuse_address = true;
    return cfg;
}

st2110::SocketRxOpenConfig make_ipv6_unicast_open_config(uint16_t port) {
    st2110::SocketRxOpenConfig cfg{};
    cfg.bind_endpoint.family = st2110::SocketAddressFamily::IPv6;
    cfg.bind_endpoint.address = "::1";
    cfg.bind_endpoint.port = port;
    cfg.reuse_address = true;
    return cfg;
}

st2110::SocketRxOpenConfig make_ipv4_multicast_open_config(uint16_t port, std::string_view interface_address = {}) {
    st2110::SocketRxOpenConfig cfg{};
    cfg.bind_endpoint.family = st2110::SocketAddressFamily::IPv4;
    cfg.bind_endpoint.address = "0.0.0.0";
    cfg.bind_endpoint.port = port;
    cfg.multicast_membership = st2110::SocketMulticastMembership{
        .family = st2110::SocketAddressFamily::IPv4,
        .group_address = "239.1.2.3",
        .interface_address = std::string(interface_address)};
    cfg.reuse_address = true;
    return cfg;
}

st2110::SocketRxOpenConfig make_ipv6_multicast_open_config(uint16_t port) {
    st2110::SocketRxOpenConfig cfg{};
    cfg.bind_endpoint.family = st2110::SocketAddressFamily::IPv6;
    cfg.bind_endpoint.address = "::";
    cfg.bind_endpoint.port = port;
    cfg.multicast_membership = st2110::SocketMulticastMembership{
        .family = st2110::SocketAddressFamily::IPv6,
        .group_address = "ff15::abcd",
        .interface_address = ""};
    cfg.reuse_address = true;
    return cfg;
}

void test_linux_socket_rx_port_ipv4_open_close_and_repeated_open() {
    auto reserved = reserve_ipv4_loopback_udp_socket();
    const uint16_t port = reserved.port();
    reserved.close_now();

    st2110::LinuxSocketRxPort port_obj;
    const auto cfg = make_ipv4_unicast_open_config(port);

    assert(!port_obj.is_open());

    assert(port_obj.open(cfg) == st2110::Error::Ok);
    assert(port_obj.is_open());

    assert(port_obj.open(cfg) == st2110::Error::InvalidBackendState);
    assert(port_obj.is_open());

    assert(port_obj.close() == st2110::Error::Ok);
    assert(!port_obj.is_open());

    assert(port_obj.close() == st2110::Error::Ok);
    assert(!port_obj.is_open());
}

void test_linux_socket_rx_port_ipv6_open_close_when_supported() {
    if (!host_supports_ipv6_loopback()) {
        return;
    }

    auto reserved = reserve_ipv6_loopback_udp_socket();
    const uint16_t port = reserved.port();
    reserved.close_now();

    st2110::LinuxSocketRxPort port_obj;
    const auto cfg = make_ipv6_unicast_open_config(port);

    assert(!port_obj.is_open());
    assert(port_obj.open(cfg) == st2110::Error::Ok);
    assert(port_obj.is_open());
    assert(port_obj.close() == st2110::Error::Ok);
    assert(!port_obj.is_open());
}

void test_linux_socket_rx_port_ipv4_multicast_open_close_and_repeated_close() {
    auto reserved = reserve_ipv4_loopback_udp_socket();
    const uint16_t port = reserved.port();
    reserved.close_now();

    st2110::LinuxSocketRxPort port_obj;
    const auto cfg = make_ipv4_multicast_open_config(port);

    assert(!port_obj.is_open());
    assert(port_obj.open(cfg) == st2110::Error::Ok);
    assert(port_obj.is_open());

    assert(port_obj.close() == st2110::Error::Ok);
    assert(!port_obj.is_open());

    assert(port_obj.close() == st2110::Error::Ok);
    assert(!port_obj.is_open());
}

void test_linux_socket_rx_port_rejects_invalid_or_unsupported_open_requests() {
    st2110::LinuxSocketRxPort port_obj;

    st2110::SocketRxOpenConfig invalid_cfg{};
    invalid_cfg.bind_endpoint.family = st2110::SocketAddressFamily::IPv4;
    invalid_cfg.bind_endpoint.address = "127.0.0.1";
    invalid_cfg.bind_endpoint.port = 0;

    assert(port_obj.open(invalid_cfg) == st2110::Error::InvalidValue);
    assert(!port_obj.is_open());

    auto bad_interface_cfg = make_ipv4_multicast_open_config(5004, "not-an-ip");
    assert(port_obj.open(bad_interface_cfg) == st2110::Error::InvalidValue);
    assert(!port_obj.is_open());

    const auto multicast6_cfg = make_ipv6_multicast_open_config(5006);
    assert(port_obj.open(multicast6_cfg) == st2110::Error::Unsupported);
    assert(!port_obj.is_open());
}

void test_linux_socket_rx_port_maps_bind_failure() {
    auto reserved = reserve_ipv4_loopback_udp_socket();
    const uint16_t port = reserved.port();

    st2110::LinuxSocketRxPort port_obj;
    const auto cfg = make_ipv4_unicast_open_config(port);

    assert(port_obj.open(cfg) == st2110::Error::BindFailed);
    assert(!port_obj.is_open());
}

void test_linux_socket_rx_port_maps_multicast_join_failure_and_cleans_up_bound_socket() {
    auto reserved = reserve_ipv4_loopback_udp_socket();
    const uint16_t port = reserved.port();
    reserved.close_now();

    st2110::LinuxSocketRxPort port_obj;
    const auto bad_cfg = make_ipv4_multicast_open_config(port, kNonLocalIpv4Interface);

    assert(port_obj.open(bad_cfg) == st2110::Error::MulticastJoinFailed);
    assert(!port_obj.is_open());

    st2110::LinuxSocketRxPort retry_port;
    const auto retry_cfg = make_ipv4_unicast_open_config(port);

    assert(retry_port.open(retry_cfg) == st2110::Error::Ok);
    assert(retry_port.is_open());
    assert(retry_port.close() == st2110::Error::Ok);
}

void test_linux_socket_rx_port_receive_contract_without_parser_logic() {
    auto reserved = reserve_ipv4_loopback_udp_socket();
    const uint16_t port = reserved.port();
    reserved.close_now();

    st2110::LinuxSocketRxPort port_obj;
    const auto cfg = make_ipv4_unicast_open_config(port);

    std::vector<std::uint8_t> buffer(1500, 0);

    auto receive_before_open = port_obj.receive(buffer);
    assert(!receive_before_open.has_value());
    assert(receive_before_open.error() == st2110::Error::InvalidBackendState);

    assert(port_obj.open(cfg) == st2110::Error::Ok);

    std::span<std::uint8_t> empty_buffer{};
    auto receive_empty = port_obj.receive(empty_buffer);
    assert(!receive_empty.has_value());
    assert(receive_empty.error() == st2110::Error::InvalidValue);

    assert(port_obj.close() == st2110::Error::Ok);
}

void test_linux_socket_rx_port_receives_one_ipv4_udp_datagram() {
    auto reserved = reserve_ipv4_loopback_udp_socket();
    const uint16_t port = reserved.port();
    reserved.close_now();

    st2110::LinuxSocketRxPort port_obj;
    const auto cfg = make_ipv4_unicast_open_config(port);

    assert(port_obj.open(cfg) == st2110::Error::Ok);

    const std::vector<std::uint8_t> payload{0x10, 0x20, 0x30, 0x40, 0x50};
    send_ipv4_loopback_datagram(port, payload);

    std::vector<std::uint8_t> buffer(1500, 0);
    auto received = port_obj.receive(buffer);

    assert(received.has_value());
    assert(received->size_bytes == payload.size());

    for (std::size_t i = 0; i < payload.size(); ++i) {
        assert(buffer[i] == payload[i]);
    }

    assert(port_obj.close() == st2110::Error::Ok);
    assert(!port_obj.is_open());
}

void test_linux_socket_rx_port_factory_returns_closed_instances() {
    st2110::LinuxSocketRxPortFactory factory;

    auto port1 = factory.create_port();
    auto port2 = factory.create_port();

    assert(port1 != nullptr);
    assert(port2 != nullptr);
    assert(port1.get() != port2.get());
    assert(!port1->is_open());
    assert(!port2->is_open());

    auto factory_ptr = st2110::make_linux_socket_rx_port_factory();
    assert(factory_ptr != nullptr);

    auto port3 = factory_ptr->create_port();
    assert(port3 != nullptr);
    assert(!port3->is_open());
}
} // namespace

int main() {
    test_linux_socket_rx_port_ipv4_open_close_and_repeated_open();
    test_linux_socket_rx_port_ipv6_open_close_when_supported();
    test_linux_socket_rx_port_ipv4_multicast_open_close_and_repeated_close();
    test_linux_socket_rx_port_rejects_invalid_or_unsupported_open_requests();
    test_linux_socket_rx_port_maps_bind_failure();
    test_linux_socket_rx_port_maps_multicast_join_failure_and_cleans_up_bound_socket();
    test_linux_socket_rx_port_receive_contract_without_parser_logic();
    test_linux_socket_rx_port_receives_one_ipv4_udp_datagram();
    test_linux_socket_rx_port_factory_returns_closed_instances();
    return 0;
}

#else

int main() {
    return 0;
}

#endif
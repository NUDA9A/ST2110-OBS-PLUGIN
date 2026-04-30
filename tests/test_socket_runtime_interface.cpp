#include <array>
#include <cassert>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

#include <st2110/error.hpp>
#include <st2110/pixel_format.hpp>
#include <st2110/rx_config.hpp>
#include <st2110/socket_runtime.hpp>
#include <st2110/video_packing_mode.hpp>
#include <st2110/video_scan_mode.hpp>

static_assert(std::is_enum_v<st2110::SocketAddressFamily>);

static_assert(std::is_same_v<decltype(st2110::validate_socket_address_family(st2110::SocketAddressFamily::IPv4)),
                             st2110::Error>);

static_assert(std::is_same_v<decltype(st2110::socket_address_family_name(st2110::SocketAddressFamily::IPv4)),
                             std::string_view>);

static_assert(std::is_same_v<decltype(st2110::is_ipv4_multicast_address(std::declval<std::string_view>())),
                             bool>);

static_assert(std::is_same_v<decltype(st2110::is_ipv6_multicast_address(std::declval<std::string_view>())),
                             bool>);

static_assert(std::is_same_v<decltype(st2110::validate_socket_endpoint(
                                 std::declval<const st2110::SocketEndpoint&>())),
                             st2110::Error>);

static_assert(std::is_same_v<decltype(st2110::validate_socket_multicast_membership(
                                 std::declval<const st2110::SocketMulticastMembership&>())),
                             st2110::Error>);

static_assert(std::is_same_v<decltype(st2110::validate_socket_rx_open_config(
                                 std::declval<const st2110::SocketRxOpenConfig&>())),
                             st2110::Error>);

static_assert(std::is_same_v<decltype(st2110::socket_rx_uses_multicast(
                                 std::declval<const st2110::SocketRxOpenConfig&>())),
                             bool>);

static_assert(std::is_same_v<decltype(st2110::socket_rx_open_config_from_video_config(
                                 std::declval<const st2110::RxVideoConfig&>())),
                             std::expected<st2110::SocketRxOpenConfig, st2110::Error>>);

static_assert(std::is_same_v<decltype(st2110::SocketReceiveResult{}.size_bytes), std::size_t>);

static_assert(std::is_abstract_v<st2110::ISocketRxPort>);
static_assert(std::is_same_v<decltype(std::declval<const st2110::ISocketRxPort&>().is_open()), bool>);
static_assert(std::is_same_v<decltype(std::declval<st2110::ISocketRxPort&>().open(
                                 std::declval<const st2110::SocketRxOpenConfig&>())),
                             st2110::Error>);
static_assert(std::is_same_v<decltype(std::declval<st2110::ISocketRxPort&>().close()),
                             st2110::Error>);
static_assert(std::is_same_v<decltype(std::declval<st2110::ISocketRxPort&>().receive(
                                 std::declval<std::span<std::uint8_t>>())),
                             std::expected<st2110::SocketReceiveResult, st2110::Error>>);

static_assert(std::is_abstract_v<st2110::ISocketRxPortFactory>);
static_assert(std::is_same_v<decltype(std::declval<const st2110::ISocketRxPortFactory&>().create_port()),
                             std::unique_ptr<st2110::ISocketRxPort>>);

namespace
{
    class FakeSocketRxPort final : public st2110::ISocketRxPort
    {
    public:
        [[nodiscard]] bool is_open() const noexcept override
        {
            return open_;
        }

        st2110::Error open(const st2110::SocketRxOpenConfig& cfg) override
        {
            if (open_)
            {
                return st2110::Error::InvalidBackendState;
            }

            last_open_config = cfg;
            open_ = true;
            ++open_count;
            return st2110::Error::Ok;
        }

        st2110::Error close() override
        {
            ++close_count;
            open_ = false;
            return st2110::Error::Ok;
        }

        std::expected<st2110::SocketReceiveResult, st2110::Error> receive(
            std::span<std::uint8_t> buffer) override
        {
            if (!open_)
            {
                return std::unexpected(st2110::Error::InvalidBackendState);
            }

            if (buffer.empty())
            {
                return std::unexpected(st2110::Error::InvalidValue);
            }

            const std::size_t size_bytes = (buffer.size() >= 4u) ? 4u : buffer.size();
            for (std::size_t i = 0; i < size_bytes; ++i)
            {
                buffer[i] = static_cast<std::uint8_t>(0xA0u + i);
            }

            ++receive_count;
            return st2110::SocketReceiveResult{.size_bytes = size_bytes};
        }

        bool open_ = false;
        int open_count = 0;
        int close_count = 0;
        int receive_count = 0;
        st2110::SocketRxOpenConfig last_open_config{};
    };

    class FakeSocketRxPortFactory final : public st2110::ISocketRxPortFactory
    {
    public:
        [[nodiscard]] std::unique_ptr<st2110::ISocketRxPort> create_port() const override
        {
            ++create_count;
            return std::make_unique<FakeSocketRxPort>();
        }

        mutable int create_count = 0;
    };

    st2110::RxVideoConfig make_valid_ipv4_multicast_video_config()
    {
        st2110::RxVideoConfig cfg{};
        cfg.width = 1920;
        cfg.height = 1080;
        cfg.fps_num = 25;
        cfg.fps_den = 1;
        cfg.udp_port = 5004;
        cfg.payload_type = 96;
        cfg.local_ip = "";
        cfg.dest_ip = "239.10.20.30";
        cfg.format = st2110::PixelFormat::UYVY;
        cfg.scan_mode = st2110::VideoScanMode::Progressive;
        cfg.packing_mode = st2110::VideoPackingMode::Gpm;
        return cfg;
    }

    st2110::RxVideoConfig make_valid_ipv4_unicast_video_config()
    {
        st2110::RxVideoConfig cfg{};
        cfg.width = 1280;
        cfg.height = 720;
        cfg.fps_num = 50;
        cfg.fps_den = 1;
        cfg.udp_port = 5006;
        cfg.payload_type = 97;
        cfg.local_ip = "10.0.0.15";
        cfg.dest_ip = "10.0.0.50";
        cfg.format = st2110::PixelFormat::UYVY;
        cfg.scan_mode = st2110::VideoScanMode::Progressive;
        cfg.packing_mode = st2110::VideoPackingMode::Gpm;
        return cfg;
    }

    st2110::RxVideoConfig make_valid_ipv6_multicast_video_config()
    {
        st2110::RxVideoConfig cfg{};
        cfg.width = 1920;
        cfg.height = 1080;
        cfg.fps_num = 25;
        cfg.fps_den = 1;
        cfg.udp_port = 5008;
        cfg.payload_type = 98;
        cfg.local_ip = "";
        cfg.dest_ip = "ff15::abcd";
        cfg.format = st2110::PixelFormat::UYVY;
        cfg.scan_mode = st2110::VideoScanMode::Progressive;
        cfg.packing_mode = st2110::VideoPackingMode::Gpm;
        return cfg;
    }

    st2110::RxVideoConfig make_valid_ipv6_unicast_video_config()
    {
        st2110::RxVideoConfig cfg{};
        cfg.width = 3840;
        cfg.height = 2160;
        cfg.fps_num = 60;
        cfg.fps_den = 1;
        cfg.udp_port = 5010;
        cfg.payload_type = 99;
        cfg.local_ip = "2001:db8::15";
        cfg.dest_ip = "2001:db8::50";
        cfg.format = st2110::PixelFormat::UYVY;
        cfg.scan_mode = st2110::VideoScanMode::Progressive;
        cfg.packing_mode = st2110::VideoPackingMode::Gpm;
        return cfg;
    }

    void test_socket_address_family_helpers()
    {
        assert(st2110::validate_socket_address_family(st2110::SocketAddressFamily::IPv4) == st2110::Error::Ok);
        assert(st2110::validate_socket_address_family(st2110::SocketAddressFamily::IPv6) == st2110::Error::Ok);
        assert(st2110::validate_socket_address_family(static_cast<st2110::SocketAddressFamily>(255)) ==
               st2110::Error::InvalidValue);

        assert(st2110::socket_address_family_name(st2110::SocketAddressFamily::IPv4) == std::string_view{"ipv4"});
        assert(st2110::socket_address_family_name(st2110::SocketAddressFamily::IPv6) == std::string_view{"ipv6"});
        assert(st2110::socket_address_family_name(static_cast<st2110::SocketAddressFamily>(255)).empty());

        assert(st2110::is_ipv4_multicast_address("224.0.0.1"));
        assert(st2110::is_ipv4_multicast_address("239.255.255.255"));
        assert(!st2110::is_ipv4_multicast_address("223.255.255.255"));
        assert(!st2110::is_ipv4_multicast_address("240.0.0.1"));
        assert(!st2110::is_ipv4_multicast_address(""));
        assert(!st2110::is_ipv4_multicast_address("not-an-ip"));
        assert(!st2110::is_ipv4_multicast_address("ff15::abcd"));

        assert(st2110::is_ipv6_multicast_address("ff15::abcd"));
        assert(st2110::is_ipv6_multicast_address("FF3E::1234"));
        assert(!st2110::is_ipv6_multicast_address("2001:db8::1"));
        assert(!st2110::is_ipv6_multicast_address("239.1.2.3"));
        assert(!st2110::is_ipv6_multicast_address(""));
    }

    void test_socket_endpoint_and_membership_validation()
    {
        st2110::SocketEndpoint endpoint4{};
        endpoint4.family = st2110::SocketAddressFamily::IPv4;
        endpoint4.address = "0.0.0.0";
        endpoint4.port = 5004;
        assert(st2110::validate_socket_endpoint(endpoint4) == st2110::Error::Ok);

        st2110::SocketEndpoint endpoint6{};
        endpoint6.family = st2110::SocketAddressFamily::IPv6;
        endpoint6.address = "::";
        endpoint6.port = 5004;
        assert(st2110::validate_socket_endpoint(endpoint6) == st2110::Error::Ok);

        st2110::SocketEndpoint empty_address = endpoint4;
        empty_address.address.clear();
        assert(st2110::validate_socket_endpoint(empty_address) == st2110::Error::InvalidValue);

        st2110::SocketEndpoint bad_port = endpoint4;
        bad_port.port = 0;
        assert(st2110::validate_socket_endpoint(bad_port) == st2110::Error::InvalidValue);

        st2110::SocketEndpoint bad_family = endpoint4;
        bad_family.family = static_cast<st2110::SocketAddressFamily>(255);
        assert(st2110::validate_socket_endpoint(bad_family) == st2110::Error::InvalidValue);

        st2110::SocketMulticastMembership membership4{};
        membership4.family = st2110::SocketAddressFamily::IPv4;
        membership4.group_address = "239.1.2.3";
        membership4.interface_address = "";
        assert(st2110::validate_socket_multicast_membership(membership4) == st2110::Error::Ok);

        st2110::SocketMulticastMembership membership6{};
        membership6.family = st2110::SocketAddressFamily::IPv6;
        membership6.group_address = "ff15::abcd";
        membership6.interface_address = "";
        assert(st2110::validate_socket_multicast_membership(membership6) == st2110::Error::Ok);

        st2110::SocketMulticastMembership empty_group = membership4;
        empty_group.group_address.clear();
        assert(st2110::validate_socket_multicast_membership(empty_group) == st2110::Error::InvalidValue);

        st2110::SocketMulticastMembership unicast_group4 = membership4;
        unicast_group4.group_address = "10.0.0.20";
        assert(st2110::validate_socket_multicast_membership(unicast_group4) == st2110::Error::InvalidValue);

        st2110::SocketMulticastMembership unicast_group6 = membership6;
        unicast_group6.group_address = "2001:db8::20";
        assert(st2110::validate_socket_multicast_membership(unicast_group6) == st2110::Error::InvalidValue);
    }

    void test_socket_open_config_validation_and_multicast_flag()
    {
        st2110::SocketRxOpenConfig cfg4{};
        cfg4.bind_endpoint.family = st2110::SocketAddressFamily::IPv4;
        cfg4.bind_endpoint.address = "0.0.0.0";
        cfg4.bind_endpoint.port = 5004;
        cfg4.multicast_membership = st2110::SocketMulticastMembership{
            .family = st2110::SocketAddressFamily::IPv4,
            .group_address = "239.1.2.3",
            .interface_address = ""
        };
        cfg4.reuse_address = true;

        assert(st2110::validate_socket_rx_open_config(cfg4) == st2110::Error::Ok);
        assert(st2110::socket_rx_uses_multicast(cfg4));

        st2110::SocketRxOpenConfig cfg6{};
        cfg6.bind_endpoint.family = st2110::SocketAddressFamily::IPv6;
        cfg6.bind_endpoint.address = "::";
        cfg6.bind_endpoint.port = 5006;
        cfg6.multicast_membership = st2110::SocketMulticastMembership{
            .family = st2110::SocketAddressFamily::IPv6,
            .group_address = "ff15::abcd",
            .interface_address = ""
        };
        cfg6.reuse_address = true;

        assert(st2110::validate_socket_rx_open_config(cfg6) == st2110::Error::Ok);
        assert(st2110::socket_rx_uses_multicast(cfg6));

        st2110::SocketRxOpenConfig no_multicast = cfg4;
        no_multicast.multicast_membership.reset();
        assert(st2110::validate_socket_rx_open_config(no_multicast) == st2110::Error::Ok);
        assert(!st2110::socket_rx_uses_multicast(no_multicast));

        st2110::SocketRxOpenConfig mismatched_family = cfg4;
        mismatched_family.multicast_membership->family = st2110::SocketAddressFamily::IPv6;
        assert(st2110::validate_socket_rx_open_config(mismatched_family) == st2110::Error::InvalidValue);
    }

    void test_socket_open_config_projection_from_ipv4_video_config()
    {
        const auto multicast_cfg = make_valid_ipv4_multicast_video_config();
        auto multicast_open_cfg = st2110::socket_rx_open_config_from_video_config(multicast_cfg);
        assert(multicast_open_cfg.has_value());

        assert(multicast_open_cfg->bind_endpoint.family == st2110::SocketAddressFamily::IPv4);
        assert(multicast_open_cfg->bind_endpoint.address == std::string_view{"0.0.0.0"});
        assert(multicast_open_cfg->bind_endpoint.port == 5004);
        assert(multicast_open_cfg->reuse_address);
        assert(multicast_open_cfg->multicast_membership.has_value());
        assert(multicast_open_cfg->multicast_membership->family == st2110::SocketAddressFamily::IPv4);
        assert(multicast_open_cfg->multicast_membership->group_address == std::string_view{"239.10.20.30"});
        assert(multicast_open_cfg->multicast_membership->interface_address.empty());
        assert(st2110::socket_rx_uses_multicast(*multicast_open_cfg));

        const auto unicast_cfg = make_valid_ipv4_unicast_video_config();
        auto unicast_open_cfg = st2110::socket_rx_open_config_from_video_config(unicast_cfg);
        assert(unicast_open_cfg.has_value());

        assert(unicast_open_cfg->bind_endpoint.family == st2110::SocketAddressFamily::IPv4);
        assert(unicast_open_cfg->bind_endpoint.address == std::string_view{"10.0.0.15"});
        assert(unicast_open_cfg->bind_endpoint.port == 5006);
        assert(unicast_open_cfg->reuse_address);
        assert(!unicast_open_cfg->multicast_membership.has_value());
        assert(!st2110::socket_rx_uses_multicast(*unicast_open_cfg));
    }

    void test_socket_open_config_projection_from_ipv6_video_config()
    {
        const auto multicast_cfg = make_valid_ipv6_multicast_video_config();
        auto multicast_open_cfg = st2110::socket_rx_open_config_from_video_config(multicast_cfg);
        assert(multicast_open_cfg.has_value());

        assert(multicast_open_cfg->bind_endpoint.family == st2110::SocketAddressFamily::IPv6);
        assert(multicast_open_cfg->bind_endpoint.address == std::string_view{"::"});
        assert(multicast_open_cfg->bind_endpoint.port == 5008);
        assert(multicast_open_cfg->reuse_address);
        assert(multicast_open_cfg->multicast_membership.has_value());
        assert(multicast_open_cfg->multicast_membership->family == st2110::SocketAddressFamily::IPv6);
        assert(multicast_open_cfg->multicast_membership->group_address == std::string_view{"ff15::abcd"});
        assert(multicast_open_cfg->multicast_membership->interface_address.empty());
        assert(st2110::socket_rx_uses_multicast(*multicast_open_cfg));

        const auto unicast_cfg = make_valid_ipv6_unicast_video_config();
        auto unicast_open_cfg = st2110::socket_rx_open_config_from_video_config(unicast_cfg);
        assert(unicast_open_cfg.has_value());

        assert(unicast_open_cfg->bind_endpoint.family == st2110::SocketAddressFamily::IPv6);
        assert(unicast_open_cfg->bind_endpoint.address == std::string_view{"2001:db8::15"});
        assert(unicast_open_cfg->bind_endpoint.port == 5010);
        assert(unicast_open_cfg->reuse_address);
        assert(!unicast_open_cfg->multicast_membership.has_value());
        assert(!st2110::socket_rx_uses_multicast(*unicast_open_cfg));
    }

    void test_socket_open_config_projection_rejects_family_mismatch()
    {
        auto ipv4_local_ipv6_dest = make_valid_ipv4_unicast_video_config();
        ipv4_local_ipv6_dest.dest_ip = "2001:db8::10";

        auto mixed1 = st2110::socket_rx_open_config_from_video_config(ipv4_local_ipv6_dest);
        assert(!mixed1.has_value());
        assert(mixed1.error() == st2110::Error::InvalidValue);

        auto ipv6_local_ipv4_dest = make_valid_ipv6_unicast_video_config();
        ipv6_local_ipv4_dest.dest_ip = "239.1.2.3";

        auto mixed2 = st2110::socket_rx_open_config_from_video_config(ipv6_local_ipv4_dest);
        assert(!mixed2.has_value());
        assert(mixed2.error() == st2110::Error::InvalidValue);

        auto bad_dest = make_valid_ipv4_unicast_video_config();
        bad_dest.dest_ip = "not-an-ip";

        auto invalid = st2110::socket_rx_open_config_from_video_config(bad_dest);
        assert(!invalid.has_value());
        assert(invalid.error() == st2110::Error::InvalidValue);
    }

    void test_fake_socket_rx_port_lifecycle_and_receive_contract()
    {
        FakeSocketRxPort port;

        assert(!port.is_open());

        std::array<std::uint8_t, 8> buffer{};
        auto receive_before_open = port.receive(buffer);
        assert(!receive_before_open.has_value());
        assert(receive_before_open.error() == st2110::Error::InvalidBackendState);

        st2110::SocketRxOpenConfig cfg{};
        cfg.bind_endpoint.family = st2110::SocketAddressFamily::IPv6;
        cfg.bind_endpoint.address = "::";
        cfg.bind_endpoint.port = 5004;
        cfg.multicast_membership = st2110::SocketMulticastMembership{
            .family = st2110::SocketAddressFamily::IPv6,
            .group_address = "ff15::abcd",
            .interface_address = ""
        };

        assert(st2110::validate_socket_rx_open_config(cfg) == st2110::Error::Ok);
        assert(port.open(cfg) == st2110::Error::Ok);
        assert(port.is_open());
        assert(port.open_count == 1);
        assert(port.last_open_config.bind_endpoint.family == st2110::SocketAddressFamily::IPv6);
        assert(port.last_open_config.bind_endpoint.address == std::string_view{"::"});
        assert(port.last_open_config.multicast_membership.has_value());
        assert(port.last_open_config.multicast_membership->family == st2110::SocketAddressFamily::IPv6);

        assert(port.open(cfg) == st2110::Error::InvalidBackendState);
        assert(port.open_count == 1);

        auto receive_ok = port.receive(buffer);
        assert(receive_ok.has_value());
        assert(receive_ok->size_bytes == 4);
        assert(port.receive_count == 1);
        assert(buffer[0] == 0xA0);
        assert(buffer[1] == 0xA1);
        assert(buffer[2] == 0xA2);
        assert(buffer[3] == 0xA3);

        std::span<std::uint8_t> empty_buffer{};
        auto receive_empty = port.receive(empty_buffer);
        assert(!receive_empty.has_value());
        assert(receive_empty.error() == st2110::Error::InvalidValue);

        assert(port.close() == st2110::Error::Ok);
        assert(!port.is_open());
        assert(port.close_count == 1);

        assert(port.close() == st2110::Error::Ok);
        assert(!port.is_open());
        assert(port.close_count == 2);
    }

    void test_fake_socket_rx_port_factory_returns_port_instances()
    {
        FakeSocketRxPortFactory factory;

        auto port1 = factory.create_port();
        auto port2 = factory.create_port();

        assert(port1 != nullptr);
        assert(port2 != nullptr);
        assert(port1.get() != port2.get());
        assert(factory.create_count == 2);

        assert(!port1->is_open());
        assert(!port2->is_open());
    }
} // namespace

int main()
{
    test_socket_address_family_helpers();
    test_socket_endpoint_and_membership_validation();
    test_socket_open_config_validation_and_multicast_flag();
    test_socket_open_config_projection_from_ipv4_video_config();
    test_socket_open_config_projection_from_ipv6_video_config();
    test_socket_open_config_projection_rejects_family_mismatch();
    test_fake_socket_rx_port_lifecycle_and_receive_contract();
    test_fake_socket_rx_port_factory_returns_port_instances();
    return 0;
}
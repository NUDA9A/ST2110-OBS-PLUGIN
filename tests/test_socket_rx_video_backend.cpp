#include <cassert>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <st2110/backend.hpp>
#include <st2110/backend_factory.hpp>
#include <st2110/error.hpp>
#include <st2110/rx_config.hpp>
#include <st2110/socket_runtime.hpp>
#include <st2110/socket_rx_video_backend.hpp>
#include <st2110/video_frame.hpp>

#if defined(__linux__)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

static_assert(std::is_final_v<st2110::SocketRxVideoBackend>);
static_assert(std::is_base_of_v<st2110::IRxVideoBackend, st2110::SocketRxVideoBackend>);
static_assert(std::is_convertible_v<st2110::SocketRxVideoBackend *, st2110::IRxBackend *>);

static_assert(std::is_constructible_v<st2110::SocketRxVideoBackend>);
static_assert(std::is_constructible_v<st2110::SocketRxVideoBackend, std::unique_ptr<st2110::ISocketRxPortFactory>>);

static_assert(
    std::is_same_v<decltype(std::declval<const st2110::SocketRxVideoBackend &>().state()), st2110::RxBackendState>);

static_assert(
    std::is_same_v<decltype(std::declval<st2110::SocketRxVideoBackend &>().stop()), st2110::RxBackendLifecycleResult>);

static_assert(
    std::is_same_v<decltype(std::declval<st2110::SocketRxVideoBackend &>().start_video(
                       std::declval<const st2110::RxVideoConfig &>(), std::declval<st2110::IVideoFrameSink &>())),
                   st2110::RxBackendLifecycleResult>);

static_assert(std::is_final_v<st2110::SocketRxVideoBackendFactory>);
static_assert(std::is_base_of_v<st2110::IRxBackendFactory, st2110::SocketRxVideoBackendFactory>);

namespace {
class FakeVideoSink final : public st2110::IVideoFrameSink {
  public:
    void on_video_frame(const st2110::VideoFrameView &frame) override {
        called = true;
        last_width = frame.width;
        last_height = frame.height;
        last_timestamp_ns = frame.timestamp_ns;
    }

    bool called = false;
    uint32_t last_width = 0;
    uint32_t last_height = 0;
    st2110::TimestampNs last_timestamp_ns = 0;
};

struct FakeSocketRxPortState {
    bool is_open = false;
    bool return_null_port = false;
    int create_count = 0;
    int open_count = 0;
    int close_count = 0;
    std::optional<st2110::SocketRxOpenConfig> last_open_config{};
    st2110::Error open_error = st2110::Error::Ok;
    st2110::Error close_error = st2110::Error::Ok;
};

class FakeSocketRxPort final : public st2110::ISocketRxPort {
  public:
    explicit FakeSocketRxPort(std::shared_ptr<FakeSocketRxPortState> state) : state_(std::move(state)) {}

    [[nodiscard]] bool is_open() const noexcept override { return state_->is_open; }

    st2110::Error open(const st2110::SocketRxOpenConfig &cfg) override {
        if (state_->is_open) {
            return st2110::Error::InvalidBackendState;
        }

        ++state_->open_count;
        state_->last_open_config = cfg;

        if (state_->open_error != st2110::Error::Ok) {
            return state_->open_error;
        }

        state_->is_open = true;
        return st2110::Error::Ok;
    }

    st2110::Error close() override {
        ++state_->close_count;

        if (!state_->is_open) {
            return st2110::Error::Ok;
        }

        if (state_->close_error != st2110::Error::Ok) {
            return state_->close_error;
        }

        state_->is_open = false;
        state_->last_open_config.reset();
        return st2110::Error::Ok;
    }

    [[nodiscard]] std::expected<st2110::SocketReceiveResult, st2110::Error>
    receive(std::span<std::uint8_t> buffer) override {
        (void)buffer;
        return std::unexpected(st2110::Error::Unsupported);
    }

  private:
    std::shared_ptr<FakeSocketRxPortState> state_;
};

class FakeSocketRxPortFactory final : public st2110::ISocketRxPortFactory {
  public:
    explicit FakeSocketRxPortFactory(std::shared_ptr<FakeSocketRxPortState> state) : state_(std::move(state)) {}

    [[nodiscard]] std::unique_ptr<st2110::ISocketRxPort> create_port() const override {
        ++state_->create_count;

        if (state_->return_null_port) {
            return nullptr;
        }

        return std::make_unique<FakeSocketRxPort>(state_);
    }

  private:
    std::shared_ptr<FakeSocketRxPortState> state_;
};

#if defined(__linux__)
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
#endif

st2110::RxVideoConfig make_valid_ipv4_multicast_video_config() {
    st2110::RxVideoConfig cfg{};
    cfg.width = 1920;
    cfg.height = 1080;
    cfg.fps_num = 30;
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

st2110::RxVideoConfig make_valid_ipv6_multicast_video_config() {
    st2110::RxVideoConfig cfg{};
    cfg.width = 1920;
    cfg.height = 1080;
    cfg.fps_num = 30;
    cfg.fps_den = 1;
    cfg.udp_port = 5006;
    cfg.payload_type = 97;
    cfg.local_ip = "";
    cfg.dest_ip = "ff15::abcd";
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.scan_mode = st2110::VideoScanMode::Progressive;
    cfg.packing_mode = st2110::VideoPackingMode::Gpm;
    return cfg;
}

st2110::RxVideoConfig make_valid_ipv4_unicast_video_config() {
    st2110::RxVideoConfig cfg{};
    cfg.width = 1280;
    cfg.height = 720;
    cfg.fps_num = 50;
    cfg.fps_den = 1;
    cfg.udp_port = 5008;
    cfg.payload_type = 98;
    cfg.local_ip = "10.0.0.15";
    cfg.dest_ip = "10.0.0.50";
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.scan_mode = st2110::VideoScanMode::Progressive;
    cfg.packing_mode = st2110::VideoPackingMode::Gpm;
    return cfg;
}

void test_socket_rx_video_backend_uses_injected_socket_port_factory_for_ipv4_projection() {
    auto state = std::make_shared<FakeSocketRxPortState>();
    st2110::SocketRxVideoBackend backend(std::make_unique<FakeSocketRxPortFactory>(state));

    FakeVideoSink sink;
    const auto cfg = make_valid_ipv4_multicast_video_config();

    assert(st2110::backend_is_stopped(backend.state()));

    auto started = backend.start_video(cfg, sink);
    assert(started.has_value());
    assert(st2110::backend_media_active(*started, st2110::RxMediaKind::Video));
    assert(state->create_count == 1);
    assert(state->open_count == 1);
    assert(state->is_open);
    assert(state->last_open_config.has_value());
    assert(state->last_open_config->bind_endpoint.family == st2110::SocketAddressFamily::IPv4);
    assert(state->last_open_config->bind_endpoint.address == std::string_view{"0.0.0.0"});
    assert(state->last_open_config->bind_endpoint.port == 5004);
    assert(state->last_open_config->multicast_membership.has_value());
    assert(state->last_open_config->multicast_membership->family == st2110::SocketAddressFamily::IPv4);
    assert(state->last_open_config->multicast_membership->group_address == std::string_view{"239.10.20.30"});
    assert(!sink.called);

    auto started_again = backend.start_video(cfg, sink);
    assert(!started_again.has_value());
    assert(started_again.error() == st2110::Error::InvalidBackendState);
    assert(state->create_count == 1);
    assert(state->open_count == 1);

    auto stopped = backend.stop();
    assert(stopped.has_value());
    assert(st2110::backend_is_stopped(*stopped));
    assert(st2110::backend_is_stopped(backend.state()));
    assert(state->close_count == 1);
    assert(!state->is_open);
}

void test_socket_rx_video_backend_uses_injected_socket_port_factory_for_ipv6_projection() {
    auto state = std::make_shared<FakeSocketRxPortState>();
    st2110::SocketRxVideoBackend backend(std::make_unique<FakeSocketRxPortFactory>(state));

    FakeVideoSink sink;
    const auto cfg = make_valid_ipv6_multicast_video_config();

    auto started = backend.start_video(cfg, sink);
    assert(started.has_value());
    assert(st2110::backend_media_active(*started, st2110::RxMediaKind::Video));
    assert(state->create_count == 1);
    assert(state->open_count == 1);
    assert(state->last_open_config.has_value());
    assert(state->last_open_config->bind_endpoint.family == st2110::SocketAddressFamily::IPv6);
    assert(state->last_open_config->bind_endpoint.address == std::string_view{"::"});
    assert(state->last_open_config->bind_endpoint.port == 5006);
    assert(state->last_open_config->multicast_membership.has_value());
    assert(state->last_open_config->multicast_membership->family == st2110::SocketAddressFamily::IPv6);
    assert(state->last_open_config->multicast_membership->group_address == std::string_view{"ff15::abcd"});
    assert(!sink.called);

    auto stopped = backend.stop();
    assert(stopped.has_value());
    assert(st2110::backend_is_stopped(*stopped));
    assert(st2110::backend_is_stopped(backend.state()));
}

void test_socket_rx_video_backend_propagates_projection_failure_and_stays_stopped() {
    auto state = std::make_shared<FakeSocketRxPortState>();
    st2110::SocketRxVideoBackend backend(std::make_unique<FakeSocketRxPortFactory>(state));

    FakeVideoSink sink;
    auto cfg = make_valid_ipv4_unicast_video_config();
    cfg.dest_ip = "2001:db8::10";

    auto started = backend.start_video(cfg, sink);
    assert(!started.has_value());
    assert(started.error() == st2110::Error::InvalidValue);
    assert(st2110::backend_is_stopped(backend.state()));
    assert(state->create_count == 0);
    assert(state->open_count == 0);
    assert(!sink.called);
}

void test_socket_rx_video_backend_propagates_port_open_failure_and_allows_retry() {
    auto state = std::make_shared<FakeSocketRxPortState>();
    state->open_error = st2110::Error::BindFailed;

    st2110::SocketRxVideoBackend backend(std::make_unique<FakeSocketRxPortFactory>(state));

    FakeVideoSink sink;
    const auto cfg = make_valid_ipv4_multicast_video_config();

    auto first_start = backend.start_video(cfg, sink);
    assert(!first_start.has_value());
    assert(first_start.error() == st2110::Error::BindFailed);
    assert(st2110::backend_is_stopped(backend.state()));
    assert(state->create_count == 1);
    assert(state->open_count == 1);
    assert(!state->is_open);

    state->open_error = st2110::Error::Ok;

    auto second_start = backend.start_video(cfg, sink);
    assert(second_start.has_value());
    assert(st2110::backend_media_active(*second_start, st2110::RxMediaKind::Video));
    assert(state->create_count == 2);
    assert(state->open_count == 2);
    assert(state->is_open);

    auto stopped = backend.stop();
    assert(stopped.has_value());
    assert(st2110::backend_is_stopped(*stopped));
}

void test_socket_rx_video_backend_rejects_null_created_port() {
    auto state = std::make_shared<FakeSocketRxPortState>();
    state->return_null_port = true;

    st2110::SocketRxVideoBackend backend(std::make_unique<FakeSocketRxPortFactory>(state));

    FakeVideoSink sink;
    const auto cfg = make_valid_ipv4_multicast_video_config();

    auto started = backend.start_video(cfg, sink);
    assert(!started.has_value());
    assert(started.error() == st2110::Error::InvalidValue);
    assert(st2110::backend_is_stopped(backend.state()));
    assert(state->create_count == 1);
    assert(state->open_count == 0);
}

void test_socket_rx_video_backend_stop_propagates_close_failure_without_losing_active_state() {
    auto state = std::make_shared<FakeSocketRxPortState>();

    st2110::SocketRxVideoBackend backend(std::make_unique<FakeSocketRxPortFactory>(state));

    FakeVideoSink sink;
    const auto cfg = make_valid_ipv4_multicast_video_config();

    auto started = backend.start_video(cfg, sink);
    assert(started.has_value());
    assert(st2110::backend_media_active(backend.state(), st2110::RxMediaKind::Video));
    assert(state->is_open);

    state->close_error = st2110::Error::SystemFailure;

    auto failed_stop = backend.stop();
    assert(!failed_stop.has_value());
    assert(failed_stop.error() == st2110::Error::SystemFailure);
    assert(st2110::backend_media_active(backend.state(), st2110::RxMediaKind::Video));
    assert(state->is_open);
    assert(state->close_count == 1);

    state->close_error = st2110::Error::Ok;

    auto successful_stop = backend.stop();
    assert(successful_stop.has_value());
    assert(st2110::backend_is_stopped(*successful_stop));
    assert(st2110::backend_is_stopped(backend.state()));
    assert(!state->is_open);
    assert(state->close_count == 2);
}

void test_socket_rx_video_backend_can_restart_after_successful_stop() {
    auto state = std::make_shared<FakeSocketRxPortState>();

    st2110::SocketRxVideoBackend backend(std::make_unique<FakeSocketRxPortFactory>(state));

    FakeVideoSink sink;
    const auto cfg = make_valid_ipv4_multicast_video_config();

    auto first_start = backend.start_video(cfg, sink);
    assert(first_start.has_value());
    assert(st2110::backend_media_active(*first_start, st2110::RxMediaKind::Video));
    assert(state->create_count == 1);
    assert(state->open_count == 1);
    assert(state->is_open);

    auto first_stop = backend.stop();
    assert(first_stop.has_value());
    assert(st2110::backend_is_stopped(*first_stop));
    assert(st2110::backend_is_stopped(backend.state()));
    assert(state->close_count == 1);
    assert(!state->is_open);

    auto second_start = backend.start_video(cfg, sink);
    assert(second_start.has_value());
    assert(st2110::backend_media_active(*second_start, st2110::RxMediaKind::Video));
    assert(state->create_count == 2);
    assert(state->open_count == 2);
    assert(state->is_open);

    auto second_stop = backend.stop();
    assert(second_stop.has_value());
    assert(st2110::backend_is_stopped(*second_stop));
    assert(st2110::backend_is_stopped(backend.state()));
    assert(state->close_count == 2);
    assert(!state->is_open);
}

void test_socket_rx_video_backend_factory_descriptor_and_creation_shape() {
    st2110::SocketRxVideoBackendFactory factory;

    const auto descriptor = factory.descriptor();
    assert(descriptor.kind == st2110::RxBackendKind::Socket);
    assert(descriptor.name == std::string_view{"socket"});
    assert(descriptor.available);
    assert(st2110::supports_media(descriptor.capabilities, st2110::RxMediaKind::Video));
    assert(!st2110::supports_media(descriptor.capabilities, st2110::RxMediaKind::Audio));
    assert(st2110::validate_rx_backend_descriptor(descriptor) == st2110::Error::Ok);

    std::unique_ptr<st2110::IRxBackend> backend = factory.create_backend();
    assert(backend != nullptr);
    assert(std::string_view(backend->backend_name()) == "socket");
    assert(st2110::backend_is_stopped(backend->state()));

    auto *video_backend = dynamic_cast<st2110::IRxVideoBackend *>(backend.get());
    assert(video_backend != nullptr);
}

#if defined(__linux__)
void test_socket_rx_video_backend_default_backend_uses_linux_port_factory_on_linux() {
    auto reserved = reserve_ipv4_loopback_udp_socket();
    assert(reserved.valid());

    st2110::SocketRxVideoBackendFactory factory;
    std::unique_ptr<st2110::IRxBackend> backend = factory.create_backend();
    assert(backend != nullptr);

    auto *video_backend = dynamic_cast<st2110::IRxVideoBackend *>(backend.get());
    assert(video_backend != nullptr);

    FakeVideoSink sink;
    auto cfg = make_valid_ipv4_unicast_video_config();
    cfg.local_ip = "127.0.0.1";
    cfg.dest_ip = "127.0.0.1";
    cfg.udp_port = reserved.port();

    auto first_start = video_backend->start_video(cfg, sink);
    assert(!first_start.has_value());
    assert(first_start.error() == st2110::Error::BindFailed);
    assert(st2110::backend_is_stopped(backend->state()));
    assert(!sink.called);

    reserved.close_now();

    auto second_start = video_backend->start_video(cfg, sink);
    assert(second_start.has_value());
    assert(st2110::backend_media_active(*second_start, st2110::RxMediaKind::Video));
    assert(!sink.called);

    auto stopped = backend->stop();
    assert(stopped.has_value());
    assert(st2110::backend_is_stopped(*stopped));
    assert(st2110::backend_is_stopped(backend->state()));
}
#else
void test_socket_rx_video_backend_default_backend_uses_stub_factory_on_unsupported_build() {
    st2110::SocketRxVideoBackendFactory factory;
    std::unique_ptr<st2110::IRxBackend> backend = factory.create_backend();
    assert(backend != nullptr);

    auto *video_backend = dynamic_cast<st2110::IRxVideoBackend *>(backend.get());
    assert(video_backend != nullptr);

    FakeVideoSink sink;
    const auto cfg = make_valid_ipv4_multicast_video_config();

    auto started = video_backend->start_video(cfg, sink);
    assert(started.has_value());
    assert(st2110::backend_media_active(*started, st2110::RxMediaKind::Video));
    assert(!sink.called);

    auto stopped = backend->stop();
    assert(stopped.has_value());
    assert(st2110::backend_is_stopped(*stopped));
    assert(st2110::backend_is_stopped(backend->state()));
}
#endif
} // namespace

int main() {
    test_socket_rx_video_backend_uses_injected_socket_port_factory_for_ipv4_projection();
    test_socket_rx_video_backend_uses_injected_socket_port_factory_for_ipv6_projection();
    test_socket_rx_video_backend_propagates_projection_failure_and_stays_stopped();
    test_socket_rx_video_backend_propagates_port_open_failure_and_allows_retry();
    test_socket_rx_video_backend_rejects_null_created_port();
    test_socket_rx_video_backend_stop_propagates_close_failure_without_losing_active_state();
    test_socket_rx_video_backend_can_restart_after_successful_stop();
    test_socket_rx_video_backend_factory_descriptor_and_creation_shape();
    test_socket_rx_video_backend_default_backend_uses_linux_port_factory_on_linux();
    return 0;
}
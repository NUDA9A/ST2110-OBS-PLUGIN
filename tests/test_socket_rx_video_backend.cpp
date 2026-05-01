#include <array>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string_view>
#include <thread>
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
constexpr std::string_view kNonLocalIpv4Interface = "203.0.113.10";

struct CapturedVideoFrame {
    uint32_t width = 0;
    uint32_t height = 0;
    st2110::TimestampNs timestamp_ns = 0;
    std::vector<std::uint8_t> bytes{};
};

class FakeVideoSink final : public st2110::IVideoFrameSink {
  public:
    void on_video_frame(const st2110::VideoFrameView &frame) override {
        CapturedVideoFrame captured{};
        captured.width = frame.width;
        captured.height = frame.height;
        captured.timestamp_ns = frame.timestamp_ns;

        const std::size_t row_bytes = frame.width * 2u;
        captured.bytes.resize(row_bytes * frame.height);

        for (uint32_t row = 0; row < frame.height; ++row) {
            const auto *src = frame.data[0] + row * frame.stride[0];
            auto *dst = captured.bytes.data() + row * row_bytes;
            for (std::size_t i = 0; i < row_bytes; ++i) {
                dst[i] = src[i];
            }
        }

        {
            std::lock_guard lock(mutex_);
            frames_.push_back(std::move(captured));
        }
        cv_.notify_all();
    }

    [[nodiscard]] bool wait_for_frame_count(std::size_t count, std::chrono::milliseconds timeout) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [&] { return frames_.size() >= count; });
    }

    [[nodiscard]] std::size_t frame_count() const {
        std::lock_guard lock(mutex_);
        return frames_.size();
    }

    [[nodiscard]] CapturedVideoFrame frame_at(std::size_t index) const {
        std::lock_guard lock(mutex_);
        return frames_.at(index);
    }

  private:
    mutable std::mutex mutex_{};
    mutable std::condition_variable cv_{};
    std::vector<CapturedVideoFrame> frames_{};
};

struct FakeSocketRxPortState {
    mutable std::mutex mutex_{};
    std::condition_variable cv_{};

    bool is_open = false;
    bool return_null_port = false;

    int create_count = 0;
    int open_count = 0;
    int close_count = 0;
    int receive_count = 0;

    std::optional<st2110::SocketRxOpenConfig> last_open_config{};

    st2110::Error open_error = st2110::Error::Ok;
    st2110::Error close_error = st2110::Error::Ok;

    std::deque<std::vector<std::uint8_t>> queued_datagrams{};
};

class FakeSocketRxPort final : public st2110::ISocketRxPort {
  public:
    explicit FakeSocketRxPort(std::shared_ptr<FakeSocketRxPortState> state) : state_(std::move(state)) {}

    [[nodiscard]] bool is_open() const noexcept override {
        std::lock_guard lock(state_->mutex_);
        return state_->is_open;
    }

    st2110::Error open(const st2110::SocketRxOpenConfig &cfg) override {
        std::lock_guard lock(state_->mutex_);

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
        std::lock_guard lock(state_->mutex_);

        ++state_->close_count;

        if (!state_->is_open) {
            return st2110::Error::Ok;
        }

        if (state_->close_error != st2110::Error::Ok) {
            return state_->close_error;
        }

        state_->is_open = false;
        state_->last_open_config.reset();
        state_->cv_.notify_all();
        return st2110::Error::Ok;
    }

    [[nodiscard]] std::expected<st2110::SocketReceiveResult, st2110::Error>
    receive(std::span<std::uint8_t> buffer) override {
        std::unique_lock lock(state_->mutex_);

        for (;;) {
            if (!state_->is_open) {
                return std::unexpected(st2110::Error::ReceiveAborted);
            }

            if (!state_->queued_datagrams.empty()) {
                auto datagram = std::move(state_->queued_datagrams.front());
                state_->queued_datagrams.pop_front();
                ++state_->receive_count;
                lock.unlock();

                if (buffer.size() < datagram.size()) {
                    return std::unexpected(st2110::Error::InvalidValue);
                }

                for (std::size_t i = 0; i < datagram.size(); ++i) {
                    buffer[i] = datagram[i];
                }

                return st2110::SocketReceiveResult{datagram.size()};
            }

            state_->cv_.wait(lock);
        }
    }

  private:
    std::shared_ptr<FakeSocketRxPortState> state_;
};

class FakeSocketRxPortFactory final : public st2110::ISocketRxPortFactory {
  public:
    explicit FakeSocketRxPortFactory(std::shared_ptr<FakeSocketRxPortState> state) : state_(std::move(state)) {}

    [[nodiscard]] std::unique_ptr<st2110::ISocketRxPort> create_port() const override {
        std::lock_guard lock(state_->mutex_);
        ++state_->create_count;

        if (state_->return_null_port) {
            return nullptr;
        }

        return std::make_unique<FakeSocketRxPort>(state_);
    }

  private:
    std::shared_ptr<FakeSocketRxPortState> state_;
};

void enqueue_datagram(const std::shared_ptr<FakeSocketRxPortState> &state, std::vector<std::uint8_t> datagram) {
    {
        std::lock_guard lock(state->mutex_);
        state->queued_datagrams.push_back(std::move(datagram));
    }
    state->cv_.notify_all();
}

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

st2110::RxVideoConfig make_receive_ipv4_unicast_video_config(uint16_t port, uint32_t width = 4, uint8_t payload_type = 112) {
    st2110::RxVideoConfig cfg{};
    cfg.width = width;
    cfg.height = 1;
    cfg.fps_num = 25;
    cfg.fps_den = 1;
    cfg.udp_port = port;
    cfg.payload_type = payload_type;
    cfg.local_ip = "127.0.0.1";
    cfg.dest_ip = "127.0.0.1";
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.scan_mode = st2110::VideoScanMode::Progressive;
    cfg.packing_mode = st2110::VideoPackingMode::Gpm;
    return cfg;
}

std::vector<std::uint8_t> build_single_segment_video_datagram(uint8_t payload_type,
                                                              uint32_t extended_seq,
                                                              uint32_t rtp_timestamp,
                                                              std::span<const std::uint8_t> segment_bytes,
                                                              bool marker,
                                                              uint16_t row_number,
                                                              uint16_t offset) {
    assert(segment_bytes.size() <= 0xFFFFu);

    std::vector<std::uint8_t> res;
    res.reserve(20u + segment_bytes.size());

    const uint16_t seq16 = static_cast<uint16_t>(extended_seq & 0xFFFFu);
    const uint16_t ext_hi16 = static_cast<uint16_t>((extended_seq >> 16) & 0xFFFFu);
    const uint32_t ssrc = 0x11223344u;

    res.push_back(0x80u);
    res.push_back(static_cast<std::uint8_t>((marker ? 0x80u : 0x00u) | payload_type));

    res.push_back(static_cast<std::uint8_t>((seq16 >> 8) & 0xFFu));
    res.push_back(static_cast<std::uint8_t>(seq16 & 0xFFu));

    res.push_back(static_cast<std::uint8_t>((rtp_timestamp >> 24) & 0xFFu));
    res.push_back(static_cast<std::uint8_t>((rtp_timestamp >> 16) & 0xFFu));
    res.push_back(static_cast<std::uint8_t>((rtp_timestamp >> 8) & 0xFFu));
    res.push_back(static_cast<std::uint8_t>(rtp_timestamp & 0xFFu));

    res.push_back(static_cast<std::uint8_t>((ssrc >> 24) & 0xFFu));
    res.push_back(static_cast<std::uint8_t>((ssrc >> 16) & 0xFFu));
    res.push_back(static_cast<std::uint8_t>((ssrc >> 8) & 0xFFu));
    res.push_back(static_cast<std::uint8_t>(ssrc & 0xFFu));

    res.push_back(static_cast<std::uint8_t>((ext_hi16 >> 8) & 0xFFu));
    res.push_back(static_cast<std::uint8_t>(ext_hi16 & 0xFFu));

    const uint16_t length = static_cast<uint16_t>(segment_bytes.size());
    res.push_back(static_cast<std::uint8_t>((length >> 8) & 0xFFu));
    res.push_back(static_cast<std::uint8_t>(length & 0xFFu));

    res.push_back(static_cast<std::uint8_t>((row_number >> 8) & 0xFFu));
    res.push_back(static_cast<std::uint8_t>(row_number & 0xFFu));

    res.push_back(static_cast<std::uint8_t>((offset >> 8) & 0xFFu));
    res.push_back(static_cast<std::uint8_t>(offset & 0xFFu));

    for (std::uint8_t byte : segment_bytes) {
        res.push_back(byte);
    }

    return res;
}

std::vector<std::uint8_t> build_rtcp_like_datagram() {
    return {0x80u, 200u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u};
}

std::vector<std::uint8_t> build_short_malformed_media_datagram() {
    return {0x80u, 0xF0u, 0x00u};
}

void assert_bytes_equal(std::span<const std::uint8_t> actual, std::span<const std::uint8_t> expected) {
    assert(actual.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        assert(actual[i] == expected[i]);
    }
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

    {
        std::lock_guard lock(state->mutex_);
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
        assert(state->last_open_config->multicast_membership->interface_address.empty());
    }

    auto started_again = backend.start_video(cfg, sink);
    assert(!started_again.has_value());
    assert(started_again.error() == st2110::Error::InvalidBackendState);

    auto stopped = backend.stop();
    assert(stopped.has_value());
    assert(st2110::backend_is_stopped(*stopped));
    assert(st2110::backend_is_stopped(backend.state()));

    {
        std::lock_guard lock(state->mutex_);
        assert(state->close_count == 1);
        assert(!state->is_open);
    }
}

void test_socket_rx_video_backend_uses_injected_socket_port_factory_for_ipv4_multicast_interface_projection() {
    auto state = std::make_shared<FakeSocketRxPortState>();
    st2110::SocketRxVideoBackend backend(std::make_unique<FakeSocketRxPortFactory>(state));

    FakeVideoSink sink;
    auto cfg = make_valid_ipv4_multicast_video_config();
    cfg.local_ip = "127.0.0.1";

    auto started = backend.start_video(cfg, sink);
    assert(started.has_value());
    assert(st2110::backend_media_active(*started, st2110::RxMediaKind::Video));

    {
        std::lock_guard lock(state->mutex_);
        assert(state->create_count == 1);
        assert(state->open_count == 1);
        assert(state->last_open_config.has_value());
        assert(state->last_open_config->bind_endpoint.family == st2110::SocketAddressFamily::IPv4);
        assert(state->last_open_config->bind_endpoint.address == std::string_view{"0.0.0.0"});
        assert(state->last_open_config->bind_endpoint.port == 5004);
        assert(state->last_open_config->multicast_membership.has_value());
        assert(state->last_open_config->multicast_membership->family == st2110::SocketAddressFamily::IPv4);
        assert(state->last_open_config->multicast_membership->group_address == std::string_view{"239.10.20.30"});
        assert(state->last_open_config->multicast_membership->interface_address == std::string_view{"127.0.0.1"});
    }

    auto stopped = backend.stop();
    assert(stopped.has_value());
    assert(st2110::backend_is_stopped(*stopped));
}

void test_socket_rx_video_backend_uses_injected_socket_port_factory_for_ipv6_projection() {
    auto state = std::make_shared<FakeSocketRxPortState>();
    st2110::SocketRxVideoBackend backend(std::make_unique<FakeSocketRxPortFactory>(state));

    FakeVideoSink sink;
    const auto cfg = make_valid_ipv6_multicast_video_config();

    auto started = backend.start_video(cfg, sink);
    assert(started.has_value());
    assert(st2110::backend_media_active(*started, st2110::RxMediaKind::Video));

    {
        std::lock_guard lock(state->mutex_);
        assert(state->create_count == 1);
        assert(state->open_count == 1);
        assert(state->last_open_config.has_value());
        assert(state->last_open_config->bind_endpoint.family == st2110::SocketAddressFamily::IPv6);
        assert(state->last_open_config->bind_endpoint.address == std::string_view{"::"});
        assert(state->last_open_config->bind_endpoint.port == 5006);
        assert(state->last_open_config->multicast_membership.has_value());
        assert(state->last_open_config->multicast_membership->family == st2110::SocketAddressFamily::IPv6);
        assert(state->last_open_config->multicast_membership->group_address == std::string_view{"ff15::abcd"});
        assert(state->last_open_config->multicast_membership->interface_address.empty());
    }

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

    {
        std::lock_guard lock(state->mutex_);
        assert(state->create_count == 0);
        assert(state->open_count == 0);
    }
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

    {
        std::lock_guard lock(state->mutex_);
        assert(state->create_count == 1);
        assert(state->open_count == 1);
        assert(!state->is_open);
    }

    state->open_error = st2110::Error::Ok;

    auto second_start = backend.start_video(cfg, sink);
    assert(second_start.has_value());
    assert(st2110::backend_media_active(*second_start, st2110::RxMediaKind::Video));

    {
        std::lock_guard lock(state->mutex_);
        assert(state->create_count == 2);
        assert(state->open_count == 2);
        assert(state->is_open);
    }

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

    {
        std::lock_guard lock(state->mutex_);
        assert(state->create_count == 1);
        assert(state->open_count == 0);
    }
}

void test_socket_rx_video_backend_stop_propagates_close_failure_without_losing_active_state() {
    auto state = std::make_shared<FakeSocketRxPortState>();

    st2110::SocketRxVideoBackend backend(std::make_unique<FakeSocketRxPortFactory>(state));

    FakeVideoSink sink;
    const auto cfg = make_valid_ipv4_multicast_video_config();

    auto started = backend.start_video(cfg, sink);
    assert(started.has_value());
    assert(st2110::backend_media_active(backend.state(), st2110::RxMediaKind::Video));

    {
        std::lock_guard lock(state->mutex_);
        assert(state->is_open);
    }

    state->close_error = st2110::Error::SystemFailure;

    auto failed_stop = backend.stop();
    assert(!failed_stop.has_value());
    assert(failed_stop.error() == st2110::Error::SystemFailure);
    assert(st2110::backend_media_active(backend.state(), st2110::RxMediaKind::Video));

    {
        std::lock_guard lock(state->mutex_);
        assert(state->is_open);
        assert(state->close_count == 1);
    }

    state->close_error = st2110::Error::Ok;

    auto successful_stop = backend.stop();
    assert(successful_stop.has_value());
    assert(st2110::backend_is_stopped(*successful_stop));
    assert(st2110::backend_is_stopped(backend.state()));

    {
        std::lock_guard lock(state->mutex_);
        assert(!state->is_open);
        assert(state->close_count == 2);
    }
}

void test_socket_rx_video_backend_can_restart_after_successful_stop() {
    auto state = std::make_shared<FakeSocketRxPortState>();

    st2110::SocketRxVideoBackend backend(std::make_unique<FakeSocketRxPortFactory>(state));

    FakeVideoSink sink;
    const auto cfg = make_valid_ipv4_multicast_video_config();

    auto first_start = backend.start_video(cfg, sink);
    assert(first_start.has_value());
    assert(st2110::backend_media_active(*first_start, st2110::RxMediaKind::Video));

    {
        std::lock_guard lock(state->mutex_);
        assert(state->create_count == 1);
        assert(state->open_count == 1);
        assert(state->is_open);
    }

    auto first_stop = backend.stop();
    assert(first_stop.has_value());
    assert(st2110::backend_is_stopped(*first_stop));
    assert(st2110::backend_is_stopped(backend.state()));

    {
        std::lock_guard lock(state->mutex_);
        assert(state->close_count == 1);
        assert(!state->is_open);
    }

    auto second_start = backend.start_video(cfg, sink);
    assert(second_start.has_value());
    assert(st2110::backend_media_active(*second_start, st2110::RxMediaKind::Video));

    {
        std::lock_guard lock(state->mutex_);
        assert(state->create_count == 2);
        assert(state->open_count == 2);
        assert(state->is_open);
    }

    auto second_stop = backend.stop();
    assert(second_stop.has_value());
    assert(st2110::backend_is_stopped(*second_stop));
    assert(st2110::backend_is_stopped(backend.state()));

    {
        std::lock_guard lock(state->mutex_);
        assert(state->close_count == 2);
        assert(!state->is_open);
    }
}

void test_socket_rx_video_backend_delivers_reordered_video_frame_from_injected_port() {
    auto state = std::make_shared<FakeSocketRxPortState>();
    st2110::SocketRxVideoBackend backend(std::make_unique<FakeSocketRxPortFactory>(state));

    FakeVideoSink sink;
    const auto cfg = make_receive_ipv4_unicast_video_config(5012, 6, 112);

    auto started = backend.start_video(cfg, sink);
    assert(started.has_value());
    assert(st2110::backend_media_active(*started, st2110::RxMediaKind::Video));

    const std::array<std::uint8_t, 4> left{0, 1, 2, 3};
    const std::array<std::uint8_t, 4> middle{4, 5, 6, 7};
    const std::array<std::uint8_t, 4> right{8, 9, 10, 11};

    enqueue_datagram(state, build_single_segment_video_datagram(112, 1, 1000, left, false, 0, 0));
    enqueue_datagram(state, build_single_segment_video_datagram(112, 3, 1000, right, true, 0, 4));
    enqueue_datagram(state, build_single_segment_video_datagram(112, 2, 1000, middle, false, 0, 2));

    assert(sink.wait_for_frame_count(1, std::chrono::milliseconds(500)));
    const auto frame = sink.frame_at(0);

    assert(frame.width == 6);
    assert(frame.height == 1);
    assert(frame.timestamp_ns == 11111111ULL);

    const std::array<std::uint8_t, 12> expected{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    assert_bytes_equal(frame.bytes, expected);

    auto stopped = backend.stop();
    assert(stopped.has_value());
    assert(st2110::backend_is_stopped(*stopped));
}

void test_socket_rx_video_backend_ignores_control_admission_and_parse_failures_before_delivering_media() {
    auto state = std::make_shared<FakeSocketRxPortState>();
    st2110::SocketRxVideoBackend backend(std::make_unique<FakeSocketRxPortFactory>(state));

    FakeVideoSink sink;
    const auto cfg = make_receive_ipv4_unicast_video_config(5014, 4, 112);

    auto started = backend.start_video(cfg, sink);
    assert(started.has_value());

    enqueue_datagram(state, build_rtcp_like_datagram());
    enqueue_datagram(state, build_single_segment_video_datagram(113, 1, 1000, std::array<std::uint8_t, 8>{0, 1, 2, 3, 4, 5, 6, 7}, true, 0, 0));
    enqueue_datagram(state, build_short_malformed_media_datagram());

    const std::array<std::uint8_t, 8> good_bytes{10, 11, 12, 13, 14, 15, 16, 17};
    enqueue_datagram(state, build_single_segment_video_datagram(112, 2, 1000, good_bytes, true, 0, 0));

    assert(sink.wait_for_frame_count(1, std::chrono::milliseconds(500)));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    assert(sink.frame_count() == 1);

    const auto frame = sink.frame_at(0);
    assert(frame.width == 4);
    assert(frame.height == 1);
    assert_bytes_equal(frame.bytes, good_bytes);

    auto stopped = backend.stop();
    assert(stopped.has_value());
    assert(st2110::backend_is_stopped(*stopped));
}

void test_socket_rx_video_backend_maps_rtp_timestamps_before_sink_delivery() {
    auto state = std::make_shared<FakeSocketRxPortState>();
    st2110::SocketRxVideoBackend backend(std::make_unique<FakeSocketRxPortFactory>(state));

    FakeVideoSink sink;
    const auto cfg = make_receive_ipv4_unicast_video_config(5016, 4, 112);

    auto started = backend.start_video(cfg, sink);
    assert(started.has_value());

    const std::array<std::uint8_t, 8> frame0_bytes{0, 1, 2, 3, 4, 5, 6, 7};
    const std::array<std::uint8_t, 8> frame1_bytes{10, 11, 12, 13, 14, 15, 16, 17};

    enqueue_datagram(state, build_single_segment_video_datagram(112, 1, 0, frame0_bytes, true, 0, 0));
    enqueue_datagram(state, build_single_segment_video_datagram(112, 2, 90000, frame1_bytes, true, 0, 0));

    assert(sink.wait_for_frame_count(2, std::chrono::milliseconds(500)));

    const auto frame0 = sink.frame_at(0);
    const auto frame1 = sink.frame_at(1);

    assert(frame0.timestamp_ns == 0);
    assert(frame1.timestamp_ns == 1000000000ULL);
    assert_bytes_equal(frame0.bytes, frame0_bytes);
    assert_bytes_equal(frame1.bytes, frame1_bytes);

    auto stopped = backend.stop();
    assert(stopped.has_value());
    assert(st2110::backend_is_stopped(*stopped));
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
void test_socket_rx_video_backend_default_backend_uses_linux_port_factory_on_linux_bind_failure() {
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

    reserved.close_now();

    auto second_start = video_backend->start_video(cfg, sink);
    assert(second_start.has_value());
    assert(st2110::backend_media_active(*second_start, st2110::RxMediaKind::Video));

    auto stopped = backend->stop();
    assert(stopped.has_value());
    assert(st2110::backend_is_stopped(*stopped));
    assert(st2110::backend_is_stopped(backend->state()));
}

void test_socket_rx_video_backend_default_backend_recovers_after_multicast_join_failure_on_linux() {
    auto reserved = reserve_ipv4_loopback_udp_socket();
    assert(reserved.valid());
    const uint16_t port = reserved.port();
    reserved.close_now();

    st2110::SocketRxVideoBackendFactory factory;
    std::unique_ptr<st2110::IRxBackend> backend = factory.create_backend();
    assert(backend != nullptr);

    auto *video_backend = dynamic_cast<st2110::IRxVideoBackend *>(backend.get());
    assert(video_backend != nullptr);

    FakeVideoSink sink;

    auto bad_cfg = make_valid_ipv4_multicast_video_config();
    bad_cfg.udp_port = port;
    bad_cfg.local_ip = std::string(kNonLocalIpv4Interface);

    auto first_start = video_backend->start_video(bad_cfg, sink);
    assert(!first_start.has_value());
    assert(first_start.error() == st2110::Error::MulticastJoinFailed);
    assert(st2110::backend_is_stopped(backend->state()));

    auto retry_cfg = make_valid_ipv4_unicast_video_config();
    retry_cfg.udp_port = port;
    retry_cfg.local_ip = "127.0.0.1";
    retry_cfg.dest_ip = "127.0.0.1";

    auto second_start = video_backend->start_video(retry_cfg, sink);
    assert(second_start.has_value());
    assert(st2110::backend_media_active(*second_start, st2110::RxMediaKind::Video));

    auto stopped = backend->stop();
    assert(stopped.has_value());
    assert(st2110::backend_is_stopped(*stopped));
    assert(st2110::backend_is_stopped(backend->state()));
}

void test_socket_rx_video_backend_default_backend_receives_one_ipv4_unicast_frame_on_linux() {
    auto reserved = reserve_ipv4_loopback_udp_socket();
    assert(reserved.valid());
    const uint16_t port = reserved.port();
    reserved.close_now();

    st2110::SocketRxVideoBackendFactory factory;
    std::unique_ptr<st2110::IRxBackend> backend = factory.create_backend();
    assert(backend != nullptr);

    auto *video_backend = dynamic_cast<st2110::IRxVideoBackend *>(backend.get());
    assert(video_backend != nullptr);

    FakeVideoSink sink;
    const auto cfg = make_receive_ipv4_unicast_video_config(port, 4, 112);

    auto started = video_backend->start_video(cfg, sink);
    assert(started.has_value());
    assert(st2110::backend_media_active(*started, st2110::RxMediaKind::Video));

    const std::array<std::uint8_t, 8> frame_bytes{21, 22, 23, 24, 25, 26, 27, 28};
    const auto datagram = build_single_segment_video_datagram(112, 1, 0, frame_bytes, true, 0, 0);
    send_ipv4_loopback_datagram(port, datagram);

    assert(sink.wait_for_frame_count(1, std::chrono::milliseconds(500)));
    const auto frame = sink.frame_at(0);

    assert(frame.width == 4);
    assert(frame.height == 1);
    assert(frame.timestamp_ns == 0);
    assert_bytes_equal(frame.bytes, frame_bytes);

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

    auto stopped = backend->stop();
    assert(stopped.has_value());
    assert(st2110::backend_is_stopped(*stopped));
    assert(st2110::backend_is_stopped(backend->state()));
}
#endif
} // namespace

int main() {
    test_socket_rx_video_backend_uses_injected_socket_port_factory_for_ipv4_projection();
    test_socket_rx_video_backend_uses_injected_socket_port_factory_for_ipv4_multicast_interface_projection();
    test_socket_rx_video_backend_uses_injected_socket_port_factory_for_ipv6_projection();
    test_socket_rx_video_backend_propagates_projection_failure_and_stays_stopped();
    test_socket_rx_video_backend_propagates_port_open_failure_and_allows_retry();
    test_socket_rx_video_backend_rejects_null_created_port();
    test_socket_rx_video_backend_stop_propagates_close_failure_without_losing_active_state();
    test_socket_rx_video_backend_can_restart_after_successful_stop();
    test_socket_rx_video_backend_delivers_reordered_video_frame_from_injected_port();
    test_socket_rx_video_backend_ignores_control_admission_and_parse_failures_before_delivering_media();
    test_socket_rx_video_backend_maps_rtp_timestamps_before_sink_delivery();
    test_socket_rx_video_backend_factory_descriptor_and_creation_shape();
#if defined(__linux__)
    test_socket_rx_video_backend_default_backend_uses_linux_port_factory_on_linux_bind_failure();
    test_socket_rx_video_backend_default_backend_recovers_after_multicast_join_failure_on_linux();
    test_socket_rx_video_backend_default_backend_receives_one_ipv4_unicast_frame_on_linux();
#else
    test_socket_rx_video_backend_default_backend_uses_stub_factory_on_unsupported_build();
#endif
    return 0;
}
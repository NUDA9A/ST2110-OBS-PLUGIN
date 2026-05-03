#include <array>
#include <cassert>
#include <chrono>
#include <concepts>
#include <condition_variable>
#include <cstddef>
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
#include <st2110/packet_parse.hpp>
#include <st2110/rx_config.hpp>
#include <st2110/socket_runtime.hpp>
#include <st2110/socket_rx_video_backend.hpp>
#include <st2110/video_frame.hpp>

static_assert(std::is_final_v<st2110::SocketRxVideoBackend>);
static_assert(std::is_base_of_v<st2110::ISocketRxVideoBackend, st2110::SocketRxVideoBackend>);
static_assert(!std::is_base_of_v<st2110::IRxVideoBackend, st2110::SocketRxVideoBackend>);
static_assert(std::is_base_of_v<st2110::IRxBackend, st2110::SocketRxVideoBackend>);
static_assert(std::is_convertible_v<st2110::SocketRxVideoBackend*, st2110::IRxBackend*>);

static_assert(std::is_constructible_v<st2110::SocketRxVideoBackend>);
static_assert(std::is_constructible_v<st2110::SocketRxVideoBackend, std::unique_ptr<st2110::ISocketRxPortFactory>>);

static_assert(
    std::is_same_v<decltype(std::declval<const st2110::SocketRxVideoBackend&>().state()), st2110::RxBackendState>);
static_assert(
    std::is_same_v<decltype(std::declval<const st2110::SocketRxVideoBackend&>().stats()), st2110::BackendStats>);
static_assert(
    std::is_same_v<decltype(std::declval<st2110::SocketRxVideoBackend&>().stop()), st2110::RxBackendLifecycleResult>);

template <class T>
concept HasOperationalVideoStart =
    requires(T& backend, const st2110::SocketRxVideoOperationalConfig& cfg, st2110::IVideoFrameSink& sink) {
        { backend.start_video(cfg, sink) } -> std::same_as<st2110::RxBackendLifecycleResult>;
    };

template <class T>
concept HasManualVideoStart = requires(T& backend, const st2110::RxVideoConfig& cfg, st2110::IVideoFrameSink& sink) {
    { backend.start_video(cfg, sink) } -> std::same_as<st2110::RxBackendLifecycleResult>;
};

static_assert(HasOperationalVideoStart<st2110::SocketRxVideoBackend>);
static_assert(!HasManualVideoStart<st2110::SocketRxVideoBackend>);

static_assert(std::is_final_v<st2110::SocketRxVideoBackendFactory>);
static_assert(std::is_base_of_v<st2110::IRxBackendFactory, st2110::SocketRxVideoBackendFactory>);

namespace {
struct CapturedVideoFrame {
    uint32_t width = 0;
    uint32_t height = 0;
    st2110::TimestampNs timestamp_ns = 0;
    std::vector<std::uint8_t> bytes{};
};

class FakeVideoSink final : public st2110::IVideoFrameSink {
  public:
    void on_video_frame(const st2110::VideoFrameView& frame) override {
        CapturedVideoFrame captured{};
        captured.width = frame.width;
        captured.height = frame.height;
        captured.timestamp_ns = frame.timestamp_ns;

        const std::size_t row_bytes = static_cast<std::size_t>(frame.width) * 2U;
        captured.bytes.resize(row_bytes * frame.height);

        for (uint32_t row = 0; row < frame.height; ++row) {
            const auto* src = frame.data[0] + row * frame.stride[0];
            auto* dst = captured.bytes.data() + row * row_bytes;
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

    [[nodiscard]] bool wait_for_frame_count(std::size_t count, std::chrono::milliseconds timeout) const {
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

    st2110::Error open(const st2110::SocketRxOpenConfig& cfg) override {
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
        state_->cv_.notify_all();
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
                state_->cv_.notify_all();
                lock.unlock();

                if (datagram.size() > buffer.size()) {
                    return std::unexpected(st2110::Error::InvalidValue);
                }

                for (std::size_t i = 0; i < datagram.size(); ++i) {
                    buffer[i] = datagram[i];
                }

                return st2110::SocketReceiveResult{.size_bytes = datagram.size()};
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

template <class Predicate>
bool wait_until(Predicate&& predicate, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return predicate();
}

void enqueue_datagram(const std::shared_ptr<FakeSocketRxPortState>& state, std::vector<std::uint8_t> datagram) {
    {
        std::lock_guard lock(state->mutex_);
        state->queued_datagrams.push_back(std::move(datagram));
    }
    state->cv_.notify_all();
}

std::size_t current_receive_count(const std::shared_ptr<FakeSocketRxPortState>& state) {
    std::lock_guard lock(state->mutex_);
    return static_cast<std::size_t>(state->receive_count);
}

bool wait_for_receive_count_at_least(const std::shared_ptr<FakeSocketRxPortState>& state,
                                     std::size_t target,
                                     std::chrono::milliseconds timeout) {
    return wait_until([&] { return current_receive_count(state) >= target; }, timeout);
}

st2110::RxVideoConfig make_valid_video_multicast_rx_config() {
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

st2110::RxVideoConfig make_valid_video_unicast_rx_config() {
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

st2110::RxVideoConfig make_receive_video_rx_config() {
    st2110::RxVideoConfig cfg{};
    cfg.width = 4;
    cfg.height = 1;
    cfg.fps_num = 25;
    cfg.fps_den = 1;
    cfg.udp_port = 5012;
    cfg.payload_type = 112;
    cfg.local_ip = "127.0.0.1";
    cfg.dest_ip = "127.0.0.1";
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.scan_mode = st2110::VideoScanMode::Progressive;
    cfg.packing_mode = st2110::VideoPackingMode::Gpm;
    return cfg;
}

st2110::SocketRxVideoOperationalConfig
make_valid_video_operational_config(const st2110::RxVideoConfig& rx_cfg,
                                    const st2110::PacketParsePolicy& packet_parse_policy = {},
                                    st2110::PartialFramePolicy partial_frame_policy = st2110::PartialFramePolicy::Drop,
                                    st2110::VideoRtpTimestampMapperConfig timestamp_mapper_config = {}) {
    auto operational = st2110::socket_rx_video_operational_config_from_rx_video_config(
        rx_cfg, packet_parse_policy, partial_frame_policy, timestamp_mapper_config);
    assert(operational.has_value());
    return *operational;
}

std::vector<std::uint8_t> build_single_segment_video_datagram(std::uint8_t payload_type,
                                                              std::uint32_t extended_seq,
                                                              std::uint32_t rtp_timestamp,
                                                              std::span<const std::uint8_t> segment_bytes,
                                                              bool marker,
                                                              std::uint16_t row_number,
                                                              std::uint16_t offset) {
    assert(segment_bytes.size() <= 0xFFFFu);

    std::vector<std::uint8_t> res;
    res.reserve(20u + segment_bytes.size());

    const std::uint16_t seq16 = static_cast<std::uint16_t>(extended_seq & 0xFFFFu);
    const std::uint16_t ext_hi16 = static_cast<std::uint16_t>((extended_seq >> 16) & 0xFFFFu);
    const std::uint32_t ssrc = 0x11223344u;

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

    const std::uint16_t length = static_cast<std::uint16_t>(segment_bytes.size());
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

std::vector<std::uint8_t> build_large_version2_video_datagram(std::size_t total_bytes, std::uint8_t payload_type) {
    assert(total_bytes >= 12U);

    std::vector<std::uint8_t> datagram(total_bytes, 0x00u);
    datagram[0] = 0x80u;
    datagram[1] = payload_type;
    datagram[2] = 0x00u;
    datagram[3] = 0x01u;
    return datagram;
}

void assert_bytes_equal(std::span<const std::uint8_t> actual, std::span<const std::uint8_t> expected) {
    assert(actual.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        assert(actual[i] == expected[i]);
    }
}

void test_socket_rx_video_operational_validator_accepts_fully_consistent_config() {
    const auto rx_cfg = make_valid_video_multicast_rx_config();
    const auto cfg = make_valid_video_operational_config(rx_cfg);

    assert(st2110::validate_socket_rx_video_operational_config(cfg) == st2110::Error::Ok);
    assert(st2110::validate_rx_video_config(cfg.rx_config) == st2110::Error::Ok);
    assert(st2110::validate_socket_rx_operational_common_config(cfg.common) == st2110::Error::Ok);
}

void test_socket_rx_video_backend_stop_before_start_and_repeated_stop_are_ok() {
    auto state = std::make_shared<FakeSocketRxPortState>();
    st2110::SocketRxVideoBackend backend(std::make_unique<FakeSocketRxPortFactory>(state));

    assert(st2110::backend_is_stopped(backend.state()));

    auto first_stop = backend.stop();
    assert(first_stop.has_value());
    assert(st2110::backend_is_stopped(*first_stop));
    assert(st2110::backend_is_stopped(backend.state()));

    auto second_stop = backend.stop();
    assert(second_stop.has_value());
    assert(st2110::backend_is_stopped(*second_stop));
    assert(st2110::backend_is_stopped(backend.state()));

    {
        std::lock_guard lock(state->mutex_);
        assert(state->create_count == 0);
        assert(state->open_count == 0);
        assert(state->close_count == 0);
        assert(!state->is_open);
    }
}

void test_socket_rx_video_backend_accepts_operational_start_and_projects_open_config() {
    auto state = std::make_shared<FakeSocketRxPortState>();
    st2110::SocketRxVideoBackend backend(std::make_unique<FakeSocketRxPortFactory>(state));
    FakeVideoSink sink;

    const auto rx_cfg = make_valid_video_multicast_rx_config();
    const auto cfg = make_valid_video_operational_config(rx_cfg);

    auto started = backend.start_video(cfg, sink);
    assert(started.has_value());
    assert(st2110::backend_media_active(*started, st2110::RxMediaKind::Video));
    assert(!st2110::backend_media_active(*started, st2110::RxMediaKind::Audio));

    {
        std::lock_guard lock(state->mutex_);
        assert(state->create_count == 1);
        assert(state->open_count == 1);
        assert(state->is_open);
        assert(state->last_open_config.has_value());
        assert(st2110::socket_rx_open_config_equal(*state->last_open_config, cfg.common.open_config));
    }

    auto started_again = backend.start_video(cfg, sink);
    assert(!started_again.has_value());
    assert(started_again.error() == st2110::Error::InvalidBackendState);

    auto stopped = backend.stop();
    assert(stopped.has_value());
    assert(st2110::backend_is_stopped(*stopped));
    assert(st2110::backend_is_stopped(backend.state()));
}

void test_socket_rx_video_backend_rejects_mismatched_open_config_vs_rx_config() {
    auto state = std::make_shared<FakeSocketRxPortState>();
    st2110::SocketRxVideoBackend backend(std::make_unique<FakeSocketRxPortFactory>(state));
    FakeVideoSink sink;

    auto cfg = make_valid_video_operational_config(make_valid_video_unicast_rx_config());
    ++cfg.common.open_config.bind_endpoint.port;

    assert(st2110::validate_socket_rx_video_operational_config(cfg) == st2110::Error::InvalidValue);

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

void test_socket_rx_video_backend_rejects_mismatched_receive_pipeline_config_vs_rx_config() {
    auto state = std::make_shared<FakeSocketRxPortState>();
    st2110::SocketRxVideoBackend backend(std::make_unique<FakeSocketRxPortFactory>(state));
    FakeVideoSink sink;

    auto cfg = make_valid_video_operational_config(make_valid_video_unicast_rx_config());
    cfg.receive_pipeline_config.depacketizer.width += 2;

    assert(st2110::validate_socket_rx_video_operational_config(cfg) == st2110::Error::InvalidValue);

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

void test_socket_rx_video_backend_rejects_null_created_port() {
    auto state = std::make_shared<FakeSocketRxPortState>();
    state->return_null_port = true;

    st2110::SocketRxVideoBackend backend(std::make_unique<FakeSocketRxPortFactory>(state));
    FakeVideoSink sink;

    const auto cfg = make_valid_video_operational_config(make_valid_video_multicast_rx_config());

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

void test_socket_rx_video_backend_delivers_frame_from_operational_start() {
    auto state = std::make_shared<FakeSocketRxPortState>();
    st2110::SocketRxVideoBackend backend(std::make_unique<FakeSocketRxPortFactory>(state));
    FakeVideoSink sink;

    auto cfg = make_valid_video_operational_config(make_receive_video_rx_config());

    auto started = backend.start_video(cfg, sink);
    assert(started.has_value());
    assert(st2110::backend_media_active(*started, st2110::RxMediaKind::Video));

    const std::array<std::uint8_t, 8> row_bytes = {0x10u, 0x20u, 0x30u, 0x40u, 0x50u, 0x60u, 0x70u, 0x80u};
    enqueue_datagram(state, build_single_segment_video_datagram(cfg.rx_config.payload_type,
                                                                1U,
                                                                90'000U,
                                                                row_bytes,
                                                                true,
                                                                0U,
                                                                0U));

    assert(wait_for_receive_count_at_least(state, 1U, std::chrono::milliseconds(500)));
    assert(sink.wait_for_frame_count(1U, std::chrono::milliseconds(500)));

    const auto frame = sink.frame_at(0);
    assert(frame.width == 4U);
    assert(frame.height == 1U);
    assert(frame.timestamp_ns == 1'000'000'000ULL);
    assert_bytes_equal(frame.bytes, row_bytes);

    const auto stats = backend.stats();
    assert(stats.datagrams_received == 1U);
    assert(stats.packets_parsed_ok == 1U);
    assert(stats.frames_delivered == 1U);
    assert(stats.media_units_delivered == 1U);
    assert(stats.packets_rejected == 0U);

    auto stopped = backend.stop();
    assert(stopped.has_value());
    assert(st2110::backend_is_stopped(*stopped));
}

void test_socket_rx_video_backend_extended_packet_policy_is_used_by_receive_runtime() {
    auto state = std::make_shared<FakeSocketRxPortState>();
    st2110::SocketRxVideoBackend backend(std::make_unique<FakeSocketRxPortFactory>(state));
    FakeVideoSink sink;

    st2110::PacketParsePolicy policy{};
    policy.max_udp_datagram_bytes = st2110::extendedUdpDatagramSizeLimitBytes;

    auto cfg = make_valid_video_operational_config(make_receive_video_rx_config(), policy);

    auto started = backend.start_video(cfg, sink);
    assert(started.has_value());

    enqueue_datagram(state, build_large_version2_video_datagram(2'000U, cfg.rx_config.payload_type));

    assert(wait_for_receive_count_at_least(state, 1U, std::chrono::milliseconds(500)));
    assert(wait_until(
        [&] {
            const auto stats = backend.stats();
            return stats.datagrams_received >= 1U && stats.packets_rejected >= 1U;
        },
        std::chrono::milliseconds(500)));

    const auto stats = backend.stats();
    assert(stats.datagrams_received == 1U);
    assert(stats.frames_delivered == 0U);
    assert(stats.media_units_delivered == 0U);
    assert(stats.packets_rejected == 1U);
    assert(stats.packet_parse.packet_policy_fail == 0U);

    auto stopped = backend.stop();
    assert(stopped.has_value());
    assert(st2110::backend_is_stopped(*stopped));
}

} // namespace

int main() {
    test_socket_rx_video_operational_validator_accepts_fully_consistent_config();
    test_socket_rx_video_backend_stop_before_start_and_repeated_stop_are_ok();
    test_socket_rx_video_backend_accepts_operational_start_and_projects_open_config();
    test_socket_rx_video_backend_rejects_mismatched_open_config_vs_rx_config();
    test_socket_rx_video_backend_rejects_mismatched_receive_pipeline_config_vs_rx_config();
    test_socket_rx_video_backend_rejects_null_created_port();
    test_socket_rx_video_backend_delivers_frame_from_operational_start();
    test_socket_rx_video_backend_extended_packet_policy_is_used_by_receive_runtime();
    return 0;
}
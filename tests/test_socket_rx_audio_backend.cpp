#include <algorithm>
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

#include <st2110/audio_channel_order.hpp>
#include <st2110/audio_frame.hpp>
#include <st2110/audio_packet.hpp>
#include <st2110/backend.hpp>
#include <st2110/backend_factory.hpp>
#include <st2110/error.hpp>
#include <st2110/packet_parse.hpp>
#include <st2110/rx_config.hpp>
#include <st2110/socket_runtime.hpp>
#include <st2110/socket_rx_audio_backend.hpp>

static_assert(std::is_final_v<st2110::SocketRxAudioBackend>);
static_assert(std::is_base_of_v<st2110::ISocketRxAudioBackend, st2110::SocketRxAudioBackend>);
static_assert(!std::is_base_of_v<st2110::IRxAudioBackend, st2110::SocketRxAudioBackend>);
static_assert(std::is_base_of_v<st2110::IRxBackend, st2110::SocketRxAudioBackend>);
static_assert(std::is_convertible_v<st2110::SocketRxAudioBackend*, st2110::IRxBackend*>);

static_assert(std::is_constructible_v<st2110::SocketRxAudioBackend>);
static_assert(std::is_constructible_v<st2110::SocketRxAudioBackend, std::unique_ptr<st2110::ISocketRxPortFactory>>);

static_assert(
    std::is_same_v<decltype(std::declval<const st2110::SocketRxAudioBackend&>().state()), st2110::RxBackendState>);
static_assert(
    std::is_same_v<decltype(std::declval<const st2110::SocketRxAudioBackend&>().stats()), st2110::BackendStats>);
static_assert(
    std::is_same_v<decltype(std::declval<st2110::SocketRxAudioBackend&>().stop()), st2110::RxBackendLifecycleResult>);

template <class T>
concept HasOperationalAudioStart =
    requires(T& backend, const st2110::SocketRxAudioOperationalConfig& cfg, st2110::IAudioFrameSink& sink) {
        { backend.start_audio(cfg, sink) } -> std::same_as<st2110::RxBackendLifecycleResult>;
    };

template <class T>
concept HasManualAudioStart = requires(T& backend, const st2110::RxAudioConfig& cfg, st2110::IAudioFrameSink& sink) {
    { backend.start_audio(cfg, sink) } -> std::same_as<st2110::RxBackendLifecycleResult>;
};

static_assert(HasOperationalAudioStart<st2110::SocketRxAudioBackend>);
static_assert(!HasManualAudioStart<st2110::SocketRxAudioBackend>);

static_assert(std::is_final_v<st2110::SocketRxAudioBackendFactory>);
static_assert(std::is_base_of_v<st2110::IRxBackendFactory, st2110::SocketRxAudioBackendFactory>);

namespace {
constexpr std::uint8_t kDefaultPayloadType = 111U;
constexpr std::uint32_t kDefaultSamplingRateHz = 48'000U;
constexpr std::uint32_t kDefaultPacketTimeUs = 1'000U;
constexpr std::uint32_t kDefaultSamplesPerPacket = 48U;
constexpr std::uint16_t kDefaultChannelCount = 2U;
constexpr std::size_t kL24BytesPerSample = 3U;

class DummyAudioSink final : public st2110::IAudioFrameSink {
  public:
    void on_audio_frame(const st2110::AudioFrameView& frame) override {
        (void)frame;
        ++call_count;
    }

    int call_count = 0;
};

struct CapturedAudioFrame {
    st2110::TimestampNs timestamp_ns = 0;
    std::uint32_t sampling_rate_hz = 0;
    std::uint16_t channel_count = 0;
    std::uint32_t samples_per_channel = 0;
    std::size_t total_sample_count = 0;
    std::size_t sample_frame_stride = 0;
    std::size_t size_bytes = 0;
    std::vector<std::int32_t> samples{};
};

class CapturingAudioSink final : public st2110::IAudioFrameSink {
  public:
    void on_audio_frame(const st2110::AudioFrameView& frame) override {
        CapturedAudioFrame captured{};
        captured.timestamp_ns = frame.timestamp_ns;
        captured.sampling_rate_hz = frame.sampling_rate_hz;
        captured.channel_count = frame.channel_count;
        captured.samples_per_channel = frame.samples_per_channel;
        captured.total_sample_count = frame.total_sample_count;
        captured.sample_frame_stride = frame.sample_frame_stride;
        captured.size_bytes = frame.size_bytes;
        captured.samples.assign(frame.samples, frame.samples + frame.total_sample_count);

        {
            std::lock_guard lock(mutex_);
            frames_.push_back(std::move(captured));
        }
        cv_.notify_all();
    }

    [[nodiscard]] bool wait_for_frame_count(std::size_t expected, std::chrono::milliseconds timeout) const {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [&] { return frames_.size() >= expected; });
    }

    [[nodiscard]] CapturedAudioFrame frame_at(std::size_t index) const {
        std::lock_guard lock(mutex_);
        return frames_.at(index);
    }

  private:
    mutable std::mutex mutex_{};
    mutable std::condition_variable cv_{};
    std::vector<CapturedAudioFrame> frames_{};
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

st2110::ParsedAudioChannelOrder make_stereo_channel_order() {
    auto parsed = st2110::parse_smpte2110_audio_channel_order_raw_value("SMPTE2110.(ST)");
    assert(parsed.has_value());
    return *parsed;
}

st2110::RxAudioConfig make_valid_audio_multicast_rx_config() {
    st2110::RxAudioConfig cfg{};
    cfg.sampling_rate_hz = kDefaultSamplingRateHz;
    cfg.packet_time_us = kDefaultPacketTimeUs;
    cfg.samples_per_packet = kDefaultSamplesPerPacket;
    cfg.channel_count = kDefaultChannelCount;
    cfg.udp_port = 5004;
    cfg.payload_type = 96;
    cfg.local_ip = "";
    cfg.dest_ip = "239.10.20.30";
    cfg.format = st2110::AudioSampleFormat::LinearPcm;
    cfg.pcm_bit_depth = st2110::AudioPcmBitDepth::Bits24;
    return cfg;
}

st2110::RxAudioConfig make_valid_audio_unicast_rx_config() {
    st2110::RxAudioConfig cfg{};
    cfg.sampling_rate_hz = kDefaultSamplingRateHz;
    cfg.packet_time_us = kDefaultPacketTimeUs;
    cfg.samples_per_packet = kDefaultSamplesPerPacket;
    cfg.channel_count = kDefaultChannelCount;
    cfg.udp_port = 5008;
    cfg.payload_type = 98;
    cfg.local_ip = "10.0.0.15";
    cfg.dest_ip = "10.0.0.50";
    cfg.format = st2110::AudioSampleFormat::LinearPcm;
    cfg.pcm_bit_depth = st2110::AudioPcmBitDepth::Bits24;
    return cfg;
}

st2110::RxAudioConfig make_receive_audio_rx_config() {
    st2110::RxAudioConfig cfg{};
    cfg.sampling_rate_hz = kDefaultSamplingRateHz;
    cfg.packet_time_us = kDefaultPacketTimeUs;
    cfg.samples_per_packet = kDefaultSamplesPerPacket;
    cfg.channel_count = kDefaultChannelCount;
    cfg.udp_port = 5016;
    cfg.payload_type = kDefaultPayloadType;
    cfg.local_ip = "127.0.0.1";
    cfg.dest_ip = "127.0.0.1";
    cfg.format = st2110::AudioSampleFormat::LinearPcm;
    cfg.pcm_bit_depth = st2110::AudioPcmBitDepth::Bits24;
    return cfg;
}

st2110::AudioRtpTimestampMapperConfig
make_configured_reference_audio_timestamp_mapper_config(std::uint32_t rtp_clock_rate,
                                                        st2110::TimestampNs anchor_timestamp_ns = 0,
                                                        std::uint32_t anchor_rtp_timestamp = 0) {
    return st2110::AudioRtpTimestampMapperConfig{
        .rtp_clock_rate = rtp_clock_rate,
        .initial_anchor_mode = st2110::RtpTimestampInitialAnchorMode::ConfiguredReference,
        .anchor_rtp_timestamp = anchor_rtp_timestamp,
        .anchor_timestamp_ns = anchor_timestamp_ns,
    };
}

st2110::SocketRxAudioOperationalConfig
make_valid_audio_operational_config(const st2110::RxAudioConfig& rx_cfg,
                                    const st2110::PacketParsePolicy& packet_parse_policy = {},
                                    const st2110::AudioFrameAssemblerConfig& frame_assembler_config =
                                        st2110::AudioFrameAssemblerConfig{},
                                    const st2110::AudioReorderBufferConfig& reorder_buffer_config =
                                        st2110::AudioReorderBufferConfig{},
                                    const st2110::AudioRtpTimestampMapperConfig& timestamp_mapper_config = {},
                                    const st2110::ParsedAudioChannelOrder& channel_order = make_stereo_channel_order()) {
    st2110::AudioRtpTimestampMapperConfig effective_timestamp_mapper_config = timestamp_mapper_config;
    if (effective_timestamp_mapper_config.rtp_clock_rate == 0) {
        effective_timestamp_mapper_config = st2110::AudioRtpTimestampMapperConfig{
            .rtp_clock_rate = rx_cfg.sampling_rate_hz,
            .initial_anchor_mode = st2110::RtpTimestampInitialAnchorMode::FirstObservedBecomesLocalZero,
            .anchor_rtp_timestamp = 0,
            .anchor_timestamp_ns = 0,
        };
    }

    auto operational = st2110::socket_rx_audio_operational_config_from_rx_audio_config(
        rx_cfg,
        packet_parse_policy,
        frame_assembler_config,
        reorder_buffer_config,
        effective_timestamp_mapper_config,
        channel_order);
    assert(operational.has_value());
    return *operational;
}

std::vector<std::uint8_t> make_l24_audio_payload(std::uint32_t samples_per_channel,
                                                 std::uint16_t channel_count,
                                                 std::int32_t first_sample_value) {
    const std::size_t total_samples =
        static_cast<std::size_t>(samples_per_channel) * static_cast<std::size_t>(channel_count);

    std::vector<std::uint8_t> payload(total_samples * kL24BytesPerSample);

    for (std::size_t i = 0; i < total_samples; ++i) {
        const std::int32_t value = first_sample_value + static_cast<std::int32_t>(i);
        assert(value >= -0x800000 && value <= 0x7fffff);

        const std::uint32_t raw =
            value < 0 ? static_cast<std::uint32_t>(value + 0x1000000) : static_cast<std::uint32_t>(value);

        payload[i * 3 + 0] = static_cast<std::uint8_t>((raw >> 16) & 0xffu);
        payload[i * 3 + 1] = static_cast<std::uint8_t>((raw >> 8) & 0xffu);
        payload[i * 3 + 2] = static_cast<std::uint8_t>(raw & 0xffu);
    }

    return payload;
}

std::vector<std::uint8_t> make_audio_rtp_datagram(std::uint16_t seq_number,
                                                  std::uint32_t timestamp,
                                                  std::uint8_t payload_type,
                                                  std::int32_t first_sample_value,
                                                  bool marker = true) {
    auto payload = make_l24_audio_payload(kDefaultSamplesPerPacket, kDefaultChannelCount, first_sample_value);

    std::vector<std::uint8_t> datagram(12U + payload.size(), 0U);
    datagram[0] = 0x80U;
    datagram[1] = static_cast<std::uint8_t>((marker ? 0x80U : 0x00U) | (payload_type & 0x7FU));

    datagram[2] = static_cast<std::uint8_t>((seq_number >> 8) & 0xFFU);
    datagram[3] = static_cast<std::uint8_t>(seq_number & 0xFFU);

    datagram[4] = static_cast<std::uint8_t>((timestamp >> 24) & 0xFFU);
    datagram[5] = static_cast<std::uint8_t>((timestamp >> 16) & 0xFFU);
    datagram[6] = static_cast<std::uint8_t>((timestamp >> 8) & 0xFFU);
    datagram[7] = static_cast<std::uint8_t>(timestamp & 0xFFU);

    const std::uint32_t ssrc = 0x11223344U;
    datagram[8] = static_cast<std::uint8_t>((ssrc >> 24) & 0xFFU);
    datagram[9] = static_cast<std::uint8_t>((ssrc >> 16) & 0xFFU);
    datagram[10] = static_cast<std::uint8_t>((ssrc >> 8) & 0xFFU);
    datagram[11] = static_cast<std::uint8_t>(ssrc & 0xFFU);

    std::copy(payload.begin(), payload.end(), datagram.begin() + 12U);
    return datagram;
}

std::vector<std::uint8_t> make_large_version2_audio_datagram(std::size_t total_bytes, std::uint8_t payload_type) {
    assert(total_bytes >= 12U);

    std::vector<std::uint8_t> datagram(total_bytes, 0x00u);
    datagram[0] = 0x80u;
    datagram[1] = payload_type;
    datagram[2] = 0x00u;
    datagram[3] = 0x01u;
    return datagram;
}

void test_socket_rx_audio_operational_validator_accepts_fully_consistent_config() {
    const auto rx_cfg = make_valid_audio_multicast_rx_config();
    const auto cfg = make_valid_audio_operational_config(rx_cfg);

    assert(st2110::validate_socket_rx_audio_operational_config(cfg) == st2110::Error::Ok);
    assert(st2110::validate_rx_audio_config(cfg.rx_config) == st2110::Error::Ok);
    assert(st2110::validate_socket_rx_operational_common_config(cfg.common) == st2110::Error::Ok);
}

void test_socket_rx_audio_backend_stop_before_start_and_repeated_stop_are_ok() {
    auto state = std::make_shared<FakeSocketRxPortState>();
    st2110::SocketRxAudioBackend backend(std::make_unique<FakeSocketRxPortFactory>(state));

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

void test_socket_rx_audio_backend_accepts_operational_start_and_projects_open_config() {
    auto state = std::make_shared<FakeSocketRxPortState>();
    st2110::SocketRxAudioBackend backend(std::make_unique<FakeSocketRxPortFactory>(state));
    DummyAudioSink sink;

    const auto rx_cfg = make_valid_audio_multicast_rx_config();
    const auto cfg = make_valid_audio_operational_config(rx_cfg);

    auto started = backend.start_audio(cfg, sink);
    assert(started.has_value());
    assert(st2110::backend_media_active(*started, st2110::RxMediaKind::Audio));
    assert(!st2110::backend_media_active(*started, st2110::RxMediaKind::Video));

    {
        std::lock_guard lock(state->mutex_);
        assert(state->create_count == 1);
        assert(state->open_count == 1);
        assert(state->is_open);
        assert(state->last_open_config.has_value());
        assert(st2110::socket_rx_open_config_equal(*state->last_open_config, cfg.common.open_config));
    }

    auto started_again = backend.start_audio(cfg, sink);
    assert(!started_again.has_value());
    assert(started_again.error() == st2110::Error::InvalidBackendState);

    auto stopped = backend.stop();
    assert(stopped.has_value());
    assert(st2110::backend_is_stopped(*stopped));
    assert(st2110::backend_is_stopped(backend.state()));
}

void test_socket_rx_audio_backend_rejects_mismatched_audio_packet_policy_vs_rx_config() {
    auto state = std::make_shared<FakeSocketRxPortState>();
    st2110::SocketRxAudioBackend backend(std::make_unique<FakeSocketRxPortFactory>(state));
    DummyAudioSink sink;

    auto cfg = make_valid_audio_operational_config(make_valid_audio_unicast_rx_config());
    ++cfg.audio_packet_policy.samples_per_packet;

    assert(st2110::validate_socket_rx_audio_operational_config(cfg) == st2110::Error::InvalidValue);

    auto started = backend.start_audio(cfg, sink);
    assert(!started.has_value());
    assert(started.error() == st2110::Error::InvalidValue);
    assert(st2110::backend_is_stopped(backend.state()));

    {
        std::lock_guard lock(state->mutex_);
        assert(state->create_count == 0);
        assert(state->open_count == 0);
    }
}

void test_socket_rx_audio_backend_rejects_invalid_reorder_config() {
    auto state = std::make_shared<FakeSocketRxPortState>();
    st2110::SocketRxAudioBackend backend(std::make_unique<FakeSocketRxPortFactory>(state));
    DummyAudioSink sink;

    auto cfg = make_valid_audio_operational_config(make_valid_audio_unicast_rx_config());
    cfg.reorder_buffer_config.window_size_packets = 0U;

    assert(st2110::validate_socket_rx_audio_operational_config(cfg) == st2110::Error::InvalidValue);

    auto started = backend.start_audio(cfg, sink);
    assert(!started.has_value());
    assert(started.error() == st2110::Error::InvalidValue);
    assert(st2110::backend_is_stopped(backend.state()));

    {
        std::lock_guard lock(state->mutex_);
        assert(state->create_count == 0);
        assert(state->open_count == 0);
    }
}

void test_socket_rx_audio_backend_rejects_mismatched_timestamp_mapper_config_vs_rx_config() {
    auto state = std::make_shared<FakeSocketRxPortState>();
    st2110::SocketRxAudioBackend backend(std::make_unique<FakeSocketRxPortFactory>(state));
    DummyAudioSink sink;

    auto cfg = make_valid_audio_operational_config(make_valid_audio_unicast_rx_config());
    cfg.timestamp_mapper_config.rtp_clock_rate += 1U;

    assert(st2110::validate_socket_rx_audio_operational_config(cfg) == st2110::Error::InvalidValue);

    auto started = backend.start_audio(cfg, sink);
    assert(!started.has_value());
    assert(started.error() == st2110::Error::InvalidValue);
    assert(st2110::backend_is_stopped(backend.state()));

    {
        std::lock_guard lock(state->mutex_);
        assert(state->create_count == 0);
        assert(state->open_count == 0);
    }
}

void test_socket_rx_audio_backend_rejects_invalid_created_port() {
    auto state = std::make_shared<FakeSocketRxPortState>();
    state->return_null_port = true;

    st2110::SocketRxAudioBackend backend(std::make_unique<FakeSocketRxPortFactory>(state));
    DummyAudioSink sink;

    const auto cfg = make_valid_audio_operational_config(make_valid_audio_multicast_rx_config());

    auto started = backend.start_audio(cfg, sink);
    assert(!started.has_value());
    assert(started.error() == st2110::Error::InvalidValue);
    assert(st2110::backend_is_stopped(backend.state()));

    {
        std::lock_guard lock(state->mutex_);
        assert(state->create_count == 1);
        assert(state->open_count == 0);
    }
}

void test_socket_rx_audio_backend_delivers_audio_block_from_operational_start_with_first_observed_local_zero_timestamp() {
    auto state = std::make_shared<FakeSocketRxPortState>();
    st2110::SocketRxAudioBackend backend(std::make_unique<FakeSocketRxPortFactory>(state));
    CapturingAudioSink sink;

    const auto cfg = make_valid_audio_operational_config(make_receive_audio_rx_config());

    auto started = backend.start_audio(cfg, sink);
    assert(started.has_value());
    assert(st2110::backend_media_active(*started, st2110::RxMediaKind::Audio));

    enqueue_datagram(state, make_audio_rtp_datagram(1U, 48'000U, cfg.rx_config.payload_type, 1000));

    assert(wait_for_receive_count_at_least(state, 1U, std::chrono::milliseconds(500)));
    assert(sink.wait_for_frame_count(1U, std::chrono::milliseconds(500)));

    const auto frame = sink.frame_at(0);
    assert(frame.timestamp_ns == 0ULL);
    assert(frame.sampling_rate_hz == kDefaultSamplingRateHz);
    assert(frame.channel_count == kDefaultChannelCount);
    assert(frame.samples_per_channel == kDefaultSamplesPerPacket);
    assert(frame.sample_frame_stride == kDefaultChannelCount);
    assert(frame.total_sample_count == static_cast<std::size_t>(kDefaultSamplesPerPacket) *
                                           static_cast<std::size_t>(kDefaultChannelCount));
    assert(frame.size_bytes == frame.total_sample_count * sizeof(std::int32_t));
    assert(!frame.samples.empty());
    assert(frame.samples[0] == 1000);
    assert(frame.samples[1] == 1001);
    assert(frame.samples[2] == 1002);

    const auto stats = backend.stats();
    assert(stats.datagrams_received == 1U);
    assert(stats.media_units_delivered == 1U);
    assert(stats.frames_delivered == 0U);
    assert(stats.packets_parsed_ok == 1U);
    assert(stats.packets_rejected == 0U);

    auto stopped = backend.stop();
    assert(stopped.has_value());
    assert(st2110::backend_is_stopped(*stopped));
}

void test_socket_rx_audio_backend_delivers_audio_block_with_configured_reference_timestamp_when_explicitly_requested() {
    auto state = std::make_shared<FakeSocketRxPortState>();
    st2110::SocketRxAudioBackend backend(std::make_unique<FakeSocketRxPortFactory>(state));
    CapturingAudioSink sink;

    const auto timestamp_mapper_config =
        make_configured_reference_audio_timestamp_mapper_config(kDefaultSamplingRateHz);
    const auto cfg =
        make_valid_audio_operational_config(make_receive_audio_rx_config(),
                                            {},
                                            st2110::AudioFrameAssemblerConfig{},
                                            st2110::AudioReorderBufferConfig{},
                                            timestamp_mapper_config);

    auto started = backend.start_audio(cfg, sink);
    assert(started.has_value());
    assert(st2110::backend_media_active(*started, st2110::RxMediaKind::Audio));

    enqueue_datagram(state, make_audio_rtp_datagram(1U, 48'000U, cfg.rx_config.payload_type, 1000));

    assert(wait_for_receive_count_at_least(state, 1U, std::chrono::milliseconds(500)));
    assert(sink.wait_for_frame_count(1U, std::chrono::milliseconds(500)));

    const auto frame = sink.frame_at(0);
    assert(frame.timestamp_ns == 1'000'000'000ULL);
    assert(frame.sampling_rate_hz == kDefaultSamplingRateHz);
    assert(frame.channel_count == kDefaultChannelCount);
    assert(frame.samples_per_channel == kDefaultSamplesPerPacket);
    assert(frame.sample_frame_stride == kDefaultChannelCount);
    assert(frame.total_sample_count == static_cast<std::size_t>(kDefaultSamplesPerPacket) *
                                           static_cast<std::size_t>(kDefaultChannelCount));
    assert(frame.size_bytes == frame.total_sample_count * sizeof(std::int32_t));
    assert(!frame.samples.empty());
    assert(frame.samples[0] == 1000);
    assert(frame.samples[1] == 1001);
    assert(frame.samples[2] == 1002);

    const auto stats = backend.stats();
    assert(stats.datagrams_received == 1U);
    assert(stats.media_units_delivered == 1U);
    assert(stats.frames_delivered == 0U);
    assert(stats.packets_parsed_ok == 1U);
    assert(stats.packets_rejected == 0U);

    auto stopped = backend.stop();
    assert(stopped.has_value());
    assert(st2110::backend_is_stopped(*stopped));
}

void test_socket_rx_audio_backend_extended_packet_policy_is_used_by_receive_runtime() {
    auto state = std::make_shared<FakeSocketRxPortState>();
    st2110::SocketRxAudioBackend backend(std::make_unique<FakeSocketRxPortFactory>(state));
    DummyAudioSink sink;

    st2110::PacketParsePolicy policy{};
    policy.max_udp_datagram_bytes = st2110::extendedUdpDatagramSizeLimitBytes;

    const auto cfg = make_valid_audio_operational_config(make_receive_audio_rx_config(), policy);

    auto started = backend.start_audio(cfg, sink);
    assert(started.has_value());

    enqueue_datagram(state, make_large_version2_audio_datagram(2'000U, cfg.rx_config.payload_type));

    assert(wait_for_receive_count_at_least(state, 1U, std::chrono::milliseconds(500)));
    assert(wait_until(
        [&] {
            const auto stats = backend.stats();
            return stats.datagrams_received >= 1U && stats.packets_rejected >= 1U;
        },
        std::chrono::milliseconds(500)));

    const auto stats = backend.stats();
    assert(stats.datagrams_received == 1U);
    assert(stats.media_units_delivered == 0U);
    assert(stats.packets_rejected == 1U);

    auto stopped = backend.stop();
    assert(stopped.has_value());
    assert(st2110::backend_is_stopped(*stopped));
}

} // namespace

int main() {
    test_socket_rx_audio_operational_validator_accepts_fully_consistent_config();
    test_socket_rx_audio_backend_stop_before_start_and_repeated_stop_are_ok();
    test_socket_rx_audio_backend_accepts_operational_start_and_projects_open_config();
    test_socket_rx_audio_backend_rejects_mismatched_audio_packet_policy_vs_rx_config();
    test_socket_rx_audio_backend_rejects_invalid_reorder_config();
    test_socket_rx_audio_backend_rejects_mismatched_timestamp_mapper_config_vs_rx_config();
    test_socket_rx_audio_backend_rejects_invalid_created_port();
    test_socket_rx_audio_backend_delivers_audio_block_from_operational_start_with_first_observed_local_zero_timestamp();
    test_socket_rx_audio_backend_delivers_audio_block_with_configured_reference_timestamp_when_explicitly_requested();
    test_socket_rx_audio_backend_extended_packet_policy_is_used_by_receive_runtime();
    return 0;
}
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
#include <thread>
#include <utility>
#include <vector>

#include <st2110/backends/socket/platform/socket_runtime.hpp>
#include <st2110/backends/socket/socket_rx_video_backend.hpp>
#include <st2110/contracts/backend/backend.hpp>
#include <st2110/delivery/video/video_frame.hpp>
#include <st2110/ingress/shared/packet_view.hpp>
#include <st2110/receive/shared/packet_admission.hpp>
#include <st2110/rx_config.hpp>

namespace {

struct FakeSocketRxPortState {
    mutable std::mutex mutex_{};
    std::condition_variable cv_{};

    bool is_open = false;

    int create_count = 0;
    int open_count = 0;
    int close_count = 0;
    int receive_count = 0;

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
        (void)cfg;

        std::lock_guard lock(state_->mutex_);
        if (state_->is_open) {
            return st2110::Error::InvalidBackendState;
        }

        ++state_->open_count;
        state_->is_open = true;
        state_->cv_.notify_all();
        return st2110::Error::Ok;
    }

    st2110::Error close() override {
        std::lock_guard lock(state_->mutex_);
        ++state_->close_count;
        state_->is_open = false;
        state_->cv_.notify_all();
        return st2110::Error::Ok;
    }

    [[nodiscard]] std::expected<st2110::SocketReceiveResult, st2110::Error>
    receive(std::span<std::uint8_t> buffer) override {
        std::unique_lock lock(state_->mutex_);

        for (;;) {
            if (!state_->is_open) {
                return std::unexpected(st2110::Error::OperationAborted);
            }

            if (!state_->queued_datagrams.empty()) {
                auto datagram = std::move(state_->queued_datagrams.front());
                state_->queued_datagrams.pop_front();
                ++state_->receive_count;
                state_->cv_.notify_all();
                lock.unlock();

                if (buffer.size() < datagram.size()) {
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
        return std::make_unique<FakeSocketRxPort>(state_);
    }

  private:
    std::shared_ptr<FakeSocketRxPortState> state_;
};

class FakeVideoSink final : public st2110::IVideoFrameSink {
  public:
    void on_video_frame(const st2110::VideoFrameView& frame) override {
        (void)frame;
        std::lock_guard lock(mutex_);
        ++frame_count_;
    }

    [[nodiscard]] std::size_t frame_count() const {
        std::lock_guard lock(mutex_);
        return frame_count_;
    }

  private:
    mutable std::mutex mutex_{};
    std::size_t frame_count_ = 0;
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

st2110::RxVideoConfig make_receive_ipv4_unicast_video_config(uint16_t port,
                                                             uint32_t width = 4,
                                                             uint8_t payload_type = 112) {
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

st2110::SocketRxVideoOperationalConfig
make_receive_video_operational_config(uint16_t port, uint32_t width = 4, uint8_t payload_type = 112) {
    const auto rx_cfg = make_receive_ipv4_unicast_video_config(port, width, payload_type);
    auto operational = st2110::socket_rx_video_operational_config_from_rx_video_config(
        rx_cfg,
        st2110::PacketParsePolicy{},
        st2110::PartialFramePolicy::Drop,
        st2110::VideoRtpTimestampMapperConfig{});
    assert(operational.has_value());
    return *operational;
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

void test_matching_dynamic_payload_type_is_accepted() {
    const std::array<std::uint8_t, 8> pixels{0x10u, 0x80u, 0x20u, 0x90u, 0x30u, 0xA0u, 0x40u, 0xB0u};
    const auto datagram = build_single_segment_video_datagram(112, 1, 90000, pixels, true, 0, 0);

    auto packet = st2110::PacketView::from_udp_datagram(st2110::ByteSpan(datagram.data(), datagram.size()));
    assert(packet.has_value());
    assert(packet->rtp.payload_type == 112u);

    assert(st2110::validate_rtp_payload_type_admission(packet->rtp.payload_type, 112u) == st2110::Error::Ok);
    assert(st2110::validate_video_packet_payload_type_admission(*packet, 112u) == st2110::Error::Ok);
}

void test_mismatching_payload_type_is_rejected() {
    const std::array<std::uint8_t, 8> pixels{0x10u, 0x80u, 0x20u, 0x90u, 0x30u, 0xA0u, 0x40u, 0xB0u};
    const auto datagram = build_single_segment_video_datagram(111, 2, 90000, pixels, true, 0, 0);

    auto packet = st2110::PacketView::from_udp_datagram(st2110::ByteSpan(datagram.data(), datagram.size()));
    assert(packet.has_value());
    assert(packet->rtp.payload_type == 111u);

    assert(st2110::validate_rtp_payload_type_admission(packet->rtp.payload_type, 112u) ==
           st2110::Error::InvalidValue);
    assert(st2110::validate_video_packet_payload_type_admission(*packet, 112u) ==
           st2110::Error::InvalidValue);
}

void test_payload_type_admission_remains_separate_from_generic_rtp_parsing() {
    const std::array<std::uint8_t, 8> pixels{0x10u, 0x80u, 0x20u, 0x90u, 0x30u, 0xA0u, 0x40u, 0xB0u};
    const auto datagram = build_single_segment_video_datagram(110, 3, 90000, pixels, true, 0, 0);

    auto packet = st2110::PacketView::from_udp_datagram(st2110::ByteSpan(datagram.data(), datagram.size()));
    assert(packet.has_value());

    assert(packet->rtp.version == 2u);
    assert(packet->rtp.payload_type == 110u);

    assert(st2110::validate_video_packet_payload_type_admission(*packet, 112u) ==
           st2110::Error::InvalidValue);
}

void test_depacketizer_is_not_entered_for_wrong_payload_type() {
    auto state = std::make_shared<FakeSocketRxPortState>();
    st2110::SocketRxVideoBackend backend(std::make_unique<FakeSocketRxPortFactory>(state));

    FakeVideoSink sink;
    const auto cfg = make_receive_video_operational_config(5004, 4, 112);

    auto started = backend.start_video(cfg, sink);
    assert(started.has_value());
    assert(st2110::backend_media_active(*started, st2110::RxMediaKind::Video));

    const std::array<std::uint8_t, 8> pixels{0x10u, 0x80u, 0x20u, 0x90u, 0x30u, 0xA0u, 0x40u, 0xB0u};
    enqueue_datagram(state, build_single_segment_video_datagram(111, 10, 90000, pixels, true, 0, 0));

    const bool received =
        wait_until([&] { return current_receive_count(state) >= 1; }, std::chrono::milliseconds(200));
    assert(received);

    const auto stats = backend.stats();
    assert(stats.datagrams_received == 1);
    assert(stats.nonmedia_datagrams_ignored == 1);
    assert(stats.datagrams_dropped == 1);
    assert(stats.packets_parsed_ok == 0);
    assert(stats.packets_rejected == 0);
    assert(stats.media_units_delivered == 0);
    assert(stats.frames_delivered == 0);
    assert(stats.depacketizer.packets_in == 0);
    assert(stats.reorder.packets_pushed == 0);
    assert(stats.packet_parse.parser_stats.packets_total == 0);

    assert(sink.frame_count() == 0);

    auto stopped = backend.stop();
    assert(stopped.has_value());
    assert(st2110::backend_is_stopped(*stopped));
}

} // namespace

int main() {
    test_matching_dynamic_payload_type_is_accepted();
    test_mismatching_payload_type_is_rejected();
    test_payload_type_admission_remains_separate_from_generic_rtp_parsing();
    test_depacketizer_is_not_entered_for_wrong_payload_type();
    return 0;
}
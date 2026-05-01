#ifndef ST2110_OBS_SOCKET_RX_SINGLE_MEDIA_BACKEND_BASE_HPP
#define ST2110_OBS_SOCKET_RX_SINGLE_MEDIA_BACKEND_BASE_HPP

#include "backend.hpp"
#include "bytes.hpp"
#include "packet_parse.hpp"
#include "socket_runtime.hpp"
#include "stats.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <thread>
#include <utility>
#include <vector>

namespace st2110 {

class SocketRxSingleMediaBackendBase : public virtual IRxBackend {
  public:
    ~SocketRxSingleMediaBackendBase() override = default;

    [[nodiscard]] const char *backend_name() const override { return "socket"; }

    [[nodiscard]] RxBackendCapabilities capabilities() const override { return capabilities_; }

    [[nodiscard]] RxBackendState state() const override { return state_; }

    [[nodiscard]] BackendStats stats() const override {
        std::lock_guard lock(stats_mutex_);
        BackendStats snapshot = build_base_stats_snapshot_locked();
        augment_stats_snapshot_locked(snapshot);
        return snapshot;
    }

    RxBackendLifecycleResult stop() override {
        if (!state_.audio_active && !state_.video_active && port_ == nullptr) {
            return state_;
        }

        if (port_ != nullptr && port_->is_open()) {
            if (Error err = port_->close(); err != Error::Ok) {
                return std::unexpected(err);
            }
        }

        clear_media_runtime_objects();

        return state_;
    }

  protected:
    SocketRxSingleMediaBackendBase(RxMediaKind media_kind, RxBackendCapabilities capabilities,
                                   std::unique_ptr<ISocketRxPortFactory> port_factory)
        : port_factory_{std::move(port_factory)}, media_kind_(media_kind), capabilities_(capabilities) {}

    [[nodiscard]] static std::unique_ptr<ISocketRxPortFactory> make_default_port_factory();

    [[nodiscard]] Error validate_common_start_preconditions() const noexcept {
        if (!media_active() && port_factory_ != nullptr) {
            return Error::Ok;
        }

        if (media_active()) {
            return Error::InvalidBackendState;
        }

        return Error::InvalidValue;
    }

    [[nodiscard]] std::unique_ptr<ISocketRxPort> create_port() const { return port_factory_->create_port(); }

    [[nodiscard]] bool media_active() const noexcept {
        switch (media_kind_) {
        case RxMediaKind::Audio:
            return state_.audio_active;
        case RxMediaKind::Video:
            return state_.video_active;
        }

        return false;
    }

    void set_media_active(bool active) noexcept {
        switch (media_kind_) {
        case RxMediaKind::Audio:
            state_.audio_active = active;
            break;
        case RxMediaKind::Video:
            state_.video_active = active;
            break;
        }
    }

    [[nodiscard]] RxBackendLifecycleResult start_common_runtime(std::unique_ptr<ISocketRxPort> port,
                                                                const SocketRxOpenConfig &open_cfg,
                                                                std::vector<std::uint8_t> receive_buffer) {
        if (Error err = port->open(open_cfg); err != Error::Ok) {
            return std::unexpected(err);
        }

        port_ = std::move(port);
        receive_buffer_ = std::move(receive_buffer);

        {
            std::lock_guard lock(stats_mutex_);
            stats_ = {};
        }

        try {
            receive_thread_ = std::jthread([this](std::stop_token stop_token) { run_receive_loop(stop_token); });
        } catch (...) {
            if (port_ && port_->is_open()) {
                (void)port_->close();
            }
            clear_media_runtime_objects();
            return std::unexpected(Error::SystemFailure);
        }

        set_media_active(true);
        return state_;
    }

    void run_receive_loop(std::stop_token stop_token) noexcept {
        if (port_ == nullptr || receive_buffer_.empty()) {
            return;
        }

        while (!stop_token.stop_requested()) {
            auto received = port_->receive(receive_buffer_);
            if (!received) {
                switch (received.error()) {
                case Error::ReceiveInterrupted:
                    continue;
                case Error::ReceiveAborted:
                case Error::ReceiveFailed:
                case Error::InvalidBackendState:
                case Error::InvalidValue:
                case Error::SystemFailure:
                case Error::Unsupported:
                case Error::BindFailed:
                case Error::MulticastJoinFailed:
                case Error::MulticastLeaveFailed:
                case Error::BufferTooSmall:
                case Error::ShortPacket:
                case Error::BadRTPVersion:
                case Error::Ok:
                default:
                    return;
                }
            }

            record_received_datagram(received->size_bytes);

            process_received_datagram(ByteSpan(receive_buffer_.data(), received->size_bytes));
        }
    }

    void clear_common_runtime_objects() noexcept {
        receive_thread_ = std::jthread{};
        port_.reset();
        receive_buffer_.clear();
        set_media_active(false);
        reset_stats();
    }

    void reset_stats() noexcept {
        std::lock_guard lock(stats_mutex_);
        stats_ = {};
    }

    void record_received_datagram(std::size_t size_bytes) noexcept {
        std::lock_guard lock(stats_mutex_);
        ++stats_.datagrams_received;
        stats_.bytes_received += size_bytes;
    }

    void record_ignored_control_datagram() noexcept {
        std::lock_guard lock(stats_mutex_);
        ++stats_.control_datagrams_ignored;
        ++stats_.datagrams_dropped;
    }

    void record_ignored_nonmedia_datagram() noexcept {
        std::lock_guard lock(stats_mutex_);
        ++stats_.nonmedia_datagrams_ignored;
        ++stats_.datagrams_dropped;
    }

    void record_rejected_packet(Error err, PacketParseStage stage) noexcept {
        std::lock_guard lock(stats_mutex_);
        ++stats_.packets_rejected;
        ++stats_.datagrams_dropped;
        record_packet_parse_result(stats_.packet_parse, err, stage);
    }

    void record_parsed_packet_ok() noexcept {
        std::lock_guard lock(stats_mutex_);
        ++stats_.packets_parsed_ok;
        record_packet_parse_result(stats_.packet_parse, Error::Ok, PacketParseStage::RtpHeader);
    }

    void record_delivered_video_frame() noexcept {
        std::lock_guard lock(stats_mutex_);
        ++stats_.frames_delivered;
    }

    void record_delivered_media_unit() noexcept {
        std::lock_guard lock(stats_mutex_);
        ++stats_.media_units_delivered;
    }

    [[nodiscard]] BackendStats build_base_stats_snapshot_locked() const noexcept { return stats_; }

    [[nodiscard]] static std::vector<std::uint8_t> make_receive_buffer(const PacketParsePolicy &policy) {
        const std::size_t size_bytes = effective_max_udp_datagram_bytes(policy) - udpHeaderBytes;
        return std::vector<std::uint8_t>(size_bytes);
    }

    [[nodiscard]] static bool is_rtcp_like_datagram(ByteSpan udp_payload) noexcept {
        if (udp_payload.size() < 2) {
            return false;
        }

        const auto version = static_cast<std::uint8_t>((udp_payload[0] >> 6) & 0x03u);
        if (version != 2u) {
            return false;
        }

        const std::uint8_t payload_type = udp_payload[1];
        return payload_type >= 192u && payload_type <= 223u;
    }

    [[nodiscard]] static bool datagram_matches_configured_payload_type(ByteSpan udp_payload,
                                                                       std::uint8_t configured_payload_type) noexcept {
        if (udp_payload.size() < 2) {
            return false;
        }

        const auto version = static_cast<std::uint8_t>((udp_payload[0] >> 6) & 0x03u);
        if (version != 2u) {
            return false;
        }

        const auto payload_type = static_cast<std::uint8_t>(udp_payload[1] & 0x7Fu);
        return payload_type == configured_payload_type;
    }

    virtual void process_received_datagram(ByteSpan udp_payload) noexcept = 0;
    virtual void clear_media_runtime_objects() noexcept = 0;
    virtual void augment_stats_snapshot_locked(BackendStats &snapshot) const noexcept {}

  protected:
    std::unique_ptr<ISocketRxPortFactory> port_factory_{};
    std::unique_ptr<ISocketRxPort> port_{};
    std::vector<std::uint8_t> receive_buffer_{};
    std::jthread receive_thread_{};

    RxBackendState state_{};
    mutable std::mutex stats_mutex_{};
    BackendStats stats_{};

  private:
    RxMediaKind media_kind_;
    RxBackendCapabilities capabilities_;
};

} // namespace st2110

#endif // ST2110_OBS_SOCKET_RX_SINGLE_MEDIA_BACKEND_BASE_HPP
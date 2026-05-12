#ifndef ST2110_OBS_SOCKET_RX_SINGLE_MEDIA_BACKEND_BASE_HPP
#define ST2110_OBS_SOCKET_RX_SINGLE_MEDIA_BACKEND_BASE_HPP

#include "st2110/contracts/backend/backend.hpp"
#include "st2110/foundation/bytes.hpp"
#include "st2110/ingress/shared/packet_parse.hpp"
#include "platform/socket_runtime.hpp"
#include <st2110/receive/shared/reorder_buffer.hpp>
#include <st2110/receive/shared/receive_reorder_tolerance_policy.hpp>

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

    [[nodiscard]] BackendStats stats() const override {
        std::lock_guard lock(stats_mutex_);
        BackendStats snapshot = build_base_stats_snapshot_locked();
        augment_stats_snapshot_locked(snapshot);
        return snapshot;
    }

    RxBackendLifecycleResult stop() override {
        if (!is_active && port_ == nullptr) {
            return is_active;
        }

        if (port_ != nullptr && port_->is_open()) {
            if (Error err = port_->close(); err != Error::Ok) {
                return std::unexpected(err);
            }
        }

        clear_media_runtime_objects();

        return is_active;
    }

  protected:
    explicit SocketRxSingleMediaBackendBase(std::unique_ptr<ISocketRxPortFactory> port_factory)
        : port_factory_{std::move(port_factory)} {}

    [[nodiscard]] static std::unique_ptr<ISocketRxPortFactory> make_default_port_factory();

    [[nodiscard]] std::unique_ptr<ISocketRxPort> create_port() const { return port_factory_->create_port(); }

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

        is_active = true;
        return is_active;
    }

    void run_receive_loop(std::stop_token stop_token) noexcept {
        if (port_ == nullptr || receive_buffer_.empty()) {
            return;
        }

        while (!stop_token.stop_requested()) {
            auto received = port_->receive(receive_buffer_);
            if (!received) {
                switch (received.error()) {
                case Error::OperationInterrupted:
                    continue;
                case Error::OperationAborted:
                case Error::InvalidBackendState:
                case Error::InvalidValue:
                case Error::SystemFailure:
                case Error::Unsupported:
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
        is_active = false;
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
        record_rejected_media_packet();
        std::lock_guard lock(stats_mutex_);
        record_packet_parse_result(stats_.packet_parse, err, stage);
    }

    void record_accepted_media_packet() noexcept {
        std::lock_guard lock(stats_mutex_);
        ++stats_.packets_parsed_ok;
    }

    void record_rejected_media_packet() noexcept {
        std::lock_guard lock(stats_mutex_);
        ++stats_.packets_rejected;
        ++stats_.datagrams_dropped;
    }

    void record_parsed_packet_ok() noexcept {
        std::lock_guard lock(stats_mutex_);
        ++stats_.packets_parsed_ok;
        record_packet_parse_result(stats_.packet_parse, Error::Ok, PacketParseStage::RtpHeader);
    }

    void record_delivered_media_unit() noexcept {
        std::lock_guard lock(stats_mutex_);
        ++stats_.media_units_delivered;
    }

    [[nodiscard]] BackendStats build_base_stats_snapshot_locked() const noexcept { return stats_; }

    [[nodiscard]] static std::vector<std::uint8_t> make_receive_buffer(const std::size_t maxudp) {
        return std::vector<std::uint8_t>(maxudp - udpHeaderBytes);
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
    virtual std::unique_ptr<IReorderBuffer> make_reorder_buffer(const ReorderBufferConfig &cfg) = 0;
    virtual void deliver_media(std::unique_ptr<StoredPacket> packet) = 0;
    void apply_reorder_buffer_policy(bool& gap_flush_used) const {
        switch (reorder_tolerance_policy_) {
        case ReceiveReorderGapPolicy::WaitForMissing:
            return;
        case ReceiveReorderGapPolicy::FlushGapOnce:
            gap_flush_used = reorder_buffer_->flush_missing_once();
            return;
        default:
            throw std::runtime_error("Such reorder policy is not implemented yet");
        }
    }
    void drain_reorder_buffer_to_sink() {
        bool gap_flush_used = false;

        while (true) {
            auto stored_packet = reorder_buffer_->pop_next();
            if (!stored_packet) {
                if (gap_flush_used) {
                    return;
                }

                apply_reorder_buffer_policy(gap_flush_used);

                if (!gap_flush_used) {
                    return;
                }

                continue;
            }

            deliver_media(std::move(stored_packet));
        }
    }
    virtual void augment_stats_snapshot_locked(BackendStats &snapshot) const noexcept {}

  protected:
    std::unique_ptr<ISocketRxPortFactory> port_factory_{};
    std::unique_ptr<ISocketRxPort> port_{};
    std::vector<std::uint8_t> receive_buffer_{};
    std::unique_ptr<IReorderBuffer> reorder_buffer_{};
    std::jthread receive_thread_{};
    ReceiveReorderGapPolicy reorder_tolerance_policy_ = ReceiveReorderGapPolicy::WaitForMissing;

    bool is_active = false;
    mutable std::mutex stats_mutex_{};
    BackendStats stats_{};
};

} // namespace st2110

#endif // ST2110_OBS_SOCKET_RX_SINGLE_MEDIA_BACKEND_BASE_HPP
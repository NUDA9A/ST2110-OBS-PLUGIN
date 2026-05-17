#ifndef ST2110_OBS_SOCKET_RX_SINGLE_MEDIA_BACKEND_BASE_HPP
#define ST2110_OBS_SOCKET_RX_SINGLE_MEDIA_BACKEND_BASE_HPP

#include "platform/socket_runtime.hpp"
#include "st2110/contracts/backend/backend.hpp"
#include "st2110/foundation/bytes.hpp"
#include "st2110/ingress/shared/packet_parse.hpp"
#include <st2110/receive/shared/receive_reorder_tolerance_policy.hpp>
#include <st2110/receive/shared/reorder_buffer.hpp>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

namespace st2110 {
struct DuplicateMergeHistory {
    std::deque<std::uint32_t> ordered_sequences{};
    std::unordered_set<std::uint32_t> sequence_set{};

    [[nodiscard]] bool contains(const std::uint32_t sequence) const { return sequence_set.contains(sequence); }

    void remember(const std::uint32_t sequence, const std::size_t capacity) {
        ordered_sequences.push_back(sequence);
        sequence_set.insert(sequence);

        while (ordered_sequences.size() > capacity) {
            sequence_set.erase(ordered_sequences.front());
            ordered_sequences.pop_front();
        }
    }

    void clear() noexcept {
        ordered_sequences.clear();
        sequence_set.clear();
    }
};

struct SocketRxRuntimeLeg {
    std::jthread thread{};
    std::unique_ptr<ISocketRxPort> port{};
    std::vector<std::uint8_t> receive_buffer{};

    void shutdown_nothrow() noexcept {
        if (port) {
            port->close();
        }
        thread = {};
        port.reset();
        receive_buffer.clear();
    }

    SocketRxRuntimeLeg() = default;

    SocketRxRuntimeLeg(const SocketRxRuntimeLeg &) = delete;
    SocketRxRuntimeLeg &operator=(const SocketRxRuntimeLeg &) = delete;

    SocketRxRuntimeLeg(SocketRxRuntimeLeg &&) noexcept = default;
    SocketRxRuntimeLeg &operator=(SocketRxRuntimeLeg &&) noexcept = default;

    ~SocketRxRuntimeLeg() { shutdown_nothrow(); }
};

class SocketRxStoredPacketQueue {
  public:
    void reset() {
        {
            std::lock_guard lock(mutex_);
            closed_ = false;
            packets_.clear();
        }
        cv_.notify_all();
    }

    void close() {
        {
            std::lock_guard lock(mutex_);
            closed_ = true;
        }
        cv_.notify_all();
    }

    [[nodiscard]] bool push(std::unique_ptr<StoredPacket> packet) {
        if (!packet) {
            return false;
        }

        {
            std::lock_guard lock(mutex_);
            if (closed_) {
                return false;
            }

            packets_.push_back(std::move(packet));
        }

        cv_.notify_one();
        return true;
    }

    [[nodiscard]] std::unique_ptr<StoredPacket> wait_pop(std::stop_token stop_token) {
        std::unique_lock lock(mutex_);

        const bool ready = cv_.wait(lock, stop_token, [this] { return closed_ || !packets_.empty(); });

        if (!ready || packets_.empty()) {
            return nullptr;
        }

        auto packet = std::move(packets_.front());
        packets_.pop_front();
        return packet;
    }

  private:
    std::mutex mutex_{};
    std::condition_variable_any cv_{};
    std::deque<std::unique_ptr<StoredPacket>> packets_{};
    bool closed_ = true;
};

class SocketRxSingleMediaBackendBase : public virtual IRxBackend {
  public:
    ~SocketRxSingleMediaBackendBase() override = default;

    [[nodiscard]] BackendStats stats_snapshot() const override {
        std::lock_guard lock(diagnostics_mutex_);
        return stats_;
    }

    [[nodiscard]] bool healthy() const override {
        std::lock_guard lock(diagnostics_mutex_);
        return healthy_;
    }

    [[nodiscard]] std::string last_error_message() const override {
        std::lock_guard lock(diagnostics_mutex_);
        return last_error_message_;
    }

    RxBackendLifecycleResult stop() override {
        stopping_.store(true, std::memory_order_relaxed);

        auto first_err = Error::Ok;

        for (const auto &leg : runtime_legs_) {
            if (!leg.port) {
                continue;
            }

            const Error err = leg.port->close();
            if (err != Error::Ok && first_err == Error::Ok) {
                first_err = err;
            }
        }

        runtime_legs_.clear();

        packet_queue_.close();
        downstream_thread_ = {};

        duplicate_merge_history_.clear();

        if (reorder_buffer_) {
            reorder_buffer_->reset();
        }

        duplicate_merge_enabled_ = false;

        if (first_err != Error::Ok) {
            record_backend_failure_noexcept(first_err, "Socket backend stop failed");
            return std::unexpected(first_err);
        }

        return true;
    }

  protected:
    explicit SocketRxSingleMediaBackendBase(std::unique_ptr<ISocketRxPortFactory> port_factory,
                                            const std::size_t maxudp, const ReorderBufferConfig &reorder_buffer_config,
                                            const std::vector<SocketRxOpenConfig> &open_configs,
                                            const std::uint8_t expected_payload_type)
        : open_configs_(open_configs), port_factory_{std::move(port_factory)}, maxudp_(maxudp),
          expected_payload_type_(expected_payload_type),
          reorder_tolerance_policy_(reorder_buffer_config.reorder_tolerance_policy),
          reorder_buffer_config_(reorder_buffer_config) {}

    [[nodiscard]] bool duplicate_merge_enabled() const noexcept { return duplicate_merge_enabled_; }

    [[nodiscard]] std::size_t duplicate_history_capacity() const noexcept {
        return reorder_buffer_config_.window_size_packets;
    }

    [[nodiscard]] static std::unique_ptr<ISocketRxPortFactory> make_default_port_factory();

    [[nodiscard]] std::unique_ptr<ISocketRxPort> create_port() const { return port_factory_->create_port(); }

    [[nodiscard]] RxBackendLifecycleResult start_common_runtime(IFrameSink *sink) {
        stopping_.store(false, std::memory_order_relaxed);
        reset_diagnostics_noexcept();

        sink_ = sink;
        duplicate_merge_history_.clear();
        std::vector<SocketRxRuntimeLeg> staged;
        staged.reserve(open_configs_.size());

        for (const auto &open_config : open_configs_) {
            SocketRxRuntimeLeg leg{};
            leg.port = create_port();
            if (!leg.port) {
                record_backend_failure_noexcept(Error::SystemFailure, "Socket port factory returned null port");
                return std::unexpected(Error::SystemFailure);
            }

            if (const Error err = leg.port->open(open_config); err != Error::Ok) {
                record_backend_failure_noexcept(err, "Socket port open failed");
                return std::unexpected(err);
            }

            leg.receive_buffer = make_receive_buffer(maxudp_);
            staged.emplace_back(std::move(leg));
        }

        packet_queue_.reset();
        duplicate_merge_history_.clear();

        duplicate_merge_enabled_ = staged.size() > 1;

        runtime_legs_ = std::move(staged);

        downstream_thread_ = std::jthread([this](std::stop_token stop_token) { run_downstream_loop(stop_token); });

        for (std::size_t i = 0; i < runtime_legs_.size(); ++i) {
            runtime_legs_[i].thread =
                std::jthread([this, i](std::stop_token stop_token) { run_receive_loop(i, stop_token); });
        }

        return true;
    }

    void run_receive_loop(const std::size_t leg_index, std::stop_token stop_token) noexcept {
        auto &leg = runtime_legs_[leg_index];
        while (!stop_token.stop_requested()) {
            auto received = leg.port->receive(leg.receive_buffer);
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
                    mark_receive_leg_unhealthy_noexcept(received.error(), leg_index, "Socket receive loop exited");
                    return;
                }
            }

            record_datagram_received_noexcept(received->size_bytes);

            process_received_datagram(leg_index, ByteSpan(leg.receive_buffer.data(), received->size_bytes),
                                      received->receive_timestamp_ns);
        }
    }

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

    virtual std::expected<std::unique_ptr<PacketView>, Error> parse_packet(std::size_t leg_index,
                                                                           ByteSpan udp_payload) = 0;
    virtual std::unique_ptr<IReorderBuffer> make_reorder_buffer(const ReorderBufferConfig &cfg) = 0;
    virtual void deliver_media(std::unique_ptr<StoredPacket> packet) = 0;
    void apply_reorder_buffer_policy(bool &gap_flush_used) const {
        switch (reorder_tolerance_policy_) {
        case ReceiveReorderGapPolicy::WaitForMissing:
            return;
        case ReceiveReorderGapPolicy::FlushGapOnce:
            gap_flush_used = reorder_buffer_->flush_missing_once();
            return;
        case ReceiveReorderGapPolicy::FlushAfterNPackets:
            gap_flush_used = reorder_buffer_->flush_after_n_packets(reorder_buffer_config_.flush_after_n_packets);
            return;
        case ReceiveReorderGapPolicy::FlushOnMarkerBoundary:
            gap_flush_used = reorder_buffer_->flush_missing_until_marker_boundary();
            return;
        default:
            throw std::runtime_error("Such reorder policy is not implemented yet");
        }
    }
    void drain_reorder_buffer_to_sink() {
        bool gap_flush_used = false;

        while (true) {
            auto stored_packet = reorder_buffer_->pop_next();
            if (stored_packet) {
                deliver_media(std::move(stored_packet));
                continue;
            }

            const bool allow_multiple_gap_flushes =
                reorder_tolerance_policy_ == ReceiveReorderGapPolicy::FlushAfterNPackets ||
                reorder_tolerance_policy_ == ReceiveReorderGapPolicy::FlushOnMarkerBoundary;

            if (gap_flush_used && !allow_multiple_gap_flushes) {
                return;
            }

            bool policy_flushed = false;
            apply_reorder_buffer_policy(policy_flushed);

            if (!policy_flushed) {
                return;
            }

            gap_flush_used = true;
        }
    }

    void process_received_datagram(const std::size_t leg_index, ByteSpan udp_payload,
                                   const TimestampNs receive_timestamp_ns) noexcept {
        if (is_rtcp_like_datagram(udp_payload)) {
            record_control_datagram_ignored_noexcept();
            return;
        }

        if (Error err = validate_packet_parse_policy(udp_payload, maxudp_); err != Error::Ok) {
            record_packet_rejected_noexcept();
            return;
        }

        auto packet = parse_packet(leg_index, udp_payload);
        if (!packet) {
            record_packet_rejected_noexcept();
            return;
        }

        if ((*packet)->rtp.payload_type != expected_payload_type_) {
            record_nonmedia_datagram_ignored_noexcept();
            return;
        }

        record_packet_parsed_ok_noexcept();

        (*packet)->receive_timestamp_ns = receive_timestamp_ns;

        auto stored_packet = (*packet)->store();
        if (!packet_queue_.push(std::move(stored_packet))) {
            record_datagram_dropped_noexcept();
        }
    }

    void process_stored_packet_downstream(std::unique_ptr<StoredPacket> packet) noexcept {
        const std::uint32_t reorder_sequence = packet->reorder_sequence();

        if (duplicate_merge_enabled() && duplicate_merge_history_.contains(reorder_sequence)) {
            return;
        }

        if (const Error err = reorder_buffer_->push(std::move(packet)); err != Error::Ok) {
            record_datagram_dropped_noexcept();
            return;
        }

        if (duplicate_merge_enabled()) {
            duplicate_merge_history_.remember(reorder_sequence, duplicate_history_capacity());
        }

        drain_reorder_buffer_to_sink();
    }

    void run_downstream_loop(std::stop_token stop_token) noexcept {
        while (!stop_token.stop_requested()) {
            auto packet = packet_queue_.wait_pop(stop_token);
            if (!packet) {
                return;
            }

            process_stored_packet_downstream(std::move(packet));
        }
    }

    void reset_diagnostics_noexcept() noexcept {
        try {
            std::lock_guard lock(diagnostics_mutex_);
            stats_ = BackendStats{};
            healthy_ = true;
            last_error_message_.clear();
        } catch (...) {
        }
    }

    void record_backend_failure_noexcept(const Error error, const char *message) noexcept {
        try {
            std::lock_guard lock(diagnostics_mutex_);
            healthy_ = false;
            last_error_message_ = message ? message : "Socket backend failure";
            last_error_message_ += ": ";
            last_error_message_ += to_string(error);
        } catch (...) {
        }
    }

    void mark_receive_leg_unhealthy_noexcept(const Error error, const std::size_t leg_index,
                                             const char *message) noexcept {
        if (stopping_.load(std::memory_order_relaxed)) {
            return;
        }

        try {
            std::lock_guard lock(diagnostics_mutex_);
            healthy_ = false;
            last_error_message_ = message ? message : "Socket receive loop exited";
            last_error_message_ += " on leg ";
            last_error_message_ += std::to_string(leg_index);
            last_error_message_ += ": ";
            last_error_message_ += to_string(error);
        } catch (...) {
        }
    }

    void record_datagram_received_noexcept(const std::size_t size_bytes) const noexcept {
        try {
            std::lock_guard lock(diagnostics_mutex_);
            ++stats_.datagrams_received;
            stats_.bytes_received += static_cast<std::uint64_t>(size_bytes);
        } catch (...) {
        }
    }

    void record_control_datagram_ignored_noexcept() const noexcept {
        try {
            std::lock_guard lock(diagnostics_mutex_);
            ++stats_.control_datagrams_ignored;
        } catch (...) {
        }
    }

    void record_nonmedia_datagram_ignored_noexcept() const noexcept {
        try {
            std::lock_guard lock(diagnostics_mutex_);
            ++stats_.nonmedia_datagrams_ignored;
        } catch (...) {
        }
    }

    void record_packet_parsed_ok_noexcept() const noexcept {
        try {
            std::lock_guard lock(diagnostics_mutex_);
            ++stats_.packets_parsed_ok;
        } catch (...) {
        }
    }

    void record_packet_rejected_noexcept() const noexcept {
        try {
            std::lock_guard lock(diagnostics_mutex_);
            ++stats_.packets_rejected;
        } catch (...) {
        }
    }

    void record_datagram_dropped_noexcept() const noexcept {
        try {
            std::lock_guard lock(diagnostics_mutex_);
            ++stats_.datagrams_dropped;
        } catch (...) {
        }
    }

    void record_media_unit_delivered_noexcept() const noexcept {
        try {
            std::lock_guard lock(diagnostics_mutex_);
            ++stats_.media_units_delivered;
        } catch (...) {
        }
    }

    void record_frame_delivered_noexcept() const noexcept {
        try {
            std::lock_guard lock(diagnostics_mutex_);
            ++stats_.frames_delivered;
        } catch (...) {
        }
    }

    mutable std::mutex diagnostics_mutex_{};
    mutable BackendStats stats_{};
    bool healthy_ = true;
    std::string last_error_message_{};
    std::atomic_bool stopping_{false};

    bool duplicate_merge_enabled_ = false;
    IFrameSink *sink_ = nullptr;
    std::vector<SocketRxOpenConfig> open_configs_{};
    std::unique_ptr<ISocketRxPortFactory> port_factory_{};
    std::size_t maxudp_{};
    std::uint8_t expected_payload_type_{};
    std::unique_ptr<IReorderBuffer> reorder_buffer_{};
    ReceiveReorderGapPolicy reorder_tolerance_policy_ = ReceiveReorderGapPolicy::WaitForMissing;
    ReorderBufferConfig reorder_buffer_config_;
    DuplicateMergeHistory duplicate_merge_history_{};
    SocketRxStoredPacketQueue packet_queue_{};
    std::jthread downstream_thread_{};
    std::vector<SocketRxRuntimeLeg> runtime_legs_{};
};

} // namespace st2110

#endif // ST2110_OBS_SOCKET_RX_SINGLE_MEDIA_BACKEND_BASE_HPP
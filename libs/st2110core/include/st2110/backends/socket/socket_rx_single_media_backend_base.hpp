#ifndef ST2110_OBS_SOCKET_RX_SINGLE_MEDIA_BACKEND_BASE_HPP
#define ST2110_OBS_SOCKET_RX_SINGLE_MEDIA_BACKEND_BASE_HPP

#include "platform/socket_runtime.hpp"
#include "st2110/contracts/backend/backend.hpp"
#include "st2110/foundation/bytes.hpp"
#include "st2110/ingress/shared/packet_parse.hpp"
#include <st2110/receive/shared/receive_reorder_tolerance_policy.hpp>
#include <st2110/receive/shared/reorder_buffer.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>
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

class SocketRxSingleMediaBackendBase : public virtual IRxBackend {
  public:
    ~SocketRxSingleMediaBackendBase() override = default;

    RxBackendLifecycleResult stop() override {
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

        {
            std::lock_guard lock(downstream_mutex_);
            duplicate_merge_history_.clear();
        }

        if (first_err != Error::Ok) {
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

    [[nodiscard]] bool duplicate_merge_enabled() const noexcept { return runtime_legs_.size() > 1; }

    [[nodiscard]] std::size_t duplicate_history_capacity() const noexcept {
        return reorder_buffer_config_.window_size_packets;
    }

    [[nodiscard]] static std::unique_ptr<ISocketRxPortFactory> make_default_port_factory();

    [[nodiscard]] std::unique_ptr<ISocketRxPort> create_port() const { return port_factory_->create_port(); }

    [[nodiscard]] RxBackendLifecycleResult start_common_runtime(IFrameSink* sink) {
        sink_ = sink;
        duplicate_merge_history_.clear();
        std::vector<SocketRxRuntimeLeg> staged;
        staged.reserve(open_configs_.size());

        for (const auto &open_config : open_configs_) {
            SocketRxRuntimeLeg leg{};
            leg.port = create_port();
            if (!leg.port) {
                return std::unexpected(Error::SystemFailure);
            }

            if (const Error err = leg.port->open(open_config); err != Error::Ok) {
                return std::unexpected(err);
            }

            leg.receive_buffer = make_receive_buffer(maxudp_);
            staged.emplace_back(std::move(leg));
        }

        runtime_legs_ = std::move(staged);

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
                    return;
                }
            }

            process_received_datagram(leg_index, ByteSpan(leg.receive_buffer.data(), received->size_bytes));
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

    void process_received_datagram(const std::size_t leg_index, ByteSpan udp_payload) noexcept {
        if (is_rtcp_like_datagram(udp_payload)) {
            return;
        }

        auto packet = parse_packet(leg_index, udp_payload);
        if (!packet) {
            return;
        }

        if ((*packet)->rtp.payload_type != expected_payload_type_) {
            return;
        }

        std::lock_guard lock(downstream_mutex_);

        const std::uint32_t reorder_sequence = (*packet)->reorder_sequence();
        if (duplicate_merge_enabled() && duplicate_merge_history_.contains(reorder_sequence)) {
            return;
        }

        if (const Error err = reorder_buffer_->push(*(*packet)); err != Error::Ok) {
            return;
        }

        if (duplicate_merge_enabled()) {
            duplicate_merge_history_.remember(reorder_sequence, duplicate_history_capacity());
        }

        drain_reorder_buffer_to_sink();
    }

    IFrameSink* sink_ = nullptr;
    std::vector<SocketRxOpenConfig> open_configs_{};
    std::unique_ptr<ISocketRxPortFactory> port_factory_{};
    std::size_t maxudp_{};
    std::uint8_t expected_payload_type_{};
    std::unique_ptr<IReorderBuffer> reorder_buffer_{};
    ReceiveReorderGapPolicy reorder_tolerance_policy_ = ReceiveReorderGapPolicy::WaitForMissing;
    ReorderBufferConfig reorder_buffer_config_;
    DuplicateMergeHistory duplicate_merge_history_{};
    std::mutex downstream_mutex_{};
    std::vector<SocketRxRuntimeLeg> runtime_legs_{};
};

} // namespace st2110

#endif // ST2110_OBS_SOCKET_RX_SINGLE_MEDIA_BACKEND_BASE_HPP
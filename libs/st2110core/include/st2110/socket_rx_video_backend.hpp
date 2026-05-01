#ifndef ST2110_OBS_SOCKET_RX_VIDEO_BACKEND_HPP
#define ST2110_OBS_SOCKET_RX_VIDEO_BACKEND_HPP

#include "backend.hpp"
#include "backend_factory.hpp"
#include "bytes.hpp"
#include "fixed_reorder_buffer.hpp"
#include "packet_parse.hpp"
#include "socket_runtime.hpp"
#include "video_receive_pipeline.hpp"
#include "video_timestamp_mapping.hpp"
#include "stats.hpp"

#include <memory>
#include <optional>
#include <thread>
#include <vector>
#include <mutex>

namespace st2110 {
class SocketRxVideoBackend final : public IRxVideoBackend {
  public:
    SocketRxVideoBackend() : port_factory_(make_default_port_factory()) {}

    explicit SocketRxVideoBackend(std::unique_ptr<ISocketRxPortFactory> port_factory)
        : port_factory_(std::move(port_factory)) {}

    [[nodiscard]] const char *backend_name() const override { return "socket"; }

    RxBackendLifecycleResult stop() override {
        if (!state_.audio_active && !state_.video_active && port_ == nullptr) {
            return state_;
        }

        if (port_ != nullptr && port_->is_open()) {
            if (Error err = port_->close(); err != Error::Ok) {
                return std::unexpected(err);
            }
        }

        clear_runtime_objects();

        return state_;
    }

    [[nodiscard]] RxBackendCapabilities capabilities() const override {
        return RxBackendCapabilities{.video_rx = true, .audio_rx = false};
    }

    RxBackendLifecycleResult start_video(const RxVideoConfig &cfg, IVideoFrameSink &sink) override {
        if (state_.video_active) {
            return std::unexpected(Error::InvalidBackendState);
        }

        if (Error err = validate_runtime_dependencies(); err != Error::Ok) {
            return std::unexpected(err);
        }

        auto open_cfg = build_open_config(cfg);
        if (!open_cfg) {
            return std::unexpected(open_cfg.error());
        }

        auto port = create_port();

        if (port == nullptr) {
            return std::unexpected(Error::InvalidValue);
        }

        return start_video_runtime(cfg, sink, *open_cfg, std::move(port));
    }

    [[nodiscard]] RxBackendState state() const override { return state_; }

    [[nodiscard]] BackendStats stats() const override;

  private:
    [[nodiscard]] static PacketParsePolicy build_packet_parse_policy(const RxVideoConfig &cfg) noexcept;
    [[nodiscard]] static VideoReceivePipelineConfig build_video_receive_pipeline_config(const RxVideoConfig &cfg);
    [[nodiscard]] static std::unique_ptr<IReorderBuffer> make_reorder_buffer();
    [[nodiscard]] static std::vector<std::uint8_t> make_receive_buffer(const PacketParsePolicy &policy);

    RxBackendLifecycleResult start_video_runtime(const RxVideoConfig &cfg, IVideoFrameSink &sink,
                                                 const SocketRxOpenConfig &open_cfg,
                                                 std::unique_ptr<ISocketRxPort> port);

    void run_video_receive_loop(std::stop_token stop_token) noexcept;
    void process_received_datagram(ByteSpan udp_payload) noexcept;
    void drain_reorder_buffer_to_sink() noexcept;
    void deliver_reconstructed_frame(ReconstructedVideoFrame &&frame) noexcept;
    [[nodiscard]] TimestampNs map_frame_timestamp_ns(uint32_t rtp_timestamp) noexcept;

    void record_received_datagram(std::size_t size_bytes) noexcept;
    void record_ignored_control_datagram() noexcept;
    void record_ignored_nonmedia_datagram() noexcept;
    void record_rejected_packet(Error err, PacketParseStage stage) noexcept;
    void record_parsed_packet_ok() noexcept;
    void record_delivered_frame() noexcept;
    [[nodiscard]] BackendStats build_stats_snapshot_locked() const noexcept;

    [[nodiscard]] static std::unique_ptr<ISocketRxPortFactory> make_default_port_factory();

    [[nodiscard]] Error validate_runtime_dependencies() const noexcept {
        if (port_factory_ == nullptr) {
            return Error::InvalidValue;
        }

        return Error::Ok;
    }

    [[nodiscard]] static std::expected<SocketRxOpenConfig, Error> build_open_config(const RxVideoConfig &cfg) {
        auto res = socket_rx_open_config_from_video_config(cfg);
        if (!res) {
            return std::unexpected(res.error());
        }

        return res;
    }

    [[nodiscard]] std::unique_ptr<ISocketRxPort> create_port() const { return port_factory_->create_port(); }

    void clear_runtime_objects() noexcept {
        receive_thread_ = std::jthread{};
        port_.reset();
        reorder_buffer_.reset();
        video_receive_pipeline_.reset();
        video_timestamp_mapper_.reset();
        packet_parse_policy_ = {};
        receive_buffer_.clear();
        video_sink_ = nullptr;
        configured_video_payload_type_.reset();
        state_ = {};
        stats_ = {};
    }

    std::unique_ptr<ISocketRxPortFactory> port_factory_{};
    std::unique_ptr<ISocketRxPort> port_{};
    RxBackendState state_{};
    std::unique_ptr<IReorderBuffer> reorder_buffer_{};
    std::unique_ptr<VideoReceivePipeline> video_receive_pipeline_{};
    std::optional<VideoRtpTimestampMapper> video_timestamp_mapper_{};
    PacketParsePolicy packet_parse_policy_{};
    std::vector<std::uint8_t> receive_buffer_{};
    IVideoFrameSink *video_sink_ = nullptr;
    std::jthread receive_thread_{};
    std::optional<std::uint8_t> configured_video_payload_type_{};
    mutable std::mutex stats_mutex_{};
    BackendStats stats_{};
};

class SocketRxVideoBackendFactory final : public IRxBackendFactory {
  public:
    [[nodiscard]] RxBackendDescriptor descriptor() const override {
        return RxBackendDescriptor{.kind = RxBackendKind::Socket,
                                   .name = "socket",
                                   .capabilities = RxBackendCapabilities{.video_rx = true, .audio_rx = false},
                                   .available = true};
    }

    [[nodiscard]] std::unique_ptr<IRxBackend> create_backend() const override {
        return std::unique_ptr<IRxBackend>(new SocketRxVideoBackend());
    }
};
} // namespace st2110

#endif // ST2110_OBS_SOCKET_RX_VIDEO_BACKEND_HPP

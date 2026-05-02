#ifndef ST2110_OBS_SOCKET_RX_VIDEO_BACKEND_HPP
#define ST2110_OBS_SOCKET_RX_VIDEO_BACKEND_HPP

#include "backend.hpp"
#include "backend_factory.hpp"
#include "bytes.hpp"
#include "fixed_reorder_buffer.hpp"
#include "packet_admission.hpp"
#include "packet_parse.hpp"
#include "socket_runtime.hpp"
#include "socket_rx_single_media_backend_base.hpp"
#include "video_receive_pipeline.hpp"
#include "video_timestamp_mapping.hpp"

#include <memory>
#include <optional>
#include <vector>

namespace st2110 {
class SocketRxVideoBackend final : public SocketRxSingleMediaBackendBase, public IRxVideoBackend {
  public:
    SocketRxVideoBackend()
        : SocketRxSingleMediaBackendBase(RxMediaKind::Video, RxBackendCapabilities{.video_rx = true, .audio_rx = false},
                                         make_default_port_factory()) {}

    explicit SocketRxVideoBackend(std::unique_ptr<ISocketRxPortFactory> port_factory)
        : SocketRxSingleMediaBackendBase(RxMediaKind::Video, RxBackendCapabilities{.video_rx = true, .audio_rx = false},
                                         std::move(port_factory)) {}

    RxBackendLifecycleResult start_video(const RxVideoConfig &cfg, IVideoFrameSink &sink) override {
        if (Error err = validate_common_start_preconditions(); err != Error::Ok) {
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

  protected:
    void clear_media_runtime_objects() noexcept override {
        clear_common_runtime_objects();

        reorder_buffer_.reset();
        video_receive_pipeline_.reset();
        video_timestamp_mapper_.reset();
        packet_parse_policy_ = {};
        video_sink_ = nullptr;
        configured_video_payload_type_.reset();
    }

    void process_received_datagram(ByteSpan udp_payload) noexcept override {
        if (reorder_buffer_ == nullptr || video_receive_pipeline_ == nullptr || !configured_video_payload_type_) {
            return;
        }

        if (is_rtcp_like_datagram(udp_payload)) {
            record_ignored_control_datagram();
            return;
        }

        auto packet = parse_packet_view_staged(udp_payload);
        if (!packet) {
            record_rejected_packet(packet.error().error, packet.error().stage);
            return;
        }

        if (Error err = validate_video_packet_payload_type_admission(*packet, *configured_video_payload_type_);
            err != Error::Ok) {
            record_ignored_nonmedia_datagram();
            return;
        }

        record_parsed_packet_ok();
        reorder_buffer_->push(*packet);
        drain_reorder_buffer_to_sink();
    }

    void augment_stats_snapshot_locked(BackendStats &snapshot) const noexcept override {
        if (reorder_buffer_ != nullptr) {
            snapshot.reorder = reorder_buffer_->stats();
        }

        if (video_receive_pipeline_ != nullptr) {
            snapshot.depacketizer = video_receive_pipeline_->depacketizer_stats();
        }
    }

  private:
    [[nodiscard]] static PacketParsePolicy build_packet_parse_policy(const RxVideoConfig &cfg) noexcept {
        return PacketParsePolicy{};
    }

    [[nodiscard]] static VideoReceivePipelineConfig build_video_receive_pipeline_config(const RxVideoConfig &cfg) {
        return {DepacketizerConfig{
                    .width = cfg.width,
                    .height = cfg.height,
                    .format = cfg.format,
                    .partial_frame_policy = PartialFramePolicy::Drop,
                    .scan_mode = cfg.scan_mode,
                    .packing_mode = cfg.packing_mode,
                },
                VideoUnitReconstructorConfig{.format = cfg.format, .scan_mode = cfg.scan_mode}};
    }

    [[nodiscard]] static std::unique_ptr<IReorderBuffer> make_reorder_buffer() {
        return std::make_unique<FixedWindowReorderBuffer>(32);
    }

    RxBackendLifecycleResult start_video_runtime(const RxVideoConfig &cfg, IVideoFrameSink &sink,
                                                 const SocketRxOpenConfig &open_cfg,
                                                 std::unique_ptr<ISocketRxPort> port) {
        PacketParsePolicy packet_parse_policy = build_packet_parse_policy(cfg);
        auto reorder_buffer = make_reorder_buffer();
        auto receive_buffer = make_receive_buffer(packet_parse_policy);

        std::unique_ptr<VideoReceivePipeline> video_receive_pipeline;
        std::optional<VideoRtpTimestampMapper> video_timestamp_mapper;

        try {
            video_receive_pipeline = std::make_unique<VideoReceivePipeline>(build_video_receive_pipeline_config(cfg));
            video_timestamp_mapper.emplace(VideoRtpTimestampMapperConfig{
                .rtp_clock_rate = 90000,
                .anchor_rtp_timestamp = 0,
                .anchor_timestamp_ns = 0,
            });
        } catch (const std::invalid_argument &) {
            return std::unexpected(Error::InvalidValue);
        } catch (const std::logic_error &) {
            return std::unexpected(Error::Unsupported);
        }

        packet_parse_policy_ = packet_parse_policy;
        reorder_buffer_ = std::move(reorder_buffer);
        video_receive_pipeline_ = std::move(video_receive_pipeline);
        video_timestamp_mapper_ = std::move(video_timestamp_mapper);
        video_sink_ = &sink;
        configured_video_payload_type_ = cfg.payload_type;

        auto started = start_common_runtime(std::move(port), open_cfg, std::move(receive_buffer));
        if (!started) {
            clear_media_runtime_objects();
            return std::unexpected(started.error());
        }

        return started;
    }

    void drain_reorder_buffer_to_sink() noexcept {
        if (reorder_buffer_ == nullptr || video_receive_pipeline_ == nullptr) {
            return;
        }

        while (true) {
            auto stored_packet = reorder_buffer_->pop_next();
            if (!stored_packet) {
                return;
            }

            auto frames = video_receive_pipeline_->push(stored_packet->view());
            for (auto &frame : frames) {
                deliver_reconstructed_frame(std::move(frame));
            }
        }
    }
    void deliver_reconstructed_frame(ReconstructedVideoFrame &&frame) noexcept {
        if (video_sink_ == nullptr || frame.partial()) {
            return;
        }

        const TimestampNs timestamp_ns = map_frame_timestamp_ns(frame.rtp_timestamp);
        video_sink_->on_video_frame(frame.frame.view(timestamp_ns));
        record_delivered_video_frame();
        record_delivered_media_unit();
    }

    [[nodiscard]] TimestampNs map_frame_timestamp_ns(uint32_t rtp_timestamp) noexcept {
        if (!video_timestamp_mapper_.has_value()) {
            return 0;
        }

        auto mapped = video_timestamp_mapper_->map(rtp_timestamp);
        if (!mapped.has_value()) {
            return 0;
        }

        return *mapped;
    }

    [[nodiscard]] static std::expected<SocketRxOpenConfig, Error> build_open_config(const RxVideoConfig &cfg) {
        auto res = socket_rx_open_config_from_video_config(cfg);
        if (!res) {
            return std::unexpected(res.error());
        }

        return res;
    }

    std::unique_ptr<IReorderBuffer> reorder_buffer_{};
    std::unique_ptr<VideoReceivePipeline> video_receive_pipeline_{};
    std::optional<VideoRtpTimestampMapper> video_timestamp_mapper_{};
    PacketParsePolicy packet_parse_policy_{};
    IVideoFrameSink *video_sink_ = nullptr;
    std::optional<std::uint8_t> configured_video_payload_type_{};
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

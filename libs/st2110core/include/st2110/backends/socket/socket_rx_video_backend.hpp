#ifndef ST2110_OBS_SOCKET_RX_VIDEO_BACKEND_HPP
#define ST2110_OBS_SOCKET_RX_VIDEO_BACKEND_HPP

#include "platform/socket_runtime.hpp"
#include "socket_rx_single_media_backend_base.hpp"
#include "st2110/contracts/backend/backend.hpp"
#include "st2110/contracts/backend/backend_factory.hpp"
#include "st2110/contracts/video/video_receiver_bootstrap.hpp"
#include "st2110/foundation/bytes.hpp"
#include "st2110/ingress/shared/packet_parse.hpp"
#include "st2110/receive/shared/fixed_reorder_buffer.hpp"
#include "st2110/receive/shared/packet_admission.hpp"
#include "st2110/receive/shared/receive_reorder_tolerance_policy.hpp"
#include "st2110/receive/video/video_receive_pipeline.hpp"
#include "st2110/receive/video/video_reorder_policy.hpp"
#include "st2110/receive/video/video_timestamp_mapping.hpp"
#include "st2110/rx_config.hpp"
#include "st2110/video_backend_support.hpp"
#include <st2110/delivery/video/socket_video_start_config.hpp>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace st2110 {
class SocketRxVideoBackend final : public SocketRxSingleMediaBackendBase, public IRxVideoBackend {
  public:
    explicit SocketRxVideoBackend(SocketVideoStartConfig cfg)
        : SocketRxSingleMediaBackendBase(make_default_port_factory()), cfg_(std::move(cfg)) {}

    RxBackendLifecycleResult start_video(IVideoFrameSink &sink) override {
        auto port = create_port();
        if (port == nullptr) {
            return std::unexpected(Error::InvalidValue);
        }

        return start_video_runtime(sink, std::move(port));
    }

  protected:
    void clear_media_runtime_objects() noexcept override {
        clear_common_runtime_objects();

        reorder_buffer_.reset();
        video_receive_pipeline_.reset();
        video_timestamp_mapper_.reset();
        maxudp_ = 0;
        reorder_tolerance_policy_ = {};
        video_sink_ = nullptr;
    }

    void process_received_datagram(ByteSpan udp_payload) noexcept override {
        if (reorder_buffer_ == nullptr || video_receive_pipeline_ == nullptr) {
            return;
        }

        if (is_rtcp_like_datagram(udp_payload)) {
            record_ignored_control_datagram();
            return;
        }

        if (const Error err = validate_packet_parse_policy(udp_payload, maxudp_); err != Error::Ok) {
            record_rejected_packet(err, PacketParseStage::PacketPolicy);
            return;
        }

        auto packet = parse_packet_view_staged(udp_payload);
        if (!packet) {
            record_rejected_packet(packet.error().error, packet.error().stage);
            return;
        }

        if (packet->rtp.payload_type != cfg_.stream.expected_payload_type) {
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

    [[nodiscard]] std::unique_ptr<IReorderBuffer> make_reorder_buffer(const ReorderBufferConfig &cfg) override {
        return std::make_unique<FixedWindowReorderBuffer<true>>(cfg.window_size_packets);
    }

    void deliver_media(std::unique_ptr<StoredPacket> packet) override {
        auto base = packet->view();
        auto video_packet = std::unique_ptr<VideoPacketView>(static_cast<VideoPacketView *>(base.release()));

        auto frames = video_receive_pipeline_->push(std::move(video_packet));
        for (auto &frame : frames) {
            deliver_reconstructed_frame(std::move(frame));
        }
    }

  private:
    RxBackendLifecycleResult start_video_runtime(IVideoFrameSink &sink, std::unique_ptr<ISocketRxPort> port) {
        auto reorder_buffer = make_reorder_buffer(cfg_.reorder_buffer_config);
        auto receive_buffer = make_receive_buffer(cfg_.legs[0].max_udp_datagram_bytes);

        std::unique_ptr<VideoReceivePipeline> video_receive_pipeline =
            std::make_unique<VideoReceivePipeline>(cfg_.video_receive_pipeline_config);
        std::optional<VideoRtpTimestampMapper> video_timestamp_mapper;
        video_timestamp_mapper.emplace(cfg_.rtp_timestamp_mapper_config);

        maxudp_ = cfg_.legs[0].max_udp_datagram_bytes;
        reorder_tolerance_policy_ = cfg_.reorder_buffer_config.reorder_tolerance_policy;
        reorder_buffer_ = std::move(reorder_buffer);
        video_receive_pipeline_ = std::move(video_receive_pipeline);
        video_timestamp_mapper_ = std::move(video_timestamp_mapper);
        video_sink_ = &sink;

        auto started = start_common_runtime(std::move(port), cfg_.open_config, std::move(receive_buffer));
        if (!started) {
            clear_media_runtime_objects();
            return std::unexpected(started.error());
        }

        return started;
    }

    void deliver_reconstructed_frame(ReconstructedVideoFrame &&frame) noexcept {
        if (video_sink_ == nullptr || frame.partial()) {
            return;
        }

        const TimestampNs timestamp_ns = map_frame_timestamp_ns(frame.rtp_timestamp);
        video_sink_->on_video_frame(frame.frame.view(timestamp_ns));
        {
            std::lock_guard lock(stats_mutex_);
            ++stats_.frames_delivered;
        }
        record_delivered_media_unit();
    }

    [[nodiscard]] TimestampNs map_frame_timestamp_ns(const std::uint32_t rtp_timestamp) noexcept {
        if (!video_timestamp_mapper_.has_value()) {
            return 0;
        }

        const auto mapped = video_timestamp_mapper_->map(rtp_timestamp);
        if (!mapped.has_value()) {
            return 0;
        }

        return *mapped;
    }

    SocketVideoStartConfig cfg_;
    std::unique_ptr<VideoReceivePipeline> video_receive_pipeline_{};
    std::optional<VideoRtpTimestampMapper> video_timestamp_mapper_{};
    std::size_t maxudp_{};
    IVideoFrameSink *video_sink_ = nullptr;
};
} // namespace st2110

#endif // ST2110_OBS_SOCKET_RX_VIDEO_BACKEND_HPP
#ifndef ST2110_OBS_SOCKET_RX_VIDEO_BACKEND_HPP
#define ST2110_OBS_SOCKET_RX_VIDEO_BACKEND_HPP

#include "platform/socket_runtime.hpp"
#include "socket_rx_single_media_backend_base.hpp"
#include "st2110/contracts/backend/backend.hpp"
#include "st2110/foundation/bytes.hpp"
#include "st2110/ingress/shared/packet_parse.hpp"
#include "st2110/receive/shared/fixed_reorder_buffer.hpp"
#include "st2110/receive/shared/packet_admission.hpp"
#include "st2110/receive/shared/receive_reorder_tolerance_policy.hpp"
#include "st2110/receive/video/video_receive_pipeline.hpp"
#include "st2110/receive/video/video_timestamp_mapping.hpp"
#include <st2110/delivery/video/socket_video_start_config.hpp>

#include <memory>
#include <utility>
#include <vector>

namespace st2110 {
class SocketRxVideoBackend final : public SocketRxSingleMediaBackendBase {
  public:
    explicit SocketRxVideoBackend(const SocketVideoStartConfig &cfg)
        : SocketRxSingleMediaBackendBase(make_default_port_factory(), cfg.legs[0].max_udp_datagram_bytes,
                                         cfg.reorder_buffer_config, collect_open_configs(cfg),
                                         cfg.stream.expected_payload_type),
          video_receive_pipeline_(cfg.video_receive_pipeline_config),
          video_timestamp_mapper_(cfg.rtp_timestamp_mapper_config) {}

    RxBackendLifecycleResult start(IFrameSink *sink) override {
        reorder_buffer_ = make_reorder_buffer(reorder_buffer_config_);
        return start_common_runtime(sink);
    }

    ~SocketRxVideoBackend() override { (void)SocketRxSingleMediaBackendBase::stop(); }

  protected:
    std::expected<std::unique_ptr<PacketView>, Error> parse_packet(const std::size_t leg_index,
                                                                   const ByteSpan udp_payload) override {
        (void)leg_index;

        auto packet = parse_packet_view(udp_payload, maxudp_);
        if (!packet) {
            return std::unexpected(packet.error());
        }

        return std::make_unique<VideoPacketView>(*packet);
    }

    [[nodiscard]] std::unique_ptr<IReorderBuffer> make_reorder_buffer(const ReorderBufferConfig &cfg) override {
        return std::make_unique<FixedWindowReorderBuffer<true>>(cfg.window_size_packets);
    }

    void deliver_media(const std::unique_ptr<StoredPacket> packet) override {
        auto base = packet->view();
        auto video_packet = std::unique_ptr<VideoPacketView>(static_cast<VideoPacketView *>(base.release()));

        auto frames = video_receive_pipeline_.push(std::move(video_packet));
        for (auto &frame : frames) {
            deliver_reconstructed_frame(std::move(frame));
        }
    }

  private:
    static std::vector<SocketRxOpenConfig> collect_open_configs(const SocketVideoStartConfig &cfg) {
        std::vector<SocketRxOpenConfig> open_configs;
        open_configs.reserve(cfg.legs.size());
        for (const auto &leg : cfg.legs) {
            open_configs.emplace_back(leg.open_config);
        }

        return open_configs;
    }

    void deliver_reconstructed_frame(ReconstructedVideoFrame &&frame) noexcept {
        const TimestampNs timestamp_ns = map_frame_timestamp_ns(frame.rtp_timestamp);
        sink_->on_video_frame(frame.frame, timestamp_ns);
    }

    [[nodiscard]] TimestampNs map_frame_timestamp_ns(const std::uint32_t rtp_timestamp) noexcept {
        const auto mapped = video_timestamp_mapper_.map(rtp_timestamp);
        if (!mapped.has_value()) {
            return 0;
        }

        return *mapped;
    }

    VideoReceivePipeline video_receive_pipeline_;
    VideoRtpTimestampMapper video_timestamp_mapper_;
};
} // namespace st2110

#endif // ST2110_OBS_SOCKET_RX_VIDEO_BACKEND_HPP
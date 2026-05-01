#include "st2110/socket_rx_video_backend.hpp"
#include "st2110/socket_stub_rx_port.hpp"

#if defined(__linux__)
#include "st2110/linux_socket_rx_port.hpp"
#endif

namespace st2110 {
std::unique_ptr<ISocketRxPortFactory> SocketRxVideoBackend::make_default_port_factory() {
#if defined(__linux__)
    return make_linux_socket_rx_port_factory();
#else
    return make_socket_stub_rx_port_factory();
#endif
}

PacketParsePolicy SocketRxVideoBackend::build_packet_parse_policy(const RxVideoConfig &cfg) noexcept {
    return PacketParsePolicy{};
}

VideoReceivePipelineConfig SocketRxVideoBackend::build_video_receive_pipeline_config(const RxVideoConfig &cfg) {
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

std::unique_ptr<IReorderBuffer> SocketRxVideoBackend::make_reorder_buffer() {
    return std::make_unique<FixedWindowReorderBuffer>(32);
}

std::vector<std::uint8_t> SocketRxVideoBackend::make_receive_buffer(const PacketParsePolicy &policy) {
    const std::size_t size_bytes = effective_max_udp_datagram_bytes(policy) - udpHeaderBytes;
    return std::vector<std::uint8_t>(size_bytes);
}

BackendStats SocketRxVideoBackend::stats() const {
    std::lock_guard lock(stats_mutex_);
    return build_stats_snapshot_locked();
}

void SocketRxVideoBackend::record_received_datagram(std::size_t size_bytes) noexcept {
    std::lock_guard lock(stats_mutex_);
    ++stats_.datagrams_received;
    stats_.bytes_received += size_bytes;
}

void SocketRxVideoBackend::record_ignored_control_datagram() noexcept {
    std::lock_guard lock(stats_mutex_);
    ++stats_.control_datagrams_ignored;
    ++stats_.datagrams_dropped;
}

void SocketRxVideoBackend::record_ignored_nonmedia_datagram() noexcept {
    std::lock_guard lock(stats_mutex_);
    ++stats_.nonmedia_datagrams_ignored;
    ++stats_.datagrams_dropped;
}

void SocketRxVideoBackend::record_rejected_packet(Error err, PacketParseStage stage) noexcept {
    std::lock_guard lock(stats_mutex_);
    ++stats_.packets_rejected;
    ++stats_.datagrams_dropped;
    record_packet_parse_result(stats_.packet_parse, err, stage);
}

void SocketRxVideoBackend::record_parsed_packet_ok() noexcept {
    std::lock_guard lock(stats_mutex_);
    ++stats_.packets_parsed_ok;
    record_packet_parse_result(stats_.packet_parse, Error::Ok, PacketParseStage::RtpHeader);
}

void SocketRxVideoBackend::record_delivered_frame() noexcept {
    std::lock_guard lock(stats_mutex_);
    ++stats_.frames_delivered;
    ++stats_.media_units_delivered;
}

BackendStats SocketRxVideoBackend::build_stats_snapshot_locked() const noexcept {
    BackendStats snapshot = stats_;

    if (reorder_buffer_ != nullptr) {
        snapshot.reorder = reorder_buffer_->stats();
    }

    if (video_receive_pipeline_ != nullptr) {
        snapshot.depacketizer = video_receive_pipeline_->depacketizer_stats();
    }

    return snapshot;
}

RxBackendLifecycleResult SocketRxVideoBackend::start_video_runtime(const RxVideoConfig &cfg, IVideoFrameSink &sink,
                                                                   const SocketRxOpenConfig &open_cfg,
                                                                   std::unique_ptr<ISocketRxPort> port) {
    PacketParsePolicy packet_parse_policy = build_packet_parse_policy(cfg);
    auto reorder_buffer = make_reorder_buffer();
    auto receive_buffer = make_receive_buffer(packet_parse_policy);

    std::unique_ptr<VideoReceivePipeline> video_receive_pipeline;
    std::optional<VideoRtpTimestampMapper> video_timestamp_mapper;

    auto video_receive_pipeline_cfg = build_video_receive_pipeline_config(cfg);
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

    if (Error err = port->open(open_cfg); err != Error::Ok) {
        return std::unexpected(err);
    }

    try {
        port_ = std::move(port);
        packet_parse_policy_ = packet_parse_policy;
        reorder_buffer_ = std::move(reorder_buffer);
        receive_buffer_ = std::move(receive_buffer);
        video_receive_pipeline_ = std::move(video_receive_pipeline);
        video_timestamp_mapper_ = std::move(video_timestamp_mapper);
        video_sink_ = &sink;
        configured_video_payload_type_ = cfg.payload_type;

        {
            std::lock_guard lock(stats_mutex_);
            stats_ = {};
        }

        receive_thread_ = std::jthread([this](std::stop_token stop_token) {
            run_video_receive_loop(stop_token);
        });
    } catch (...) {
        if (port_ && port_->is_open()) {
            (void)port_->close();
        }
        clear_runtime_objects();
        return std::unexpected(Error::SystemFailure);
    }

    state_.video_active = true;
    return state_;
}

void SocketRxVideoBackend::run_video_receive_loop(std::stop_token stop_token) noexcept {
    if (port_ == nullptr || video_receive_pipeline_ == nullptr || video_sink_ == nullptr || receive_buffer_.empty()) {
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

        process_received_datagram(
            ByteSpan(receive_buffer_.data(), received->size_bytes));
    }
}

[[nodiscard]] bool is_rtcp_like_datagram(ByteSpan udp_payload) noexcept {
    if (udp_payload.size() < 2) {
        return false;
    }

    const std::uint8_t version = static_cast<std::uint8_t>((udp_payload[0] >> 6) & 0x03u);
    if (version != 2u) {
        return false;
    }

    const std::uint8_t payload_type = udp_payload[1];
    return payload_type >= 192u && payload_type <= 223u;
}

[[nodiscard]] bool datagram_matches_configured_payload_type(ByteSpan udp_payload,
                                                            std::uint8_t configured_payload_type) noexcept {
    if (udp_payload.size() < 2) {
        return false;
    }

    const std::uint8_t version = static_cast<std::uint8_t>((udp_payload[0] >> 6) & 0x03u);
    if (version != 2u) {
        return false;
    }

    const std::uint8_t payload_type = static_cast<std::uint8_t>(udp_payload[1] & 0x7Fu);
    return payload_type == configured_payload_type;
}

void SocketRxVideoBackend::process_received_datagram(ByteSpan udp_payload) noexcept {
    if (reorder_buffer_ == nullptr || video_receive_pipeline_ == nullptr || !configured_video_payload_type_) {
        return;
    }

    if (is_rtcp_like_datagram(udp_payload)) {
        record_ignored_control_datagram();
        return;
    }

    if (!datagram_matches_configured_payload_type(udp_payload, *configured_video_payload_type_)) {
        record_ignored_nonmedia_datagram();
        return;
    }

    auto packet = parse_packet_view_staged(udp_payload);
    if (!packet) {
        record_rejected_packet(packet.error().error, packet.error().stage);
        return;
    }

    record_parsed_packet_ok();
    reorder_buffer_->push(*packet);
    drain_reorder_buffer_to_sink();
}

void SocketRxVideoBackend::drain_reorder_buffer_to_sink() noexcept {
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

void SocketRxVideoBackend::deliver_reconstructed_frame(ReconstructedVideoFrame &&frame) noexcept {
    if (video_sink_ == nullptr || frame.partial()) {
        return;
    }

    const TimestampNs timestamp_ns = map_frame_timestamp_ns(frame.rtp_timestamp);
    video_sink_->on_video_frame(frame.frame.view(timestamp_ns));
    record_delivered_frame();
}

TimestampNs SocketRxVideoBackend::map_frame_timestamp_ns(uint32_t rtp_timestamp) noexcept {
    if (!video_timestamp_mapper_.has_value()) {
        return 0;
    }

    auto mapped = video_timestamp_mapper_->map(rtp_timestamp);
    if (!mapped.has_value()) {
        return 0;
    }

    return *mapped;
}

} // namespace st2110
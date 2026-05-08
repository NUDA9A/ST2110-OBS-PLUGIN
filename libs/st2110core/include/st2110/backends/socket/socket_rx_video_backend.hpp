#ifndef ST2110_OBS_SOCKET_RX_VIDEO_BACKEND_HPP
#define ST2110_OBS_SOCKET_RX_VIDEO_BACKEND_HPP

#include "st2110/contracts/backend/backend.hpp"
#include "st2110/contracts/backend/backend_factory.hpp"
#include "st2110/foundation/bytes.hpp"
#include "st2110/ingress/shared/packet_parse.hpp"
#include "st2110/receive/shared/fixed_reorder_buffer.hpp"
#include "st2110/receive/shared/packet_admission.hpp"
#include "st2110/receive/video/video_receive_pipeline.hpp"
#include "st2110/receive/video/video_reorder_policy.hpp"
#include "st2110/receive/video/video_timestamp_mapping.hpp"
#include "st2110/rx_config.hpp"
#include "st2110/signaling_structs.hpp"
#include "socket_rx_single_media_backend_base.hpp"
#include "st2110/video_backend_support.hpp"
#include "platform/socket_runtime.hpp"
#include "st2110/receive/shared/receive_reorder_tolerance_policy.hpp"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace st2110 {
struct SocketRxVideoOperationalConfig {
    SocketRxOperationalCommonConfig common{};
    RxVideoConfig rx_config{};
    VideoReceivePipelineConfig receive_pipeline_config{};
    VideoRtpTimestampMapperConfig timestamp_mapper_config{};
    VideoReorderBufferConfig reorder_buffer_config{};
};

struct SocketRxVideoSupportPolicy {
    /*
     * Current socket video implementation-support boundary.
     *
     * These switches describe actual current support of the project socket
     * receive path, not structural recognition of common video capability.
     */
    bool require_current_socket_receive_pipeline_storage_support = true;
    bool require_socket_runtime_packing_mode_support = true;
    bool require_progressive_scan_mode = true;
    bool require_single_stream_topology = true;
    bool require_90khz_rtp_clock = true;
};

[[nodiscard]] inline SocketRxVideoSupportPolicy default_socket_rx_video_support_policy() {
    return SocketRxVideoSupportPolicy{
        .require_current_socket_receive_pipeline_storage_support = true,
        .require_socket_runtime_packing_mode_support = true,
        .require_progressive_scan_mode = true,
        .require_single_stream_topology = true,
        .require_90khz_rtp_clock = true,
    };
}

[[nodiscard]] inline Error validate_socket_rx_video_scan_mode_implementation_support(VideoScanMode mode) noexcept {
    if (mode != VideoScanMode::Progressive) {
        return Error::Unsupported;
    }

    return Error::Ok;
}

[[nodiscard]] inline Error
validate_socket_rx_video_topology_implementation_support(const VideoReceiveTopology &topology) noexcept {
    if (Error err = validate_video_receive_topology(topology); err != Error::Ok) {
        return err;
    }

    if (topology.kind != VideoReceiveTopologyKind::SingleStream || topology.stream_count != 1) {
        return Error::Unsupported;
    }

    return Error::Ok;
}

[[nodiscard]] inline Error
validate_socket_rx_video_rtp_clock_implementation_support(const VideoReceiveRtpClock &rtp_clock) noexcept {
    if (Error err = validate_video_receive_rtp_clock(rtp_clock); err != Error::Ok) {
        return err;
    }

    if (rtp_clock.rtp_clock_rate != 90000) {
        return Error::Unsupported;
    }

    return Error::Ok;
}

/*
 * Current socket receive-pipeline support boundary:
 * depacketizer + reconstructor + current VideoFrame storage/handoff path.
 *
 * This helper is intentionally backend-local even if it reuses project frame
 * compatibility helpers internally, because it models what the socket backend
 * can actually deliver today.
 */
[[nodiscard]] inline Error validate_socket_rx_video_receive_pipeline_storage_implementation_support(
    const CommonVideoBackendSupportMatrix &matrix) {
    if (Error err = validate_common_video_backend_support_matrix(matrix); err != Error::Ok) {
        return err;
    }

    if (Error err = validate_project_video_frame_handoff_format_matches_pixel_format(
            matrix.receive_capability.handoff_format, matrix.project_pixel_format);
        err != Error::Ok) {
        return err;
    }

    if (Error err =
            validate_project_video_frame_storage_compatibility(matrix.receive_capability, matrix.project_pixel_format);
        err != Error::Ok) {
        return err;
    }

    return Error::Ok;
}

[[nodiscard]] inline Error
validate_socket_rx_video_receive_capability_implementation_support(const VideoReceiveCapability &capability,
                                                                   const SocketRxVideoSupportPolicy &support) {
    if (Error err = validate_video_receive_capability_structure(capability); err != Error::Ok) {
        return err;
    }

    if (support.require_progressive_scan_mode) {
        if (Error err = validate_socket_rx_video_scan_mode_implementation_support(capability.scan_mode);
            err != Error::Ok) {
            return err;
        }
    }

    if (support.require_single_stream_topology) {
        if (Error err = validate_socket_rx_video_topology_implementation_support(capability.topology);
            err != Error::Ok) {
            return err;
        }
    }

    if (support.require_90khz_rtp_clock) {
        if (Error err = validate_socket_rx_video_rtp_clock_implementation_support(capability.rtp_clock);
            err != Error::Ok) {
            return err;
        }
    }

    return Error::Ok;
}

[[nodiscard]] inline Error
validate_socket_rx_video_backend_support_matrix_implementation_support(const CommonVideoBackendSupportMatrix &matrix,
                                                                       const SocketRxVideoSupportPolicy &support) {
    if (Error err = validate_common_video_backend_support_matrix(matrix); err != Error::Ok) {
        return err;
    }

    if (support.require_current_socket_receive_pipeline_storage_support) {
        if (Error err = validate_socket_rx_video_receive_pipeline_storage_implementation_support(matrix);
            err != Error::Ok) {
            return err;
        }
    }

    return validate_socket_rx_video_receive_capability_implementation_support(matrix.receive_capability, support);
}

/*
 * Shared-first Socket support entry point:
 * 1) resolve/validate the common video backend-support matrix;
 * 2) apply only Socket-local implementation-support policy.
 *
 * This helper must not call project-delivery support validation directly.
 */
[[nodiscard]] inline Error validate_socket_rx_video_config_support(const RxVideoConfig &cfg,
                                                                   const SocketRxVideoSupportPolicy &support) {
    auto matrix = common_video_backend_support_matrix_from_rx_video_config(cfg);
    if (!matrix.has_value()) {
        return matrix.error();
    }

    return validate_socket_rx_video_backend_support_matrix_implementation_support(*matrix, support);
}

[[nodiscard]] inline Error validate_socket_rx_video_operational_config(const SocketRxVideoOperationalConfig &cfg) {
    if (const Error err = validate_video_reorder_buffer_config(cfg.reorder_buffer_config); err != Error::Ok) {
        return err;
    }

    if (const Error err = validate_socket_rx_operational_common_config(cfg.common); err != Error::Ok) {
        return err;
    }

    if (const Error err =
            validate_socket_rx_video_config_support(cfg.rx_config, default_socket_rx_video_support_policy());
        err != Error::Ok) {
        return err;
    }

    if (const Error err = validate_video_rtp_timestamp_mapper_config(cfg.timestamp_mapper_config); err != Error::Ok) {
        return err;
    }

    auto capability = rx_video_config_effective_receive_capability(cfg.rx_config);
    if (!capability.has_value()) {
        return capability.error();
    }

    if (cfg.timestamp_mapper_config.rtp_clock_rate != capability->rtp_clock.rtp_clock_rate) {
        return Error::InvalidValue;
    }

    auto expected_open_config = socket_rx_open_config_from_video_config(cfg.rx_config);
    if (!expected_open_config) {
        return expected_open_config.error();
    }

    if (!socket_rx_open_config_equal(cfg.common.open_config, *expected_open_config)) {
        return Error::InvalidValue;
    }

    const auto &depacketizer = cfg.receive_pipeline_config.depacketizer;
    const auto &reconstructor = cfg.receive_pipeline_config.reconstructor;

    if (depacketizer.width != cfg.rx_config.width || depacketizer.height != cfg.rx_config.height ||
        depacketizer.format != cfg.rx_config.format || depacketizer.scan_mode != cfg.rx_config.scan_mode ||
        depacketizer.packing_mode != cfg.rx_config.packing_mode) {
        return Error::InvalidValue;
    }

    if (reconstructor.format != cfg.rx_config.format || reconstructor.scan_mode != cfg.rx_config.scan_mode) {
        return Error::InvalidValue;
    }

    if (reconstructor.format != depacketizer.format || reconstructor.scan_mode != depacketizer.scan_mode) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

[[nodiscard]] inline std::expected<SocketRxVideoOperationalConfig, Error>
socket_rx_video_operational_config_from_video_receiver_bootstrap(const VideoReceiverBootstrapConfig &bootstrap) {
    if (const Error err = validate_video_receiver_timing_config(bootstrap.timing_config); err != Error::Ok) {
        return std::unexpected(err);
    }

    if (const Error err =
            validate_socket_rx_video_config_support(bootstrap.rx_config, default_socket_rx_video_support_policy());
        err != Error::Ok) {
        return std::unexpected(err);
    }

    auto open_config = socket_rx_open_config_from_video_config(bootstrap.rx_config);
    if (!open_config) {
        return std::unexpected(open_config.error());
    }

    SocketRxVideoOperationalConfig operational{
        .common =
            SocketRxOperationalCommonConfig{
                .open_config = *open_config,
                .packet_parse_policy = bootstrap.packet_parse_policy,
            },
        .rx_config = bootstrap.rx_config,
        .receive_pipeline_config = bootstrap.receive_pipeline_config,
        .timestamp_mapper_config = bootstrap.timestamp_mapper_config,
        .reorder_buffer_config = bootstrap.reorder_buffer_config,
    };

    if (const Error err = validate_socket_rx_video_operational_config(operational); err != Error::Ok) {
        return std::unexpected(err);
    }

    return operational;
}

[[nodiscard]] inline std::expected<SocketRxVideoOperationalConfig, Error>
socket_rx_video_operational_config_from_rx_video_config(const RxVideoConfig &cfg,
                                                        const PacketParsePolicy &packet_parse_policy,
                                                        PartialFramePolicy partial_frame_policy,
                                                        const VideoRtpTimestampMapperConfig &timestamp_mapper_config,
                                                        const VideoReorderBufferConfig &reorder_buffer_config = {}) {
    if (const Error err = validate_socket_rx_video_config_support(cfg, default_socket_rx_video_support_policy());
        err != Error::Ok) {
        return std::unexpected(err);
    }

    auto open_config = socket_rx_open_config_from_video_config(cfg);
    if (!open_config) {
        return std::unexpected(open_config.error());
    }

    SocketRxVideoOperationalConfig operational{
        .common =
            SocketRxOperationalCommonConfig{
                .open_config = *open_config,
                .packet_parse_policy = packet_parse_policy,
            },
        .rx_config = cfg,
        .receive_pipeline_config =
            VideoReceivePipelineConfig{
                .depacketizer =
                    DepacketizerConfig{
                        .width = cfg.width,
                        .height = cfg.height,
                        .format = cfg.format,
                        .partial_frame_policy = partial_frame_policy,
                        .scan_mode = cfg.scan_mode,
                        .packing_mode = cfg.packing_mode,
                    },
                .reconstructor =
                    VideoUnitReconstructorConfig{
                        .format = cfg.format,
                        .scan_mode = cfg.scan_mode,
                    },
            },
        .timestamp_mapper_config = timestamp_mapper_config,
        .reorder_buffer_config = reorder_buffer_config,
    };

    if (const Error err = validate_socket_rx_video_operational_config(operational); err != Error::Ok) {
        return std::unexpected(err);
    }

    return operational;
}

class SocketRxVideoBackend final : public SocketRxSingleMediaBackendBase, public ISocketRxVideoBackend {
  public:
    SocketRxVideoBackend()
        : SocketRxSingleMediaBackendBase(RxMediaKind::Video, RxBackendCapabilities{.video_rx = true, .audio_rx = false},
                                         make_default_port_factory()) {}

    explicit SocketRxVideoBackend(std::unique_ptr<ISocketRxPortFactory> port_factory)
        : SocketRxSingleMediaBackendBase(RxMediaKind::Video, RxBackendCapabilities{.video_rx = true, .audio_rx = false},
                                         std::move(port_factory)) {}

    RxBackendLifecycleResult start_video(const SocketRxVideoOperationalConfig &cfg, IVideoFrameSink &sink) override {
        if (Error err = validate_common_start_preconditions(); err != Error::Ok) {
            return std::unexpected(err);
        }

        if (Error err = validate_socket_rx_video_operational_config(cfg); err != Error::Ok) {
            return std::unexpected(err);
        }

        auto port = create_port();
        if (port == nullptr) {
            return std::unexpected(Error::InvalidValue);
        }

        return start_video_runtime(cfg, sink, std::move(port));
    }

  protected:
    void clear_media_runtime_objects() noexcept override {
        clear_common_runtime_objects();

        reorder_buffer_.reset();
        video_receive_pipeline_.reset();
        video_timestamp_mapper_.reset();
        packet_parse_policy_ = {};
        reorder_tolerance_policy_ = {};
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

        if (const Error err = validate_packet_parse_policy(udp_payload, packet_parse_policy_); err != Error::Ok) {
            record_rejected_packet(err, PacketParseStage::PacketPolicy);
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
    [[nodiscard]] static std::unique_ptr<IReorderBuffer> make_reorder_buffer(const VideoReorderBufferConfig &cfg) {
        return std::make_unique<FixedWindowReorderBuffer>(cfg.window_size_packets);
    }

    RxBackendLifecycleResult start_video_runtime(const SocketRxVideoOperationalConfig &cfg, IVideoFrameSink &sink,
                                                 std::unique_ptr<ISocketRxPort> port) {
        auto reorder_buffer = make_reorder_buffer(cfg.reorder_buffer_config);
        auto receive_buffer = make_receive_buffer(cfg.common.packet_parse_policy);

        std::unique_ptr<VideoReceivePipeline> video_receive_pipeline;
        std::optional<VideoRtpTimestampMapper> video_timestamp_mapper;

        try {
            video_receive_pipeline = std::make_unique<VideoReceivePipeline>(cfg.receive_pipeline_config);
            video_timestamp_mapper.emplace(cfg.timestamp_mapper_config);
        } catch (const std::invalid_argument &) {
            return std::unexpected(Error::InvalidValue);
        } catch (const std::logic_error &) {
            return std::unexpected(Error::Unsupported);
        }

        packet_parse_policy_ = cfg.common.packet_parse_policy;
        reorder_tolerance_policy_ = cfg.reorder_buffer_config.reorder_tolerance_policy;
        reorder_buffer_ = std::move(reorder_buffer);
        video_receive_pipeline_ = std::move(video_receive_pipeline);
        video_timestamp_mapper_ = std::move(video_timestamp_mapper);
        video_sink_ = &sink;
        configured_video_payload_type_ = cfg.rx_config.payload_type;

        auto started = start_common_runtime(std::move(port), cfg.common.open_config, std::move(receive_buffer));
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

        bool gap_flush_used = false;

        while (true) {
            auto stored_packet = reorder_buffer_->pop_next();
            if (!stored_packet) {
                if (gap_flush_used) {
                    return;
                }

                if (!receive_reorder_policy_allows_gap_flush_once(reorder_tolerance_policy_)) {
                    return;
                }

                if (!reorder_buffer_->flush_missing_once()) {
                    return;
                }

                gap_flush_used = true;
                continue;
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
        {
            std::lock_guard lock(stats_mutex_);
            ++stats_.frames_delivered;
        }
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

    std::unique_ptr<IReorderBuffer> reorder_buffer_{};
    std::unique_ptr<VideoReceivePipeline> video_receive_pipeline_{};
    std::optional<VideoRtpTimestampMapper> video_timestamp_mapper_{};
    PacketParsePolicy packet_parse_policy_{};
    ReceiveReorderTolerancePolicy reorder_tolerance_policy_{};
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
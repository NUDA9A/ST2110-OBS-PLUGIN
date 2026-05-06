#ifndef ST2110_OBS_PLUGIN_MTL_RX_VIDEO_BACKEND_HPP
#define ST2110_OBS_PLUGIN_MTL_RX_VIDEO_BACKEND_HPP

#include "backend.hpp"
#include "rx_config.hpp"
#include "video_receive_capability.hpp"

#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <string>

namespace st2110 {

struct MtlRxVideoDeviceContext {};
struct MtlRxVideoSessionContext {};

struct MtlRxVideoSupportPolicy {
    VideoRuntimeSupportPolicy runtime_support{};
    bool require_progressive_scan_mode = true;
    bool require_single_stream_topology = true;
    bool require_90khz_rtp_clock = true;
    bool require_project_handoff_format_support = true;
};

[[nodiscard]] MtlRxVideoSupportPolicy default_mtl_rx_video_support_policy();

[[nodiscard]] Error validate_mtl_rx_video_receive_capability_support(const VideoReceiveCapability &capability,
                                                                     const MtlRxVideoSupportPolicy &support) noexcept;

[[nodiscard]] Error validate_mtl_rx_video_config_support(const RxVideoConfig &cfg,
                                                         const MtlRxVideoSupportPolicy &support);

class MtlRxVideoBackend final : public IRxVideoBackend {
  public:
    MtlRxVideoBackend() = default;
    ~MtlRxVideoBackend() override;

    MtlRxVideoBackend(const MtlRxVideoBackend &) = delete;
    MtlRxVideoBackend &operator=(const MtlRxVideoBackend &) = delete;
    MtlRxVideoBackend(MtlRxVideoBackend &&) = delete;
    MtlRxVideoBackend &operator=(MtlRxVideoBackend &&) = delete;

    [[nodiscard]] const char *backend_name() const override;
    RxBackendLifecycleResult start_video(const RxVideoConfig &cfg, IVideoFrameSink &sink) override;
    RxBackendLifecycleResult stop() override;
    [[nodiscard]] RxBackendState state() const override;
    [[nodiscard]] RxBackendCapabilities capabilities() const override;
    [[nodiscard]] BackendStats stats() const override;

  private:
    static constexpr std::uint16_t kDefaultFrameBufferCount = 3;
    static constexpr std::uint32_t kVideoRtpClockRate = 90000;

    struct ProjectedVideoStartConfig {
        VideoReceiveCapability receive_capability{};
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint32_t fps_num = 0;
        std::uint32_t fps_den = 1;
        std::uint8_t payload_type = 0;
        std::string local_ip{};
        std::string dest_ip{};
        PixelFormat pixel_format = PixelFormat::UYVY;
        VideoFrameHandoffFormat handoff_format = VideoFrameHandoffFormat::Uyvy;
        VideoTransportPayloadFormat transport_format = VideoTransportPayloadFormat::Rfc4175Ycbcr422_8Bit;
        VideoScanMode scan_mode = VideoScanMode::Progressive;
        VideoPackingMode packing_mode = VideoPackingMode::Gpm;
        bool mtl_interlaced = false;
        std::uint16_t session_port_count = 1;
        std::uint16_t frame_buffer_count = kDefaultFrameBufferCount;
    };

    [[nodiscard]] static Error
    validate_video_frame_view_compatibility(const RxVideoConfig &cfg,
                                            const VideoReceiveCapability &capability) noexcept;

    [[nodiscard]] static Error validate_mtl_st20p_mvp_compatibility(const RxVideoConfig &cfg);

    [[nodiscard]] static std::expected<bool, Error> scan_mode_maps_to_mtl_interlaced(VideoScanMode scan_mode) noexcept;

    [[nodiscard]] static std::expected<std::uint16_t, Error>
    session_port_count_from_receive_topology(const VideoReceiveTopology &topology) noexcept;

    [[nodiscard]] static std::expected<ProjectedVideoStartConfig, Error>
    project_video_start_config(const RxVideoConfig &cfg);

    [[nodiscard]] Error validate_start_state_locked() const noexcept;
    void reset_runtime_locked() noexcept;

    mutable std::mutex mutex_{};
    RxBackendState state_{};
    BackendStats stats_{};
    IVideoFrameSink *video_sink_ = nullptr;

    /*
     * Declaration order is intentional:
     * session_ must be releasable before device_ during reset/teardown.
     */
    std::unique_ptr<MtlRxVideoDeviceContext> device_{};
    std::unique_ptr<MtlRxVideoSessionContext> session_{};
};

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_MTL_RX_VIDEO_BACKEND_HPP
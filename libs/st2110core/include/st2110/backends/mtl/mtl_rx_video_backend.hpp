#ifndef ST2110_OBS_PLUGIN_MTL_RX_VIDEO_BACKEND_HPP
#define ST2110_OBS_PLUGIN_MTL_RX_VIDEO_BACKEND_HPP

#include "st2110/contracts/backend/backend.hpp"
#include "st2110/rx_config.hpp"
#include "st2110/video_backend_support.hpp"
#include "st2110/video_receive_capability.hpp"

#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace st2110 {

struct MtlRxVideoDeviceContext {};
struct MtlRxVideoSessionContext {};

struct MtlRxVideoSupportPolicy {
    /*
     * Current MTL video implementation-support boundary.
     *
     * These switches describe actual current support of the MTL session and
     * current project-to-MTL projection path, not structural recognition of
     * common video capability and not current VideoFrameView delivery limits.
     */
    bool require_mtl_session_packing_mode_support = true;
    bool require_progressive_scan_mode = false;
    bool require_single_stream_topology = true;
    bool require_90khz_rtp_clock = true;
    bool require_mtl_project_handoff_format_support = true;
};

[[nodiscard]] MtlRxVideoSupportPolicy default_mtl_rx_video_support_policy();

[[nodiscard]] Error validate_mtl_rx_video_session_packing_mode_implementation_support(VideoPackingMode mode) noexcept;

[[nodiscard]] Error validate_mtl_rx_video_receive_capability_session_implementation_support(
    const VideoReceiveCapability &capability, const MtlRxVideoSupportPolicy &support) noexcept;

[[nodiscard]] Error validate_mtl_rx_video_backend_support_matrix_project_projection_implementation_support(
    const CommonVideoBackendSupportMatrix &matrix, const MtlRxVideoSupportPolicy &support) noexcept;

[[nodiscard]] Error
validate_mtl_rx_video_backend_support_matrix_implementation_support(const CommonVideoBackendSupportMatrix &matrix,
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

    struct ProjectedCommonVideoConfig {
        VideoReceiveCapability receive_capability{};
        std::uint8_t payload_type = 0;
        std::string local_ip{};
        std::string dest_ip{};
        PixelFormat project_pixel_format = PixelFormat::UYVY;
        VideoFrameHandoffFormat handoff_format = VideoFrameHandoffFormat::Uyvy;
        VideoTransportPayloadFormat transport_format = VideoTransportPayloadFormat::Rfc4175Ycbcr422_8Bit;
        VideoScanMode scan_mode = VideoScanMode::Progressive;
        VideoPackingMode packing_mode = VideoPackingMode::Gpm;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint32_t fps_num = 0;
        std::uint32_t fps_den = 1;
        std::uint16_t session_port_count = 1;
    };

    enum class MtlDevicePortPmd {
        DpdkUser,
        NativeAfXdp,
        KernelSocket,
        DpdkAfXdp,
        DpdkAfPacket,
    };

    enum class MtlDevicePortNetProtocol {
        Static,
        Dhcp,
    };

    struct MtlDevicePortRuntimeConfig {
        std::string port_name{};
        std::string source_ip{};
        MtlDevicePortPmd pmd = MtlDevicePortPmd::DpdkUser;
        MtlDevicePortNetProtocol net_protocol = MtlDevicePortNetProtocol::Static;
        std::uint16_t rx_queue_count = 1;
    };

    struct MtlDeviceRuntimeConfig {
        MtlDevicePortRuntimeConfig primary_port{};
        std::optional<MtlDevicePortRuntimeConfig> redundant_port{};
        bool auto_start_stop = true;
        bool enable_hw_timestamp = false;
        int socket_id = -1;
        std::string lcores{};
    };

    struct MtlSessionRuntimeConfig {
        std::uint16_t primary_udp_port = 0;
        std::optional<std::uint16_t> redundant_udp_port{};
        std::uint16_t frame_buffer_count = kDefaultFrameBufferCount;
        bool enable_block_get = true;
        bool receive_incomplete_frame = false;
        bool enable_rtcp = false;
        bool enable_timing_parser_stat = false;
        bool enable_timing_parser_meta = false;
    };

    struct ProjectedMtlVideoSessionConfig {
        ProjectedCommonVideoConfig common{};
        MtlDeviceRuntimeConfig device_runtime{};
        MtlSessionRuntimeConfig session_runtime{};
        bool mtl_interlaced = false;
    };

    /*
     * Current project VideoFrameView delivery boundary only.
     * This is intentionally narrower than MTL session/project projection support
     * and must not be used as the backend support/projection acceptance boundary.
     */
    [[nodiscard]] static Error
    validate_video_frame_view_delivery_support(const RxVideoConfig &cfg,
                                               const VideoReceiveCapability &capability) noexcept;

    /*
     * Common-video projection acceptance boundary.
     * This validates that the generic config can be resolved into the common
     * receive-capability model that the MTL backend consumes locally.
     *
     * It must not apply backend-local device/session runtime defaults.
     * It must not apply current VideoFrameView delivery limits.
     */
    [[nodiscard]] static Error validate_projected_common_video_support(const RxVideoConfig &cfg);

    [[nodiscard]] static std::expected<bool, Error> scan_mode_maps_to_mtl_interlaced(VideoScanMode scan_mode) noexcept;

    [[nodiscard]] static std::expected<std::uint16_t, Error>
    session_port_count_from_receive_topology(const VideoReceiveTopology &topology) noexcept;

    /*
     * Stage 1 of the MTL-local projection boundary:
     * resolve RxVideoConfig into a backend-consumable common-video projection.
     */
    [[nodiscard]] static std::expected<ProjectedCommonVideoConfig, Error>
    project_common_video_config(const RxVideoConfig &cfg);

    /*
     * Named default runtime/device policy for callers that do not yet supply
     * backend-local runtime config explicitly.
     *
     * Defaults must stay explicit at the call site instead of being hidden in
     * the final st20p/session projection helper.
     */
    [[nodiscard]] static MtlDeviceRuntimeConfig default_mtl_device_runtime_config() noexcept;

    [[nodiscard]] static std::expected<MtlSessionRuntimeConfig, Error>
    default_mtl_session_runtime_config(const RxVideoConfig &cfg, const ProjectedCommonVideoConfig &common);

    /*
     * Stage 2 of the MTL-local projection boundary:
     * combine already-projected common video state with backend-local
     * MTL device/session runtime policy.
     *
     * This helper still does not build st20p_rx_ops directly.
     * It produces the named intermediate session-projection object which later
     * step(s) will map into st20p_rx_ops through explicit per-axis helpers.
     */
    [[nodiscard]] static std::expected<ProjectedMtlVideoSessionConfig, Error>
    project_mtl_video_session_config(const ProjectedCommonVideoConfig &common,
                                     const MtlDeviceRuntimeConfig &device_runtime,
                                     const MtlSessionRuntimeConfig &session_runtime);

    /*
     * Convenience overload for the current start path.
     * It must call:
     * - project_common_video_config(...)
     * - default_mtl_device_runtime_config()
     * - default_mtl_session_runtime_config(...)
     * - project_mtl_video_session_config(...)
     *
     * in that order, so that the defaulting remains explicit.
     */
    [[nodiscard]] static std::expected<ProjectedMtlVideoSessionConfig, Error>
    project_mtl_video_session_config(const RxVideoConfig &cfg);

    [[nodiscard]] static Error
    validate_projected_mtl_video_session_config(const ProjectedMtlVideoSessionConfig &cfg) noexcept;

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
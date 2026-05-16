#ifndef ST2110_OBS_PLUGIN_MTL_RX_VIDEO_BACKEND_PROXY_HPP
#define ST2110_OBS_PLUGIN_MTL_RX_VIDEO_BACKEND_PROXY_HPP

#include <st2110/backends/mtl/mtl_worker_graph_client.hpp>
#include <st2110/contracts/backend/backend.hpp>
#include <st2110/delivery/video/mtl_video_start_config.hpp>

#include <memory>
#include <optional>
#include <string>

namespace st2110 {

/*
 * OBS-process MTL video backend proxy.
 *
 * This class must not own real MTL runtime state and must not call MTL APIs.
 * The future implementation will send video session commands to an MTL worker
 * process and deliver worker-exported frames to the OBS-process sink.
 */
class MtlRxVideoBackendProxy final : public IRxBackend {
  public:
    MtlRxVideoBackendProxy(MtlVideoStartConfig cfg, std::shared_ptr<MtlWorkerGraphClient> graph_client);
    ~MtlRxVideoBackendProxy() override = default;

    MtlRxVideoBackendProxy(const MtlRxVideoBackendProxy &) = delete;
    MtlRxVideoBackendProxy &operator=(const MtlRxVideoBackendProxy &) = delete;

    MtlRxVideoBackendProxy(MtlRxVideoBackendProxy &&) noexcept = delete;
    MtlRxVideoBackendProxy &operator=(MtlRxVideoBackendProxy &&) noexcept = delete;

    RxBackendLifecycleResult start(IFrameSink *sink) override;
    RxBackendLifecycleResult stop() override;

    [[nodiscard]] std::optional<MtlWorkerErrorDetail> last_error_detail() const;
    [[nodiscard]] std::string last_error_message() const;

  private:
    MtlVideoStartConfig cfg_{};
    std::shared_ptr<MtlWorkerGraphClient> graph_client_{};
    IFrameSink *sink_ = nullptr;
    bool started_ = false;
};

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_MTL_RX_VIDEO_BACKEND_PROXY_HPP
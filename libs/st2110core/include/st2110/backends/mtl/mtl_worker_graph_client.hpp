#ifndef ST2110_OBS_PLUGIN_MTL_WORKER_GRAPH_CLIENT_HPP
#define ST2110_OBS_PLUGIN_MTL_WORKER_GRAPH_CLIENT_HPP

#include <st2110/backends/mtl/mtl_worker_manager.hpp>
#include <st2110/backends/mtl/mtl_worker_protocol.hpp>
#include <st2110/contracts/backend/backend.hpp>
#include <st2110/delivery/audio/mtl_audio_start_config.hpp>
#include <st2110/delivery/video/mtl_video_start_config.hpp>
#include <st2110/foundation/error.hpp>

#include <expected>
#include <memory>
#include <optional>

namespace st2110 {

/*
 * OBS-process-side MTL worker graph client.
 *
 * This class is the future shared control client for one MTL receive graph.
 * Video and audio proxy backends should coordinate through one graph client
 * instead of independently controlling the worker.
 *
 * This class must not include MTL runtime headers and must not call MTL APIs.
 * Current skeleton does not implement process spawn, IPC, or shared memory.
 */
class MtlWorkerGraphClient final {
  public:
    MtlWorkerGraphClient();
    ~MtlWorkerGraphClient();

    MtlWorkerGraphClient(const MtlWorkerGraphClient &) = delete;
    MtlWorkerGraphClient &operator=(const MtlWorkerGraphClient &) = delete;

    MtlWorkerGraphClient(MtlWorkerGraphClient &&) noexcept = delete;
    MtlWorkerGraphClient &operator=(MtlWorkerGraphClient &&) noexcept = delete;

    [[nodiscard]] std::expected<bool, Error> configure_video(MtlVideoStartConfig cfg);
    [[nodiscard]] std::expected<bool, Error> configure_audio(MtlAudioStartConfig cfg);

    [[nodiscard]] std::expected<bool, Error> attach_sink(IFrameSink *sink);
    void detach_sink_noexcept(IFrameSink *sink) noexcept;

    [[nodiscard]] std::expected<bool, Error> start();
    [[nodiscard]] std::expected<bool, Error> stop();

    [[nodiscard]] std::expected<MtlWorkerStatsEvent, Error> stats();

    void stop_noexcept() noexcept;

    [[nodiscard]] std::expected<MtlWorkerStartSessionsRequest, Error> make_start_sessions_request() const;
    [[nodiscard]] MtlWorkerStopSessionsRequest make_stop_sessions_request() const;
    [[nodiscard]] std::expected<MtlWorkerStatsRequest, Error> make_stats_request() const;

    [[nodiscard]] bool configured() const noexcept;
    [[nodiscard]] bool running() const noexcept;

    [[nodiscard]] MtlWorkerGraphId graph_id() const noexcept;
    [[nodiscard]] const std::optional<MtlVideoStartConfig> &video_config() const noexcept;
    [[nodiscard]] const std::optional<MtlAudioStartConfig> &audio_config() const noexcept;

    [[nodiscard]] IFrameSink *sink() const noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_MTL_WORKER_GRAPH_CLIENT_HPP
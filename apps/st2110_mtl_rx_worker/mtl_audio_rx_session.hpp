#ifndef ST2110_OBS_PLUGIN_MTL_RX_WORKER_AUDIO_RX_SESSION_HPP
#define ST2110_OBS_PLUGIN_MTL_RX_WORKER_AUDIO_RX_SESSION_HPP

#include "mtl_runtime_context.hpp"

#include <st2110/delivery/audio/mtl_audio_start_config.hpp>
#include <st2110/foundation/error.hpp>

#include <expected>
#include <memory>

namespace st2110_mtl_rx_worker {

/*
 * Worker-process-local MTL ST30P audio RX session.
 *
 * Owns st30p_rx_handle lifetime only.
 * Does not own mtl_handle.
 * Does not know OBS sinks.
 *
 * Current implementation starts a worker-local receive thread that drains MTL
 * audio frames and returns them to MTL. Shared-memory export and
 * AudioBlockReady IPC notifications are intentionally not implemented in this
 * step.
 *
 * The owning worker graph must destroy video/audio sessions before destroying
 * the MtlRuntimeContext they were created against.
 */
class MtlAudioRxSession final {
  public:
    static std::expected<std::unique_ptr<MtlAudioRxSession>, st2110::Error> create(MtlRuntimeContext &runtime,
                                                                                   st2110::MtlAudioStartConfig cfg);

    ~MtlAudioRxSession();

    MtlAudioRxSession(const MtlAudioRxSession &) = delete;
    MtlAudioRxSession &operator=(const MtlAudioRxSession &) = delete;

    MtlAudioRxSession(MtlAudioRxSession &&) noexcept = delete;
    MtlAudioRxSession &operator=(MtlAudioRxSession &&) noexcept = delete;

    void wake_block() noexcept;

    [[nodiscard]] const st2110::MtlAudioStartConfig &config() const noexcept;

  private:
    struct Impl;

    explicit MtlAudioRxSession(std::unique_ptr<Impl> impl);

    std::unique_ptr<Impl> impl_;
};

} // namespace st2110_mtl_rx_worker

#endif // ST2110_OBS_PLUGIN_MTL_RX_WORKER_AUDIO_RX_SESSION_HPP
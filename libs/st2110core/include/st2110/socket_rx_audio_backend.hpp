#ifndef ST2110_OBS_SOCKET_RX_AUDIO_BACKEND_HPP
#define ST2110_OBS_SOCKET_RX_AUDIO_BACKEND_HPP

#include "backend.hpp"
#include "backend_factory.hpp"
#include "socket_runtime.hpp"

#include <memory>
#include <mutex>

namespace st2110 {
class SocketRxAudioBackend final : public IRxAudioBackend {
  public:
    SocketRxAudioBackend() : port_factory_(make_default_port_factory()) {}

    explicit SocketRxAudioBackend(std::unique_ptr<ISocketRxPortFactory> port_factory) : port_factory_(std::move(port_factory)) {}

    [[nodiscard]] const char *backend_name() const override {
        return "socket";
    }

    RxBackendLifecycleResult stop() override {
        if (!state_.audio_active && !state_.video_active && port_ == nullptr) {
            return state_;
        }

        if (port_ != nullptr && port_->is_open()) {
            if (Error err = port_->close(); err != Error::Ok) {
                return std::unexpected(err);
            }
        }

        clear_runtime_objects();

        return state_;
    }

    [[nodiscard]] RxBackendCapabilities capabilities() const override {
        return RxBackendCapabilities{.video_rx = false, .audio_rx = true};
    }

    RxBackendLifecycleResult start_audio(const RxAudioConfig &cfg, IAudioFrameSink &sink) override {
        if (state_.audio_active) {
            return std::unexpected(Error::InvalidBackendState);
        }

        if (Error err = validate_runtime_dependencies(); err != Error::Ok) {
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

        return start_audio_runtime(cfg, sink, *open_cfg, std::move(port));
    }

    [[nodiscard]] RxBackendState state() const override {
        return state_;
    }

    [[nodiscard]] BackendStats stats() const override {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        return stats_;
    }

  private:
    [[nodiscard]] static std::unique_ptr<ISocketRxPortFactory> make_default_port_factory();

    [[nodiscard]] Error validate_runtime_dependencies() const noexcept {
        if (port_factory_ == nullptr) {
            return Error::InvalidValue;
        }

        return Error::Ok;
    }

    [[nodiscard]] static std::expected<SocketRxOpenConfig, Error>
    build_open_config(const RxAudioConfig &cfg);

    [[nodiscard]] std::unique_ptr<ISocketRxPort> create_port() const;

    RxBackendLifecycleResult start_audio_runtime(const RxAudioConfig &cfg,
                                                 IAudioFrameSink &sink,
                                                 const SocketRxOpenConfig &open_cfg,
                                                 std::unique_ptr<ISocketRxPort> port);

    void clear_runtime_objects() noexcept;

    std::unique_ptr<ISocketRxPortFactory> port_factory_{};
    std::unique_ptr<ISocketRxPort> port_{};
    RxBackendState state_{};
    IAudioFrameSink *audio_sink_ = nullptr;
    mutable std::mutex stats_mutex_{};
    BackendStats stats_{};
};

class SocketRxAudioBackendFactory final : public IRxBackendFactory {
  public:
    [[nodiscard]] RxBackendDescriptor descriptor() const override;

    [[nodiscard]] std::unique_ptr<IRxBackend> create_backend() const override;
};
} // namespace st2110

#endif // ST2110_OBS_SOCKET_RX_AUDIO_BACKEND_HPP
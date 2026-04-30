#ifndef ST2110_OBS_SOCKET_RX_VIDEO_BACKEND_HPP
#define ST2110_OBS_SOCKET_RX_VIDEO_BACKEND_HPP

#include "backend.hpp"
#include "backend_factory.hpp"
#include "socket_runtime.hpp"
#include "socket_stub_rx_port.hpp"

#include <memory>

namespace st2110 {
class SocketRxVideoBackend final : public IRxVideoBackend {
  public:
    SocketRxVideoBackend() : port_factory_(make_socket_stub_rx_port_factory()) {}

    explicit SocketRxVideoBackend(std::unique_ptr<ISocketRxPortFactory> port_factory)
        : port_factory_(std::move(port_factory)) {}

    [[nodiscard]] const char *backend_name() const override { return "socket"; }

    RxBackendLifecycleResult stop() override {
        if (!state_.audio_active && !state_.video_active && port_ == nullptr) {
            return state_;
        }

        if (port_ != nullptr) {
            if (port_->is_open()) {
                if (Error err = port_->close(); err != Error::Ok) {
                    return std::unexpected(err);
                }
            }

            clear_runtime_objects();
        }

        return state_;
    }

    [[nodiscard]] RxBackendCapabilities capabilities() const override {
        return RxBackendCapabilities{.video_rx = true, .audio_rx = false};
    }

    RxBackendLifecycleResult start_video(const RxVideoConfig &cfg, IVideoFrameSink &sink) override {
        if (state_.video_active) {
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

        (void)sink;
        return open_port_for_video(*open_cfg, std::move(port));
    }

    [[nodiscard]] RxBackendState state() const override { return state_; }

  private:
    [[nodiscard]] Error validate_runtime_dependencies() const noexcept {
        if (port_factory_ == nullptr) {
            return Error::InvalidValue;
        }

        return Error::Ok;
    }

    [[nodiscard]] static std::expected<SocketRxOpenConfig, Error> build_open_config(const RxVideoConfig &cfg) {
        auto res = socket_rx_open_config_from_video_config(cfg);
        if (!res) {
            return std::unexpected(res.error());
        }

        return res;
    }

    [[nodiscard]] std::unique_ptr<ISocketRxPort> create_port() const { return port_factory_->create_port(); }

    RxBackendLifecycleResult open_port_for_video(const SocketRxOpenConfig &open_cfg,
                                                 std::unique_ptr<ISocketRxPort> port) {
        if (Error err = port->open(open_cfg); err != Error::Ok) {
            return std::unexpected(err);
        }
        port_ = std::move(port);
        state_.video_active = true;

        return state_;
    }

    void clear_runtime_objects() noexcept {
        port_.reset();
        state_ = {};
    }

    std::unique_ptr<ISocketRxPortFactory> port_factory_{};
    std::unique_ptr<ISocketRxPort> port_{};
    RxBackendState state_{};
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

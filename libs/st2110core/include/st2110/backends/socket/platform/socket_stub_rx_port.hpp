#ifndef ST2110_OBS_PLUGIN_SOCKET_STUB_RX_PORT_HPP
#define ST2110_OBS_PLUGIN_SOCKET_STUB_RX_PORT_HPP

#include "socket_runtime.hpp"

#include <memory>
#include <optional>
#include <span>

namespace st2110 {
class SocketStubRxPort final : public ISocketRxPort {
  public:
    SocketStubRxPort() = default;

    [[nodiscard]] bool is_open() const noexcept override { return open_; }

    Error open(const SocketRxOpenConfig &cfg) override {
        if (Error err = validate_socket_rx_open_config(cfg); err != Error::Ok) {
            return err;
        }

        if (open_) {
            return Error::InvalidBackendState;
        }

        open_cfg_ = cfg;
        open_ = true;
        return Error::Ok;
    }

    Error close() override {
        if (open_) {
            open_cfg_ = {};
        }

        open_ = false;
        return Error::Ok;
    }

    [[nodiscard]] std::expected<SocketReceiveResult, Error> receive(std::span<std::uint8_t> buffer) override {
        if (!open_) {
            return std::unexpected(Error::InvalidBackendState);
        }
        if (buffer.empty()) {
            return std::unexpected(Error::InvalidValue);
        }

        return std::unexpected(Error::Unsupported);
    }

  private:
    bool open_ = false;
    std::optional<SocketRxOpenConfig> open_cfg_{};
};

class SocketStubRxPortFactory final : public ISocketRxPortFactory {
  public:
    [[nodiscard]] std::unique_ptr<ISocketRxPort> create_port() const override {
        return std::make_unique<SocketStubRxPort>();
    }
};

[[nodiscard]] inline std::unique_ptr<ISocketRxPortFactory> make_socket_stub_rx_port_factory() {
    return std::make_unique<SocketStubRxPortFactory>();
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_SOCKET_STUB_RX_PORT_HPP
#ifndef ST2110_OBS_PLUGIN_BACKEND_FACTORY_HPP
#define ST2110_OBS_PLUGIN_BACKEND_FACTORY_HPP

#include "backend.hpp"
#include "error.hpp"

#include <expected>
#include <memory>
#include <span>
#include <string_view>

namespace st2110 {
enum class RxBackendKind { Socket, Mtl };

[[nodiscard]] inline Error validate_rx_backend_kind(RxBackendKind kind) noexcept {
    switch (kind) {
    case RxBackendKind::Socket:
    case RxBackendKind::Mtl:
        return Error::Ok;
    default:
        return Error::InvalidValue;
    }
}

[[nodiscard]] inline std::string_view rx_backend_kind_name(RxBackendKind kind) noexcept {
    switch (kind) {
    case RxBackendKind::Socket:
        return "socket";
    case RxBackendKind::Mtl:
        return "mtl";
    default:
        return {};
    }
}

[[nodiscard]] inline std::expected<RxBackendKind, Error> parse_rx_backend_kind(std::string_view value) noexcept {
    if (value == "socket") {
        return RxBackendKind::Socket;
    }

    if (value == "mtl") {
        return RxBackendKind::Mtl;
    }

    return std::unexpected(Error::InvalidValue);
}

[[nodiscard]] inline Error validate_rx_media_kind(RxMediaKind kind) noexcept {
    switch (kind) {
    case RxMediaKind::Video:
    case RxMediaKind::Audio:
        return Error::Ok;
    default:
        return Error::InvalidValue;
    }
}

struct RxBackendDescriptor {
    RxBackendKind kind = RxBackendKind::Socket;
    std::string_view name{};
    RxBackendCapabilities capabilities{};
    bool available = true;
};

struct RxBackendSelection {
    RxBackendKind backend_kind = RxBackendKind::Socket;
    RxMediaKind media_kind = RxMediaKind::Video;
};

class IRxBackendFactory {
  public:
    virtual RxBackendDescriptor descriptor() const = 0;
    virtual std::unique_ptr<IRxBackend> create_backend() const = 0;
    virtual ~IRxBackendFactory() = default;
};

[[nodiscard]] inline Error validate_rx_backend_descriptor(const RxBackendDescriptor &descriptor) noexcept {
    if (const Error err = validate_rx_backend_kind(descriptor.kind); err != Error::Ok) {
        return err;
    }

    if (descriptor.name.empty()) {
        return Error::InvalidValue;
    }

    if (!descriptor.capabilities.video_rx && !descriptor.capabilities.audio_rx) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

[[nodiscard]] inline Error validate_rx_backend_selection(const RxBackendSelection &selection) noexcept {
    if (const Error err = validate_rx_backend_kind(selection.backend_kind); err != Error::Ok) {
        return err;
    }

    if (const Error err = validate_rx_media_kind(selection.media_kind); err != Error::Ok) {
        return err;
    }

    return Error::Ok;
}

[[nodiscard]] bool rx_backend_kind_built(RxBackendKind kind) noexcept;

[[nodiscard]] std::span<IRxBackendFactory *const> default_rx_backend_factories() noexcept;

[[nodiscard]] inline std::expected<IRxBackendFactory *, Error>
select_rx_backend_factory(std::span<IRxBackendFactory *const> factories, const RxBackendSelection &selection) {
    if (const Error err = validate_rx_backend_selection(selection); err != Error::Ok) {
        return std::unexpected(err);
    }

    for (IRxBackendFactory *const factory : factories) {
        if (factory == nullptr) {
            return std::unexpected(Error::InvalidValue);
        }

        const RxBackendDescriptor descriptor = factory->descriptor();
        if (const Error err = validate_rx_backend_descriptor(descriptor); err != Error::Ok) {
            return std::unexpected(err);
        }
    }

    for (IRxBackendFactory *const factory : factories) {
        const RxBackendDescriptor descriptor = factory->descriptor();

        if (descriptor.kind != selection.backend_kind) {
            continue;
        }

        if (!descriptor.available) {
            continue;
        }

        if (!supports_media(descriptor.capabilities, selection.media_kind)) {
            continue;
        }

        return factory;
    }

    return std::unexpected(Error::Unsupported);
}

[[nodiscard]] inline std::expected<std::unique_ptr<IRxBackend>, Error>
create_rx_backend(std::span<IRxBackendFactory *const> factories, const RxBackendSelection &selection) {
    auto selected_factory = select_rx_backend_factory(factories, selection);
    if (!selected_factory.has_value()) {
        return std::unexpected(selected_factory.error());
    }

    std::unique_ptr<IRxBackend> backend = (*selected_factory)->create_backend();
    if (!backend) {
        return std::unexpected(Error::InvalidValue);
    }

    return backend;
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_BACKEND_FACTORY_HPP
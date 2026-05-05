#ifndef ST2110_OBS_PLUGIN_MTL_RX_BACKEND_FACTORY_HPP
#define ST2110_OBS_PLUGIN_MTL_RX_BACKEND_FACTORY_HPP

#include "backend_factory.hpp"

namespace st2110 {

class MtlRxVideoBackendFactory final : public IRxBackendFactory {
public:
    [[nodiscard]] RxBackendDescriptor descriptor() const override;
    [[nodiscard]] std::unique_ptr<IRxBackend> create_backend() const override;
};

class MtlRxAudioBackendFactory final : public IRxBackendFactory {
public:
    [[nodiscard]] RxBackendDescriptor descriptor() const override;
    [[nodiscard]] std::unique_ptr<IRxBackend> create_backend() const override;
};

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_MTL_RX_BACKEND_FACTORY_HPP
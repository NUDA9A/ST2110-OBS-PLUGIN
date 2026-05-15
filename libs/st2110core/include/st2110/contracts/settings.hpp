#ifndef ST2110_OBS_SETTINGS_HPP
#define ST2110_OBS_SETTINGS_HPP

#include <st2110/receive/shared/receive_reorder_tolerance_policy.hpp>
#include <st2110/contracts/video/partial_unit_policy.hpp>

namespace st2110 {
enum class ReceiveBackendKind {
    Socket,
    Mtl,
};

struct Settings {
    ReorderBufferConfig reorder_buffer_config{};
    PartialUnitPolicy partial_unit_policy{};
    ReceiveBackendKind backend_kind = ReceiveBackendKind::Socket;

    friend bool operator==(const Settings &, const Settings &) = default;
};
}

#endif // ST2110_OBS_SETTINGS_HPP

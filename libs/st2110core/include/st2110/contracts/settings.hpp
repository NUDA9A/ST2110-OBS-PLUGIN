#ifndef ST2110_OBS_SETTINGS_HPP
#define ST2110_OBS_SETTINGS_HPP

#include <st2110/contracts/video/partial_unit_policy.hpp>
#include <st2110/receive/shared/receive_reorder_tolerance_policy.hpp>

namespace st2110 {
enum class ReceiveBackendKind {
    Socket,
    Mtl,
};

struct Settings {
    ReceiveReorderGapPolicy reorder_tolerance_policy = ReceiveReorderGapPolicy::WaitForMissing;
    PartialUnitPolicy partial_unit_policy = PartialUnitPolicy::EmitWithFlag;
    ReceiveBackendKind backend_kind = ReceiveBackendKind::Socket;

    friend bool operator==(const Settings &, const Settings &) = default;
};
} // namespace st2110

#endif // ST2110_OBS_SETTINGS_HPP
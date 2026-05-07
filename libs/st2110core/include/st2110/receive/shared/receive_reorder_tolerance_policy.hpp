#ifndef ST2110_OBS_PLUGIN_RECEIVE_REORDER_TOLERANCE_POLICY_HPP
#define ST2110_OBS_PLUGIN_RECEIVE_REORDER_TOLERANCE_POLICY_HPP

#include "st2110/foundation/error.hpp"

namespace st2110 {

enum class ReceiveReorderGapPolicy {
    WaitForMissing,
    FlushGapOnce,
};

struct ReceiveReorderTolerancePolicy {
    ReceiveReorderGapPolicy gap_policy = ReceiveReorderGapPolicy::WaitForMissing;
};

[[nodiscard]] inline Error validate_receive_reorder_gap_policy(ReceiveReorderGapPolicy policy) noexcept {
    switch (policy) {
    case ReceiveReorderGapPolicy::WaitForMissing:
    case ReceiveReorderGapPolicy::FlushGapOnce:
        return Error::Ok;
    default:
        return Error::InvalidValue;
    }
}

[[nodiscard]] inline Error
validate_receive_reorder_tolerance_policy(const ReceiveReorderTolerancePolicy& policy) noexcept {
    return validate_receive_reorder_gap_policy(policy.gap_policy);
}

[[nodiscard]] inline bool
receive_reorder_policy_allows_gap_flush_once(const ReceiveReorderTolerancePolicy& policy) noexcept {
    return policy.gap_policy == ReceiveReorderGapPolicy::FlushGapOnce;
}

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_RECEIVE_REORDER_TOLERANCE_POLICY_HPP
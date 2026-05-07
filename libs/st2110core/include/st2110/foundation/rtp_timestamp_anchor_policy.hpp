#ifndef ST2110_OBS_PLUGIN_RTP_TIMESTAMP_ANCHOR_POLICY_HPP
#define ST2110_OBS_PLUGIN_RTP_TIMESTAMP_ANCHOR_POLICY_HPP

#include "error.hpp"

namespace st2110 {

enum class RtpTimestampInitialAnchorMode {
    ConfiguredReference,
    FirstObservedBecomesLocalZero,
};

[[nodiscard]] inline Error validate_rtp_timestamp_initial_anchor_mode(RtpTimestampInitialAnchorMode mode) noexcept {
    switch (mode) {
    case RtpTimestampInitialAnchorMode::ConfiguredReference:
    case RtpTimestampInitialAnchorMode::FirstObservedBecomesLocalZero:
        return Error::Ok;
    default:
        return Error::InvalidValue;
    }
}

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_RTP_TIMESTAMP_ANCHOR_POLICY_HPP
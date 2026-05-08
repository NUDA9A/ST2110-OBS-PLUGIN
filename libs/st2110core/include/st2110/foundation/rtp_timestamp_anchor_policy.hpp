#ifndef ST2110_OBS_PLUGIN_RTP_TIMESTAMP_ANCHOR_POLICY_HPP
#define ST2110_OBS_PLUGIN_RTP_TIMESTAMP_ANCHOR_POLICY_HPP

namespace st2110 {

enum class RtpTimestampInitialAnchorMode {
    ConfiguredReference,
    FirstObservedBecomesLocalZero,
};
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_RTP_TIMESTAMP_ANCHOR_POLICY_HPP
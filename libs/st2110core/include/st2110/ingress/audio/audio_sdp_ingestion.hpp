#ifndef ST2110_OBS_PLUGIN_AUDIO_SDP_INGESTION_HPP
#define ST2110_OBS_PLUGIN_AUDIO_SDP_INGESTION_HPP

#include "st2110/foundation/error.hpp"
#include "st2110/model/audio/audio_signaling.hpp"
#include "audio_sdp_media_section.hpp"
#include "audio_sdp_signaling_adapter.hpp"
#include "audio_sdp_timing_attributes.hpp"

#include <expected>
#include <string_view>

namespace st2110 {
[[nodiscard]] inline Error
validate_raw_audio_sdp_required_st2110_clock_signaling(const RawAudioSdpTimingAttributes &raw_timing) {
    if (!raw_audio_sdp_has_reference_clock(raw_timing)) {
        return Error::InvalidValue;
    }

    if (!raw_audio_sdp_has_media_level_mediaclk(raw_timing)) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

[[nodiscard]] inline std::expected<AudioStreamSignaling, Error>
parse_audio_stream_signaling_from_sdp(std::string_view sdp, uint8_t expected_payload_type) {
    auto raw = select_raw_audio_sdp_media_section(sdp, expected_payload_type);

    if (!raw.has_value()) {
        return std::unexpected(raw.error());
    }

    auto raw_timing = parse_audio_sdp_timing_attributes(*raw);

    if (!raw_timing.has_value()) {
        return std::unexpected(raw_timing.error());
    }

    if (Error err = validate_raw_audio_sdp_required_st2110_clock_signaling(*raw_timing); err != Error::Ok) {
        return std::unexpected(err);
    }

    return audio_stream_signaling_from_raw_audio_sdp_media_section(*raw);
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_AUDIO_SDP_INGESTION_HPP
#ifndef ST2110_OBS_PLUGIN_AUDIO_SDP_INGESTION_HPP
#define ST2110_OBS_PLUGIN_AUDIO_SDP_INGESTION_HPP

#include "audio_sdp_media_section.hpp"
#include "audio_sdp_signaling_adapter.hpp"
#include "audio_signaling.hpp"
#include "error.hpp"

#include <expected>
#include <string_view>
#include <vector>

namespace st2110 {
[[nodiscard]] inline bool raw_audio_sdp_has_attribute(const std::vector<RawAudioSdpAttribute> &attributes,
                                                      std::string_view name) {
  for (const auto &attribute : attributes) {
    if (attribute.name == name) {
      return true;
    }
  }

  return false;
}

[[nodiscard]] inline Error validate_raw_audio_sdp_required_st2110_clock_signaling(const RawAudioSdpMediaSection &raw) {
  const bool has_ts_refclk = raw_audio_sdp_has_attribute(raw.unknown_session_attributes, "ts-refclk") ||
                             raw_audio_sdp_has_attribute(raw.unknown_attributes, "ts-refclk");

  if (!has_ts_refclk) {
    return Error::InvalidValue;
  }

  const bool has_media_level_mediaclk = raw_audio_sdp_has_attribute(raw.unknown_attributes, "mediaclk");

  if (!has_media_level_mediaclk) {
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

  if (Error err = validate_raw_audio_sdp_required_st2110_clock_signaling(*raw); err != Error::Ok) {
    return std::unexpected(err);
  }

  return audio_stream_signaling_from_raw_audio_sdp_media_section(*raw);
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_AUDIO_SDP_INGESTION_HPP
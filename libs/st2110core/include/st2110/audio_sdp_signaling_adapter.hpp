#ifndef ST2110_OBS_PLUGIN_AUDIO_SDP_SIGNALING_ADAPTER_HPP
#define ST2110_OBS_PLUGIN_AUDIO_SDP_SIGNALING_ADAPTER_HPP

#include "audio_sdp_media_section.hpp"
#include "audio_signaling.hpp"
#include "error.hpp"

#include <expected>
#include <string>
#include <string_view>

namespace st2110 {
[[nodiscard]] inline char audio_sdp_ascii_lower(char c) {
  if (c >= 'A' && c <= 'Z') {
    return static_cast<char>(c - 'A' + 'a');
  }

  return c;
}

[[nodiscard]] inline bool audio_sdp_ascii_iequals(std::string_view lhs, std::string_view rhs) {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  for (std::size_t i = 0; i < lhs.size(); ++i) {
    if (audio_sdp_ascii_lower(lhs[i]) != audio_sdp_ascii_lower(rhs[i])) {
      return false;
    }
  }

  return true;
}

[[nodiscard]] inline std::expected<AudioPcmEncoding, Error>
audio_pcm_encoding_from_raw_audio_sdp_rtpmap_encoding_name(std::string_view encoding_name) {
  if (encoding_name.empty()) {
    return std::unexpected(Error::InvalidValue);
  }

  if (audio_sdp_ascii_iequals(encoding_name, "L24") || audio_sdp_ascii_iequals(encoding_name, "L16")) {
    return AudioPcmEncoding::LinearPcm;
  }

  return std::unexpected(Error::Unsupported);
}

[[nodiscard]] inline AudioChannelOrderSignaling
audio_channel_order_signaling_from_raw_audio_sdp_value(std::string_view value) {
  AudioChannelOrderSignaling res{};

  if (value.starts_with("SMPTE2110.")) {
    res.convention = AudioChannelOrderConvention::Smpte2110;
  } else {
    res.convention = AudioChannelOrderConvention::Other;
  }

  res.raw_value = std::string(value);
  return res;
}

[[nodiscard]] inline std::expected<AudioStreamSignaling, Error>
audio_stream_signaling_from_raw_audio_sdp_media_section(const RawAudioSdpMediaSection &raw) {
  if (!contains_audio_payload_type(raw.media_payload_types, raw.payload_type)) {
    return std::unexpected(Error::InvalidValue);
  }

  if (raw.rtpmap.empty() || raw.parsed_rtpmap.encoding_name.empty() || raw.parsed_rtpmap.sampling_rate_hz == 0) {
    return std::unexpected(Error::InvalidValue);
  }

  if (!raw.parsed_rtpmap.channel_count.has_value()) {
    return std::unexpected(Error::InvalidValue);
  }

  if (!raw.packet_time_us.has_value()) {
    return std::unexpected(Error::InvalidValue);
  }

  auto encoding = audio_pcm_encoding_from_raw_audio_sdp_rtpmap_encoding_name(raw.parsed_rtpmap.encoding_name);

  if (!encoding.has_value()) {
    return std::unexpected(encoding.error());
  }

  AudioStreamSignaling signaling{};
  signaling.media.pcm_encoding = *encoding;
  signaling.media.sampling_rate_hz = raw.parsed_rtpmap.sampling_rate_hz;
  signaling.media.packet_time_us = *raw.packet_time_us;
  signaling.media.channel_count = *raw.parsed_rtpmap.channel_count;

  if (raw.channel_order.has_value()) {
    if (raw.channel_order->empty()) {
      return std::unexpected(Error::InvalidValue);
    }

    signaling.channel_order = audio_channel_order_signaling_from_raw_audio_sdp_value(*raw.channel_order);
  }

  if (Error err = validate_audio_stream_signaling(signaling); err != Error::Ok) {
    return std::unexpected(err);
  }

  return signaling;
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_AUDIO_SDP_SIGNALING_ADAPTER_HPP
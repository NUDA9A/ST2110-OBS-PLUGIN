#ifndef ST2110_OBS_PLUGIN_AUDIO_SIGNALING_RX_CONFIG_HPP
#define ST2110_OBS_PLUGIN_AUDIO_SIGNALING_RX_CONFIG_HPP

#include "audio_signaling.hpp"
#include "rx_config.hpp"
#include "config_validation.hpp"

#include <expected>
#include <string>

namespace st2110 {
[[nodiscard]] inline std::expected<RxAudioConfig, Error>
rx_audio_config_from_audio_stream_signaling(const AudioStreamSignaling &signaling, uint16_t udp_port,
                                            uint8_t payload_type, std::string local_ip, std::string dest_ip,
                                            AudioSampleFormat format = AudioSampleFormat::LinearPcm) {
  if (Error err = validate_audio_stream_signaling(signaling); err != Error::Ok) {
    return std::unexpected(err);
  }

  auto samples_per_packet = config_validation::audio_samples_per_packet_from_rate_and_packet_time(
      signaling.media.sampling_rate_hz, signaling.media.packet_time_us);

  if (!samples_per_packet) {
    return std::unexpected(samples_per_packet.error());
  }

  RxAudioConfig res{};
  res.sampling_rate_hz = signaling.media.sampling_rate_hz;
  res.packet_time_us = signaling.media.packet_time_us;
  res.channel_count = signaling.media.channel_count;
  res.samples_per_packet = *samples_per_packet;
  res.udp_port = udp_port;
  res.payload_type = payload_type;
  res.local_ip = local_ip;
  res.dest_ip = dest_ip;
  res.format = format;

  if (Error err = validate_rx_audio_config(res); err != Error::Ok) {
    return std::unexpected(err);
  }

  return res;
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_AUDIO_SIGNALING_RX_CONFIG_HPP

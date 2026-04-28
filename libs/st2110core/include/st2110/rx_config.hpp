#ifndef ST2110_OBS_PLUGIN_RX_CONFIG_HPP
#define ST2110_OBS_PLUGIN_RX_CONFIG_HPP

#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <string>

#include "audio_signaling.hpp"
#include "config_validation.hpp"
#include "pixel_format.hpp"
#include "video_scan_mode.hpp"
#include "video_packing_mode.hpp"

namespace st2110 {
struct RxVideoConfig;
[[nodiscard]] Error validate_rx_video_config(const RxVideoConfig &cfg);

struct RxVideoConfig {
  uint32_t width;
  uint32_t height;
  uint32_t fps_num;
  uint32_t fps_den;
  uint16_t udp_port;
  uint8_t payload_type;
  std::string local_ip;
  std::string dest_ip;
  PixelFormat format;
  VideoScanMode scan_mode = VideoScanMode::Progressive;
  VideoPackingMode packing_mode = VideoPackingMode::Gpm;

  [[nodiscard]] bool is_valid() const {
    return (validate_rx_video_config(*this) == Error::Ok);
  }
};

enum class AudioSampleFormat { LinearPcm };

struct RxAudioConfig;
[[nodiscard]] Error validate_rx_audio_config(const RxAudioConfig &cfg);

struct RxAudioConfig {
  uint32_t sampling_rate_hz = 0;
  uint32_t packet_time_us = 0;
  uint32_t samples_per_packet = 0;
  uint16_t channel_count = 0;

  uint16_t udp_port = 0;
  uint8_t payload_type = 0;
  std::string local_ip;
  std::string dest_ip;

  AudioSampleFormat format = AudioSampleFormat::LinearPcm;

  [[nodiscard]] bool is_valid() const {
    return validate_rx_audio_config(*this) == Error::Ok;
  }
};

struct AudioRuntimeSupportPolicy {
  std::span<const AudioSampleFormat> sample_formats;
  std::span<const AudioConformanceRange> conformance_ranges;
};

namespace audio_runtime_support {
inline constexpr auto default_sample_formats =
    std::array{AudioSampleFormat::LinearPcm};

inline constexpr auto default_conformance_ranges =
    std::array{audio_level_a_receiver_baseline()};
} // namespace audio_runtime_support

[[nodiscard]] inline bool audio_sample_format_supported(
    AudioSampleFormat format,
    std::span<const AudioSampleFormat> supported_formats) {
  for (AudioSampleFormat supported : supported_formats) {
    if (format == supported) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] inline std::expected<AudioMediaDescription, Error>
audio_media_description_from_rx_audio_config(const RxAudioConfig &cfg) {
  AudioPcmEncoding pcmEncoding;
  switch (cfg.format) {
  case AudioSampleFormat::LinearPcm:
    pcmEncoding = AudioPcmEncoding::LinearPcm;
    break;
  default:
    return std::unexpected(Error::Unsupported);
  }

  return AudioMediaDescription{pcmEncoding, cfg.sampling_rate_hz,
                               cfg.packet_time_us, cfg.channel_count};
}

[[nodiscard]] inline bool rx_audio_config_matches_any_conformance_range(
    const RxAudioConfig &cfg, std::span<const AudioConformanceRange> ranges) {
  auto media = audio_media_description_from_rx_audio_config(cfg);
  if (!media) {
    return false;
  }

  for (const AudioConformanceRange &range : ranges) {
    if (audio_media_description_matches_conformance_range(*media, range)) {
      return true;
    }
  }

  return false;
}

[[nodiscard]] inline Error validate_rx_audio_config_against_runtime_support(
    const RxAudioConfig &cfg, const AudioRuntimeSupportPolicy &support) {
  if (!audio_sample_format_supported(cfg.format, support.sample_formats)) {
    return Error::Unsupported;
  }

  if (!rx_audio_config_matches_any_conformance_range(
          cfg, support.conformance_ranges)) {
    return Error::Unsupported;
  }

  auto expected_samples_per_packet =
      config_validation::audio_samples_per_packet_from_rate_and_packet_time(
          cfg.sampling_rate_hz, cfg.packet_time_us);

  if (!expected_samples_per_packet ||
      cfg.samples_per_packet != *expected_samples_per_packet) {
    return Error::InvalidValue;
  }

  if (Error err = config_validation::validate_udp_port(cfg.udp_port);
      err != Error::Ok) {
    return err;
  }

  if (!config_validation::is_dynamic_rtp_payload_type(cfg.payload_type)) {
    return Error::InvalidValue;
  }

  if (!config_validation::is_non_empty(cfg.dest_ip)) {
    return Error::InvalidValue;
  }

  return Error::Ok;
}

[[nodiscard]] inline AudioRuntimeSupportPolicy
default_audio_rx_runtime_support_policy() {
  return AudioRuntimeSupportPolicy{
      std::span<const AudioSampleFormat>{
          audio_runtime_support::default_sample_formats},
      std::span<const AudioConformanceRange>{
          audio_runtime_support::default_conformance_ranges}};
}

[[nodiscard]] inline Error validate_rx_audio_config(const RxAudioConfig &cfg) {
  return validate_rx_audio_config_against_runtime_support(
      cfg, default_audio_rx_runtime_support_policy());
}

[[nodiscard]] inline Error validate_rx_video_config(const RxVideoConfig &cfg) {
  Error err = config_validation::validate_video_format_constraints(
      cfg.format, cfg.width, cfg.height);
  if (err != Error::Ok) {
    return err;
  }

  err = config_validation::validate_frame_rate(cfg.fps_num, cfg.fps_den);
  if (err != Error::Ok) {
    return err;
  }

  err = config_validation::validate_udp_port(cfg.udp_port);
  if (err != Error::Ok) {
    return err;
  }

  if (!config_validation::is_dynamic_rtp_payload_type(cfg.payload_type)) {
    return Error::InvalidValue;
  }

  if (!config_validation::is_non_empty(cfg.dest_ip)) {
    return Error::InvalidValue;
  }

  err = config_validation::validate_video_scan_mode(cfg.scan_mode);
  if (err != Error::Ok) {
    return err;
  }

  err = validate_runtime_video_packing_mode_support(cfg.packing_mode);
  if (err != Error::Ok) {
    return err;
  }

  return Error::Ok;
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_RX_CONFIG_HPP

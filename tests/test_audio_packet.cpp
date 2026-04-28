#include <array>
#include <cassert>
#include <cstdint>
#include <span>
#include <type_traits>

#include <st2110/audio_packet.hpp>
#include <st2110/bytes.hpp>
#include <st2110/error.hpp>
#include <st2110/rtp.hpp>
#include <st2110/rx_config.hpp>

static_assert(std::is_trivially_copyable_v<st2110::AudioRtpPacketPolicy>);
static_assert(std::is_trivially_copyable_v<st2110::AudioRtpPacketView>);

static_assert(std::is_same_v<decltype(st2110::AudioRtpPacketView{}.payload), st2110::ByteSpan>);

namespace {

st2110::RxAudioConfig make_level_a_rx_config(uint16_t channels, uint8_t payload_type = 111) {
  st2110::RxAudioConfig cfg{};
  cfg.sampling_rate_hz = 48000;
  cfg.packet_time_us = 1000;
  cfg.samples_per_packet = 48;
  cfg.channel_count = channels;
  cfg.udp_port = 5004;
  cfg.payload_type = payload_type;
  cfg.local_ip = "0.0.0.0";
  cfg.dest_ip = "239.0.0.1";
  cfg.format = st2110::AudioSampleFormat::LinearPcm;
  return cfg;
}

st2110::RtpHeaderView make_rtp_header(uint8_t payload_type, bool marker = false) {
  st2110::RtpHeaderView rtp{};
  rtp.version = 2;
  rtp.padding_flag = false;
  rtp.extension_flag = false;
  rtp.csrc_count = 0;
  rtp.marker = marker;
  rtp.payload_type = payload_type;
  rtp.seq_number = 0x1234;
  rtp.timestamp = 0x01020304;
  rtp.ssrc = 0x11223344;
  rtp.payload_offset = 12;
  rtp.payload_len = 0;
  return rtp;
}

void test_wire_format_sample_sizes_are_explicit() {
  auto l16 = st2110::audio_pcm_wire_sample_bytes(st2110::AudioPcmWireFormat::L16);
  assert(l16.has_value());
  assert(*l16 == 2);

  auto l24 = st2110::audio_pcm_wire_sample_bytes(st2110::AudioPcmWireFormat::L24);
  assert(l24.has_value());
  assert(*l24 == 3);

  auto invalid = st2110::audio_pcm_wire_sample_bytes(static_cast<st2110::AudioPcmWireFormat>(255));
  assert(!invalid.has_value());
  assert(invalid.error() == st2110::Error::InvalidValue);
}

void test_policy_from_rx_audio_config_preserves_runtime_axes() {
  auto cfg = make_level_a_rx_config(2, 111);
  assert(cfg.is_valid());

  auto policy = st2110::audio_rtp_packet_policy_from_rx_audio_config(cfg, st2110::AudioPcmWireFormat::L24);

  assert(policy.has_value());
  assert(policy->sampling_rate_hz == 48000);
  assert(policy->channel_count == 2);
  assert(policy->samples_per_packet == 48);
  assert(policy->payload_type == 111);
  assert(policy->wire_format == st2110::AudioPcmWireFormat::L24);

  auto payload_bytes = st2110::audio_rtp_packet_payload_size_bytes(*policy);
  assert(payload_bytes.has_value());
  assert(*payload_bytes == 48u * 2u * 3u);
}

void test_policy_rejects_inconsistent_samples_per_packet() {
  auto cfg = make_level_a_rx_config(2, 111);
  cfg.samples_per_packet = 47;

  auto policy = st2110::audio_rtp_packet_policy_from_rx_audio_config(cfg, st2110::AudioPcmWireFormat::L24);

  assert(!policy.has_value());
  assert(policy.error() == st2110::Error::InvalidValue);
}

void test_l16_payload_size_is_not_hardcoded_to_l24_or_stereo() {
  auto cfg = make_level_a_rx_config(1, 110);

  auto policy = st2110::audio_rtp_packet_policy_from_rx_audio_config(cfg, st2110::AudioPcmWireFormat::L16);

  assert(policy.has_value());

  auto payload_bytes = st2110::audio_rtp_packet_payload_size_bytes(*policy);
  assert(payload_bytes.has_value());
  assert(*payload_bytes == 48u * 1u * 2u);
}

void test_make_audio_packet_view_accepts_matching_payload() {
  auto cfg = make_level_a_rx_config(2, 111);

  auto policy = st2110::audio_rtp_packet_policy_from_rx_audio_config(cfg, st2110::AudioPcmWireFormat::L24);
  assert(policy.has_value());

  constexpr std::size_t expected_payload_size = 48u * 2u * 3u;
  std::array<uint8_t, expected_payload_size> payload{};

  auto rtp = make_rtp_header(111, true);
  rtp.payload_len = payload.size();

  auto packet = st2110::make_audio_rtp_packet_view(rtp, st2110::ByteSpan(payload.data(), payload.size()), *policy);

  assert(packet.has_value());
  assert(packet->rtp.payload_type == 111);
  assert(packet->rtp.marker);
  assert(packet->rtp.timestamp == 0x01020304u);
  assert(packet->payload.data() == payload.data());
  assert(packet->payload.size() == payload.size());
  assert(packet->sampling_rate_hz == 48000);
  assert(packet->channel_count == 2);
  assert(packet->samples_per_channel == 48);
  assert(packet->wire_format == st2110::AudioPcmWireFormat::L24);
}

void test_make_audio_packet_view_rejects_payload_type_mismatch() {
  auto cfg = make_level_a_rx_config(2, 111);

  auto policy = st2110::audio_rtp_packet_policy_from_rx_audio_config(cfg, st2110::AudioPcmWireFormat::L24);
  assert(policy.has_value());

  std::array<uint8_t, 48u * 2u * 3u> payload{};

  auto rtp = make_rtp_header(112);
  rtp.payload_len = payload.size();

  auto packet = st2110::make_audio_rtp_packet_view(rtp, st2110::ByteSpan(payload.data(), payload.size()), *policy);

  assert(!packet.has_value());
  assert(packet.error() == st2110::Error::InvalidValue);
}

void test_make_audio_packet_view_rejects_payload_size_mismatch() {
  auto cfg = make_level_a_rx_config(2, 111);

  auto policy = st2110::audio_rtp_packet_policy_from_rx_audio_config(cfg, st2110::AudioPcmWireFormat::L24);
  assert(policy.has_value());

  std::array<uint8_t, (48u * 2u * 3u) - 1u> payload{};

  auto rtp = make_rtp_header(111);
  rtp.payload_len = payload.size();

  auto packet = st2110::make_audio_rtp_packet_view(rtp, st2110::ByteSpan(payload.data(), payload.size()), *policy);

  assert(!packet.has_value());
  assert(packet.error() == st2110::Error::InvalidValue);
}

} // namespace

int main() {
  test_wire_format_sample_sizes_are_explicit();
  test_policy_from_rx_audio_config_preserves_runtime_axes();
  test_policy_rejects_inconsistent_samples_per_packet();
  test_l16_payload_size_is_not_hardcoded_to_l24_or_stereo();
  test_make_audio_packet_view_accepts_matching_payload();
  test_make_audio_packet_view_rejects_payload_type_mismatch();
  test_make_audio_packet_view_rejects_payload_size_mismatch();
  return 0;
}
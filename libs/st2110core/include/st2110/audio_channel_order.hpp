#ifndef ST2110_OBS_PLUGIN_AUDIO_CHANNEL_ORDER_HPP
#define ST2110_OBS_PLUGIN_AUDIO_CHANNEL_ORDER_HPP

#include "audio_signaling.hpp"
#include "error.hpp"

#include <cstdint>
#include <expected>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace st2110 {
enum class AudioChannelGroupKind {
  Mono,
  Stereo,
  DualMono,
  MatrixStereo,
  FiveOne,
  SevenOne,
  TwentyTwoTwo,
  SdiGroup,
  Undefined,
  Other
};

struct AudioChannelOrderGroup {
  AudioChannelGroupKind kind = AudioChannelGroupKind::Other;
  std::string symbol;
  uint16_t channel_count = 0;
};

struct ParsedAudioChannelOrder {
  AudioChannelOrderConvention convention = AudioChannelOrderConvention::Unspecified;
  std::string raw_value;
  std::vector<AudioChannelOrderGroup> groups;
  uint16_t declared_channel_count = 0;
};

[[nodiscard]] inline bool is_audio_channel_order_digit(char c) { return c >= '0' && c <= '9'; }

[[nodiscard]] inline bool audio_channel_order_token_contains_ws(std::string_view token) {
  for (const char c : token) {
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      return true;
    }
  }

  return false;
}

[[nodiscard]] inline std::expected<uint16_t, Error>
parse_audio_channel_order_u_two_digit_count(std::string_view symbol) {
  if (symbol.size() != 3 || symbol[0] != 'U' || !is_audio_channel_order_digit(symbol[1]) ||
      !is_audio_channel_order_digit(symbol[2])) {
    return std::unexpected(Error::InvalidValue);
  }

  const uint16_t value =
      static_cast<uint16_t>((static_cast<uint16_t>(symbol[1] - '0') * 10U) + static_cast<uint16_t>(symbol[2] - '0'));

  if (value == 0 || value > 64) {
    return std::unexpected(Error::InvalidValue);
  }

  return value;
}

[[nodiscard]] inline std::expected<AudioChannelOrderGroup, Error>
make_audio_channel_order_undefined_group(uint16_t channel_count) {
  if (channel_count == 0 || channel_count > 64) {
    return std::unexpected(Error::InvalidValue);
  }

  std::string symbol = "U";
  symbol.push_back(static_cast<char>('0' + (channel_count / 10U)));
  symbol.push_back(static_cast<char>('0' + (channel_count % 10U)));

  return AudioChannelOrderGroup{
      .kind = AudioChannelGroupKind::Undefined, .symbol = std::move(symbol), .channel_count = channel_count};
}

[[nodiscard]] inline std::expected<AudioChannelOrderGroup, Error>
audio_channel_order_group_from_smpte2110_symbol(std::string_view symbol) {
  if (symbol.empty()) {
    return std::unexpected(Error::InvalidValue);
  }

  if (audio_channel_order_token_contains_ws(symbol)) {
    return std::unexpected(Error::InvalidValue);
  }

  if (symbol == "M") {
    return AudioChannelOrderGroup{
        .kind = AudioChannelGroupKind::Mono, .symbol = std::string(symbol), .channel_count = 1};
  }

  if (symbol == "ST") {
    return AudioChannelOrderGroup{
        .kind = AudioChannelGroupKind::Stereo, .symbol = std::string(symbol), .channel_count = 2};
  }

  if (symbol == "DM") {
    return AudioChannelOrderGroup{
        .kind = AudioChannelGroupKind::DualMono, .symbol = std::string(symbol), .channel_count = 2};
  }

  if (symbol == "LtRt") {
    return AudioChannelOrderGroup{
        .kind = AudioChannelGroupKind::MatrixStereo, .symbol = std::string(symbol), .channel_count = 2};
  }

  if (symbol == "51") {
    return AudioChannelOrderGroup{
        .kind = AudioChannelGroupKind::FiveOne, .symbol = std::string(symbol), .channel_count = 6};
  }

  if (symbol == "71") {
    return AudioChannelOrderGroup{
        .kind = AudioChannelGroupKind::SevenOne, .symbol = std::string(symbol), .channel_count = 8};
  }

  if (symbol == "222") {
    return AudioChannelOrderGroup{
        .kind = AudioChannelGroupKind::TwentyTwoTwo, .symbol = std::string(symbol), .channel_count = 24};
  }

  if (symbol == "SGRP") {
    return AudioChannelOrderGroup{
        .kind = AudioChannelGroupKind::SdiGroup, .symbol = std::string(symbol), .channel_count = 4};
  }

  if (symbol.starts_with("U")) {
    auto count = parse_audio_channel_order_u_two_digit_count(symbol);

    if (!count.has_value()) {
      return std::unexpected(count.error());
    }

    return AudioChannelOrderGroup{
        .kind = AudioChannelGroupKind::Undefined, .symbol = std::string(symbol), .channel_count = *count};
  }

  return std::unexpected(Error::Unsupported);
}

[[nodiscard]] inline std::expected<ParsedAudioChannelOrder, Error>
parse_smpte2110_audio_channel_order_raw_value(std::string_view raw_value) {
  static constexpr std::string_view prefix = "SMPTE2110.";

  if (!raw_value.starts_with(prefix)) {
    return std::unexpected(Error::InvalidValue);
  }

  std::string_view groups_text = raw_value.substr(prefix.size());

  if (groups_text.size() < 3 || groups_text.front() != '(' || groups_text.back() != ')') {
    return std::unexpected(Error::InvalidValue);
  }

  groups_text.remove_prefix(1);
  groups_text.remove_suffix(1);

  if (groups_text.empty()) {
    return std::unexpected(Error::InvalidValue);
  }

  ParsedAudioChannelOrder parsed{};
  parsed.convention = AudioChannelOrderConvention::Smpte2110;
  parsed.raw_value = std::string(raw_value);

  std::size_t group_start = 0;

  while (group_start <= groups_text.size()) {
    const std::size_t separator_pos = groups_text.find(',', group_start);

    const std::string_view symbol = separator_pos == std::string_view::npos
                                        ? groups_text.substr(group_start)
                                        : groups_text.substr(group_start, separator_pos - group_start);

    if (symbol.empty()) {
      return std::unexpected(Error::InvalidValue);
    }

    auto group = audio_channel_order_group_from_smpte2110_symbol(symbol);

    if (!group.has_value()) {
      return std::unexpected(group.error());
    }

    const uint32_t next_declared_count =
        static_cast<uint32_t>(parsed.declared_channel_count) + static_cast<uint32_t>(group->channel_count);

    if (next_declared_count > std::numeric_limits<uint16_t>::max()) {
      return std::unexpected(Error::InvalidValue);
    }

    parsed.declared_channel_count = static_cast<uint16_t>(next_declared_count);
    parsed.groups.push_back(std::move(*group));

    if (separator_pos == std::string_view::npos) {
      break;
    }

    group_start = separator_pos + 1;

    if (group_start > groups_text.size()) {
      return std::unexpected(Error::InvalidValue);
    }
  }

  if (parsed.groups.empty() || parsed.declared_channel_count == 0) {
    return std::unexpected(Error::InvalidValue);
  }

  return parsed;
}

[[nodiscard]] inline std::expected<ParsedAudioChannelOrder, Error>
parse_audio_channel_order_signaling(const AudioChannelOrderSignaling &channel_order) {
  if (Error err = validate_audio_channel_order_signaling(channel_order); err != Error::Ok) {
    return std::unexpected(err);
  }

  switch (channel_order.convention) {
  case AudioChannelOrderConvention::Unspecified: {
    ParsedAudioChannelOrder parsed{};
    parsed.convention = AudioChannelOrderConvention::Unspecified;
    parsed.raw_value = channel_order.raw_value;
    return parsed;
  }

  case AudioChannelOrderConvention::Smpte2110:
    return parse_smpte2110_audio_channel_order_raw_value(channel_order.raw_value);

  case AudioChannelOrderConvention::Other: {
    ParsedAudioChannelOrder parsed{};
    parsed.convention = AudioChannelOrderConvention::Other;
    parsed.raw_value = channel_order.raw_value;
    return parsed;
  }

  default:
    return std::unexpected(Error::InvalidValue);
  }
}

[[nodiscard]] inline Error
validate_audio_channel_order_against_channel_count(const AudioChannelOrderSignaling &channel_order,
                                                   uint16_t channel_count) {
  if (channel_count == 0) {
    return Error::InvalidValue;
  }

  auto parsed = parse_audio_channel_order_signaling(channel_order);

  if (!parsed.has_value()) {
    return parsed.error();
  }

  if (parsed->convention == AudioChannelOrderConvention::Smpte2110 && parsed->declared_channel_count > channel_count) {
    return Error::InvalidValue;
  }

  return Error::Ok;
}

[[nodiscard]] inline std::expected<ParsedAudioChannelOrder, Error>
effective_audio_channel_order_from_audio_stream_signaling(const AudioStreamSignaling &signaling) {
  if (Error err = validate_audio_media_description_against_conformance_range(signaling.media,
                                                                             audio_level_a_receiver_baseline());
      err != Error::Ok) {
    return std::unexpected(err);
  }

  const uint16_t channel_count = signaling.media.channel_count;

  if (!signaling.channel_order.has_value() ||
      signaling.channel_order->convention == AudioChannelOrderConvention::Unspecified) {
    auto undefined_group = make_audio_channel_order_undefined_group(channel_count);

    if (!undefined_group.has_value()) {
      return std::unexpected(undefined_group.error());
    }

    ParsedAudioChannelOrder effective{};
    effective.convention = AudioChannelOrderConvention::Unspecified;
    effective.raw_value = "";
    effective.groups.push_back(std::move(*undefined_group));
    effective.declared_channel_count = channel_count;

    return effective;
  }

  if (Error err = validate_audio_channel_order_against_channel_count(*signaling.channel_order, channel_count);
      err != Error::Ok) {
    return std::unexpected(err);
  }

  auto parsed = parse_audio_channel_order_signaling(*signaling.channel_order);

  if (!parsed.has_value()) {
    return std::unexpected(parsed.error());
  }

  if (parsed->convention != AudioChannelOrderConvention::Smpte2110) {
    return parsed;
  }

  if (parsed->declared_channel_count < channel_count) {
    const uint16_t undefined_remainder = static_cast<uint16_t>(channel_count - parsed->declared_channel_count);

    auto undefined_group = make_audio_channel_order_undefined_group(undefined_remainder);

    if (!undefined_group.has_value()) {
      return std::unexpected(undefined_group.error());
    }

    parsed->groups.push_back(std::move(*undefined_group));
    parsed->declared_channel_count = channel_count;
  }

  return parsed;
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_AUDIO_CHANNEL_ORDER_HPP
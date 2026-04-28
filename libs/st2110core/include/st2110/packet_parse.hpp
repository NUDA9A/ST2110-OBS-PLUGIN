#ifndef ST2110_OBS_PLUGIN_PACKET_PARSE_HPP
#define ST2110_OBS_PLUGIN_PACKET_PARSE_HPP

#include "error.hpp"
#include "bytes.hpp"
#include "packet_view.hpp"

#include <cstdint>
#include <optional>
#include <expected>

namespace st2110 {
inline constexpr std::size_t udpHeaderBytes = 8;
inline constexpr std::size_t standardUdpDatagramSizeLimitBytes = 1460;
inline constexpr std::size_t extendedUdpDatagramSizeLimitBytes = 8960;
inline constexpr std::size_t minRtpHeaderBytes = 12;
inline constexpr std::size_t minParsableUdpDatagramBytes = udpHeaderBytes + minRtpHeaderBytes;

struct PacketParsePolicy {
  std::optional<std::size_t> max_udp_datagram_bytes{};
};

[[nodiscard]] inline std::size_t udp_datagram_size_bytes(ByteSpan udp_payload) {
  return udp_payload.size() + udpHeaderBytes;
}

[[nodiscard]] inline std::size_t effective_max_udp_datagram_bytes(const PacketParsePolicy &policy) {
  return policy.max_udp_datagram_bytes.value_or(standardUdpDatagramSizeLimitBytes);
}

[[nodiscard]] inline Error validate_packet_parse_policy_config(const PacketParsePolicy &policy) {
  if (policy.max_udp_datagram_bytes.has_value() && *policy.max_udp_datagram_bytes < minParsableUdpDatagramBytes) {
    return Error::InvalidValue;
  }

  return Error::Ok;
}

[[nodiscard]] inline Error validate_packet_parse_policy(ByteSpan udp_payload, const PacketParsePolicy &policy) {
  if (udp_datagram_size_bytes(udp_payload) > effective_max_udp_datagram_bytes(policy)) {
    return Error::InvalidValue;
  }

  return Error::Ok;
}

[[nodiscard]] inline std::expected<PacketView, Error> parse_packet_view(ByteSpan udp_payload,
                                                                        const PacketParsePolicy &policy = {}) {
  if (Error err = validate_packet_parse_policy_config(policy); err != Error::Ok) {
    return std::unexpected(err);
  }
  if (Error err = validate_packet_parse_policy(udp_payload, policy); err != Error::Ok) {
    return std::unexpected(err);
  }

  return PacketView::from_udp_datagram(udp_payload);
}

[[nodiscard]] inline std::expected<PacketView, Error> parse_packet_view(ByteSpan udp_payload, PacketParseStats &stats,
                                                                        const PacketParsePolicy &policy = {}) {
  if (Error err = validate_packet_parse_policy_config(policy); err != Error::Ok) {
    record_packet_parse_result(stats, err, PacketParseStage::PacketPolicy);
    return std::unexpected(err);
  }
  if (Error err = validate_packet_parse_policy(udp_payload, policy); err != Error::Ok) {
    record_packet_parse_result(stats, err, PacketParseStage::PacketPolicy);
    return std::unexpected(err);
  }

  auto res = parse_packet_view_staged(udp_payload);
  if (!res.has_value()) {
    record_packet_parse_result(stats, res.error().error, res.error().stage);
    return std::unexpected(res.error().error);
  }

  record_packet_parse_result(stats, Error::Ok, PacketParseStage::RtpHeader);
  return *res;
}

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_PACKET_PARSE_HPP

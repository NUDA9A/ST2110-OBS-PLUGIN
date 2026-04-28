#include "st2110/video_sdp_media_section.hpp"

#include <cassert>
#include <cstdint>
#include <string>

using namespace st2110;

namespace {
constexpr uint8_t kPayloadType = 112;

std::string valid_fmtp_line() {
  return "a=fmtp:112 "
         "sampling=YCbCr-4:2:2; "
         "width=1920; "
         "height=1080; "
         "exactframerate=25; "
         "depth=8; "
         "colorimetry=BT709; "
         "PM=2110GPM; "
         "SSN=ST2110-20:2017; "
         "TCS=SDR\n";
}

std::string make_video_sdp(const std::string &session_connection = {},
                           const std::string &media_connection = "c=IN IP4 239.1.1.1\n") {
  std::string sdp;
  sdp += "v=0\n";
  sdp += "o=- 0 0 IN IP4 127.0.0.1\n";
  sdp += "s=ST2110 test\n";
  sdp += "t=0 0\n";
  sdp += session_connection;
  sdp += "m=video 50000 RTP/AVP 112\n";
  sdp += media_connection;
  sdp += "a=mid:primary\n";
  sdp += "a=rtpmap:112 raw/90000\n";
  sdp += valid_fmtp_line();
  return sdp;
}

void parses_unicast_connection_address_without_parameters() {
  auto parsed = parse_connection_data("c=IN IP4 192.0.2.10");

  assert(parsed.has_value());

  assert(parsed->network_type == "IN");
  assert(parsed->address_type == "IP4");
  assert(parsed->connection_address == "192.0.2.10");
  assert(parsed->base_address == "192.0.2.10");
  assert(!parsed->ttl.has_value());
  assert(!parsed->address_count.has_value());
}

void parses_ipv4_multicast_connection_address_with_ttl() {
  auto parsed = parse_connection_data("c=IN IP4 239.1.1.1/32");

  assert(parsed.has_value());

  assert(parsed->network_type == "IN");
  assert(parsed->address_type == "IP4");
  assert(parsed->connection_address == "239.1.1.1/32");
  assert(parsed->base_address == "239.1.1.1");
  assert(parsed->ttl.has_value());
  assert(*parsed->ttl == 32);
  assert(!parsed->address_count.has_value());
}

void parses_ipv4_multicast_connection_address_with_ttl_and_address_count() {
  auto parsed = parse_connection_data("c=IN IP4 239.1.1.1/32/4");

  assert(parsed.has_value());

  assert(parsed->network_type == "IN");
  assert(parsed->address_type == "IP4");
  assert(parsed->connection_address == "239.1.1.1/32/4");
  assert(parsed->base_address == "239.1.1.1");
  assert(parsed->ttl.has_value());
  assert(*parsed->ttl == 32);
  assert(parsed->address_count.has_value());
  assert(*parsed->address_count == 4);
}

void rejects_empty_connection_address_base() {
  auto parsed = parse_connection_data("c=IN IP4 /32");

  assert(!parsed.has_value());
  assert(parsed.error() == Error::InvalidValue);
}

void rejects_empty_ttl_parameter() {
  auto parsed = parse_connection_data("c=IN IP4 239.1.1.1/");

  assert(!parsed.has_value());
  assert(parsed.error() == Error::InvalidValue);
}

void rejects_non_numeric_ttl_parameter() {
  auto parsed = parse_connection_data("c=IN IP4 239.1.1.1/not-a-ttl");

  assert(!parsed.has_value());
  assert(parsed.error() == Error::InvalidValue);
}

void rejects_out_of_range_ttl_parameter() {
  auto parsed = parse_connection_data("c=IN IP4 239.1.1.1/256");

  assert(!parsed.has_value());
  assert(parsed.error() == Error::InvalidValue);
}

void rejects_empty_address_count_parameter() {
  auto parsed = parse_connection_data("c=IN IP4 239.1.1.1/32/");

  assert(!parsed.has_value());
  assert(parsed.error() == Error::InvalidValue);
}

void rejects_non_numeric_address_count_parameter() {
  auto parsed = parse_connection_data("c=IN IP4 239.1.1.1/32/not-a-count");

  assert(!parsed.has_value());
  assert(parsed.error() == Error::InvalidValue);
}

void rejects_zero_address_count_parameter() {
  auto parsed = parse_connection_data("c=IN IP4 239.1.1.1/32/0");

  assert(!parsed.has_value());
  assert(parsed.error() == Error::InvalidValue);
}

void rejects_too_many_connection_address_parameters() {
  auto parsed = parse_connection_data("c=IN IP4 239.1.1.1/32/2/extra");

  assert(!parsed.has_value());
  assert(parsed.error() == Error::InvalidValue);
}

void preserves_session_and_media_connection_behavior() {
  const std::string sdp = make_video_sdp("c=IN IP4 239.10.10.10/32/2\n", "c=IN IP4 239.20.20.20/64\n");

  auto raw = select_raw_video_sdp_media_section(sdp, kPayloadType);

  assert(raw.has_value());

  assert(raw->session_connection.has_value());
  assert(raw->session_connection->network_type == "IN");
  assert(raw->session_connection->address_type == "IP4");
  assert(raw->session_connection->connection_address == "239.10.10.10/32/2");
  assert(raw->session_connection->base_address == "239.10.10.10");
  assert(raw->session_connection->ttl.has_value());
  assert(*raw->session_connection->ttl == 32);
  assert(raw->session_connection->address_count.has_value());
  assert(*raw->session_connection->address_count == 2);

  assert(raw->media_connection.has_value());
  assert(raw->media_connection->network_type == "IN");
  assert(raw->media_connection->address_type == "IP4");
  assert(raw->media_connection->connection_address == "239.20.20.20/64");
  assert(raw->media_connection->base_address == "239.20.20.20");
  assert(raw->media_connection->ttl.has_value());
  assert(*raw->media_connection->ttl == 64);
  assert(!raw->media_connection->address_count.has_value());
}

void malformed_session_connection_is_rejected_by_media_section_parser() {
  const std::string sdp = make_video_sdp("c=IN IP4 239.10.10.10/not-a-ttl\n");

  auto raw = select_raw_video_sdp_media_section(sdp, kPayloadType);

  assert(!raw.has_value());
  assert(raw.error() == Error::InvalidValue);
}

void malformed_media_connection_is_rejected_by_media_section_parser() {
  const std::string sdp = make_video_sdp({}, "c=IN IP4 239.20.20.20/32/0\n");

  auto raw = select_raw_video_sdp_media_section(sdp, kPayloadType);

  assert(!raw.has_value());
  assert(raw.error() == Error::InvalidValue);
}
} // namespace

int main() {
  parses_unicast_connection_address_without_parameters();
  parses_ipv4_multicast_connection_address_with_ttl();
  parses_ipv4_multicast_connection_address_with_ttl_and_address_count();

  rejects_empty_connection_address_base();
  rejects_empty_ttl_parameter();
  rejects_non_numeric_ttl_parameter();
  rejects_out_of_range_ttl_parameter();
  rejects_empty_address_count_parameter();
  rejects_non_numeric_address_count_parameter();
  rejects_zero_address_count_parameter();
  rejects_too_many_connection_address_parameters();

  preserves_session_and_media_connection_behavior();
  malformed_session_connection_is_rejected_by_media_section_parser();
  malformed_media_connection_is_rejected_by_media_section_parser();

  return 0;
}
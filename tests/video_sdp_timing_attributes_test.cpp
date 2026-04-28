#include "st2110/video_sdp_media_section.hpp"
#include "st2110/video_sdp_timing_attributes.hpp"

#include <cassert>
#include <string>

using namespace st2110;

static void test_parses_known_ptp_reference_clock() {
  auto parsed = parse_video_sdp_reference_clock("ptp=IEEE1588-2008:00-11-22-33-44-55-66-77:127");

  assert(parsed.has_value());

  const auto &refclk = *parsed;
  assert(refclk.kind == RawVideoSdpReferenceClock::Kind::Ptp);
  assert(refclk.raw_value == "ptp=IEEE1588-2008:00-11-22-33-44-55-66-77:127");
  assert(refclk.ptp.has_value());
  assert(refclk.local_mac == std::nullopt);

  assert(refclk.ptp->version == "IEEE1588-2008");
  assert(refclk.ptp->gmid == "00-11-22-33-44-55-66-77");
  assert(refclk.ptp->domain.has_value());
  assert(*refclk.ptp->domain == 127);
}

static void test_parses_ptp_reference_clock_without_domain() {
  auto parsed = parse_video_sdp_reference_clock("ptp=IEEE1588-2008:00-11-22-33-44-55-66-77");

  assert(parsed.has_value());

  const auto &refclk = *parsed;
  assert(refclk.kind == RawVideoSdpReferenceClock::Kind::Ptp);
  assert(refclk.ptp.has_value());
  assert(refclk.ptp->version == "IEEE1588-2008");
  assert(refclk.ptp->gmid == "00-11-22-33-44-55-66-77");
  assert(!refclk.ptp->domain.has_value());
}

static void test_rejects_ptp_reference_clock_without_gmid() {
  auto parsed = parse_video_sdp_reference_clock("ptp=IEEE1588-2008");

  assert(!parsed.has_value());
  assert(parsed.error() == Error::InvalidValue);
}

static void test_rejects_ptp_reference_clock_without_version() {
  auto parsed = parse_video_sdp_reference_clock("ptp=:00-11-22-33-44-55-66-77");

  assert(!parsed.has_value());
  assert(parsed.error() == Error::InvalidValue);
}

static void test_parses_localmac_reference_clock() {
  auto parsed = parse_video_sdp_reference_clock("localmac=aa-bb-cc-dd-ee-ff");

  assert(parsed.has_value());

  const auto &refclk = *parsed;
  assert(refclk.kind == RawVideoSdpReferenceClock::Kind::LocalMac);
  assert(refclk.raw_value == "localmac=aa-bb-cc-dd-ee-ff");
  assert(refclk.local_mac.has_value());
  assert(*refclk.local_mac == "aa-bb-cc-dd-ee-ff");
  assert(!refclk.ptp.has_value());
}

static void test_preserves_unknown_reference_clock_as_other() {
  auto parsed = parse_video_sdp_reference_clock("something-else=value");

  assert(parsed.has_value());

  const auto &refclk = *parsed;
  assert(refclk.kind == RawVideoSdpReferenceClock::Kind::Other);
  assert(refclk.raw_value == "something-else=value");
  assert(!refclk.ptp.has_value());
  assert(!refclk.local_mac.has_value());
}

static void test_preserves_unknown_reference_clock_without_equals_as_other() {
  auto parsed = parse_video_sdp_reference_clock("custom-clock");

  assert(parsed.has_value());

  const auto &refclk = *parsed;
  assert(refclk.kind == RawVideoSdpReferenceClock::Kind::Other);
  assert(refclk.raw_value == "custom-clock");
  assert(!refclk.ptp.has_value());
  assert(!refclk.local_mac.has_value());
}

static void test_rejects_malformed_ptp_reference_clock_domain() {
  auto parsed = parse_video_sdp_reference_clock("ptp=IEEE1588-2008:00-11-22-33-44-55-66-77:999");

  assert(!parsed.has_value());
  assert(parsed.error() == Error::InvalidValue);
}

static void test_parses_known_media_clock_direct() {
  auto parsed = parse_video_sdp_media_clock("direct=123456");

  assert(parsed.has_value());

  const auto &mediaclk = *parsed;
  assert(mediaclk.kind == RawVideoSdpMediaClock::Kind::Direct);
  assert(mediaclk.raw_value == "direct=123456");
  assert(mediaclk.direct_offset.has_value());
  assert(*mediaclk.direct_offset == 123456);
}

static void test_parses_large_u64_media_clock_direct() {
  auto parsed = parse_video_sdp_media_clock("direct=4294967296");

  assert(parsed.has_value());

  const auto &mediaclk = *parsed;
  assert(mediaclk.kind == RawVideoSdpMediaClock::Kind::Direct);
  assert(mediaclk.direct_offset.has_value());
  assert(*mediaclk.direct_offset == 4294967296ULL);
}

static void test_parses_known_media_clock_sender() {
  auto parsed = parse_video_sdp_media_clock("sender");

  assert(parsed.has_value());

  const auto &mediaclk = *parsed;
  assert(mediaclk.kind == RawVideoSdpMediaClock::Kind::Sender);
  assert(mediaclk.raw_value == "sender");
  assert(!mediaclk.direct_offset.has_value());
}

static void test_preserves_unknown_media_clock_as_other() {
  auto parsed = parse_video_sdp_media_clock("capture=relative");

  assert(parsed.has_value());

  const auto &mediaclk = *parsed;
  assert(mediaclk.kind == RawVideoSdpMediaClock::Kind::Other);
  assert(mediaclk.raw_value == "capture=relative");
  assert(!mediaclk.direct_offset.has_value());
}

static void test_preserves_unknown_media_clock_without_equals_as_other() {
  auto parsed = parse_video_sdp_media_clock("capture");

  assert(parsed.has_value());

  const auto &mediaclk = *parsed;
  assert(mediaclk.kind == RawVideoSdpMediaClock::Kind::Other);
  assert(mediaclk.raw_value == "capture");
  assert(!mediaclk.direct_offset.has_value());
}

static void test_rejects_malformed_media_clock_direct_value() {
  auto parsed = parse_video_sdp_media_clock("direct=abc");

  assert(!parsed.has_value());
  assert(parsed.error() == Error::InvalidValue);
}

static void test_parses_timestamp_mode_token() {
  auto parsed = parse_video_sdp_timestamp_mode("NEW");

  assert(parsed.has_value());
  assert(parsed->raw_token == "NEW");
}

static void test_rejects_empty_timestamp_mode_token() {
  auto parsed = parse_video_sdp_timestamp_mode("   \t  ");

  assert(!parsed.has_value());
  assert(parsed.error() == Error::InvalidValue);
}

static void test_parses_sender_type_token() {
  auto parsed = parse_video_sdp_sender_type("2110TPN");

  assert(parsed.has_value());
  assert(parsed->raw_token == "2110TPN");
}

static void test_parses_numeric_timing_values() {
  auto tsdelay = parse_video_sdp_ts_delay("1800");
  assert(tsdelay.has_value());
  assert(*tsdelay == 1800);

  auto troff = parse_video_sdp_troff("42");
  assert(troff.has_value());
  assert(*troff == 42);

  auto cmax = parse_video_sdp_cmax("7");
  assert(cmax.has_value());
  assert(*cmax == 7);
}

static void test_parses_large_u64_tsdelay() {
  auto tsdelay = parse_video_sdp_ts_delay("4294967296");

  assert(tsdelay.has_value());
  assert(*tsdelay == 4294967296ULL);
}

static void test_rejects_malformed_numeric_timing_values() {
  auto tsdelay = parse_video_sdp_ts_delay("18x00");
  assert(!tsdelay.has_value());
  assert(tsdelay.error() == Error::InvalidValue);

  auto troff = parse_video_sdp_troff("");
  assert(!troff.has_value());
  assert(troff.error() == Error::InvalidValue);

  auto cmax = parse_video_sdp_cmax("999999999999999999999999");
  assert(!cmax.has_value());
  assert(cmax.error() == Error::InvalidValue);
}

static void test_parses_aggregate_timing_attributes_from_legacy_raw_media_section_fields() {
  RawVideoSdpMediaSection raw{};
  raw.ts_refclk = std::string("ptp=IEEE1588-2008:00-11-22-33-44-55-66-77:127");
  raw.mediaclk = std::string("direct=0");
  raw.tsmode = std::string("NEW");
  raw.tsdelay = std::string("1800");
  raw.tp = std::string("2110TPN");
  raw.troff = std::string("42");
  raw.cmax = std::string("7");

  auto parsed = parse_video_sdp_timing_attributes(raw);

  assert(parsed.has_value());

  const auto &timing = *parsed;

  assert(timing.reference_clock.has_value());
  assert(timing.reference_clock->scope == RawSdpAttributeScope::Media);
  assert(timing.reference_clock->value.kind == RawVideoSdpReferenceClock::Kind::Ptp);
  assert(timing.reference_clock->value.ptp.has_value());
  assert(timing.reference_clock->value.ptp->version == "IEEE1588-2008");
  assert(timing.reference_clock->value.ptp->gmid == "00-11-22-33-44-55-66-77");
  assert(timing.reference_clock->value.ptp->domain.has_value());
  assert(*timing.reference_clock->value.ptp->domain == 127);

  assert(timing.media_clock.has_value());
  assert(timing.media_clock->scope == RawSdpAttributeScope::Media);
  assert(timing.media_clock->value.kind == RawVideoSdpMediaClock::Kind::Direct);
  assert(timing.media_clock->value.direct_offset.has_value());
  assert(*timing.media_clock->value.direct_offset == 0);

  assert(timing.timestamp_mode.has_value());
  assert(timing.timestamp_mode->scope == RawSdpAttributeScope::Media);
  assert(timing.timestamp_mode->value.raw_token == "NEW");

  assert(timing.ts_delay_sender_ticks.has_value());
  assert(timing.ts_delay_sender_ticks->scope == RawSdpAttributeScope::Media);
  assert(timing.ts_delay_sender_ticks->value == 1800);

  assert(timing.sender_type.has_value());
  assert(timing.sender_type->scope == RawSdpAttributeScope::Media);
  assert(timing.sender_type->value.raw_token == "2110TPN");

  assert(timing.troff_us.has_value());
  assert(timing.troff_us->scope == RawSdpAttributeScope::Media);
  assert(timing.troff_us->value == 42);

  assert(timing.cmax.has_value());
  assert(timing.cmax->scope == RawSdpAttributeScope::Media);
  assert(timing.cmax->value == 7);
}

static void test_parses_aggregate_timing_attributes_from_scoped_session_fields() {
  RawVideoSdpMediaSection raw{};
  raw.session_ts_refclk = RawSdpScopedAttributeValue{.value = "ptp=IEEE1588-2008:00-11-22-33-44-55-66-77:127",
                                                     .scope = RawSdpAttributeScope::Session};
  raw.session_mediaclk = RawSdpScopedAttributeValue{.value = "direct=0", .scope = RawSdpAttributeScope::Session};
  raw.session_tsmode = RawSdpScopedAttributeValue{.value = "NEW", .scope = RawSdpAttributeScope::Session};
  raw.session_tsdelay = RawSdpScopedAttributeValue{.value = "1800", .scope = RawSdpAttributeScope::Session};
  raw.session_tp = RawSdpScopedAttributeValue{.value = "2110TPN", .scope = RawSdpAttributeScope::Session};
  raw.session_troff = RawSdpScopedAttributeValue{.value = "42", .scope = RawSdpAttributeScope::Session};
  raw.session_cmax = RawSdpScopedAttributeValue{.value = "7", .scope = RawSdpAttributeScope::Session};

  auto parsed = parse_video_sdp_timing_attributes(raw);

  assert(parsed.has_value());

  const auto &timing = *parsed;

  assert(timing.reference_clock.has_value());
  assert(timing.reference_clock->scope == RawSdpAttributeScope::Session);
  assert(timing.reference_clock->value.kind == RawVideoSdpReferenceClock::Kind::Ptp);

  assert(timing.media_clock.has_value());
  assert(timing.media_clock->scope == RawSdpAttributeScope::Session);
  assert(timing.media_clock->value.kind == RawVideoSdpMediaClock::Kind::Direct);

  assert(timing.timestamp_mode.has_value());
  assert(timing.timestamp_mode->scope == RawSdpAttributeScope::Session);
  assert(timing.timestamp_mode->value.raw_token == "NEW");

  assert(timing.ts_delay_sender_ticks.has_value());
  assert(timing.ts_delay_sender_ticks->scope == RawSdpAttributeScope::Session);
  assert(timing.ts_delay_sender_ticks->value == 1800);

  assert(timing.sender_type.has_value());
  assert(timing.sender_type->scope == RawSdpAttributeScope::Session);
  assert(timing.sender_type->value.raw_token == "2110TPN");

  assert(timing.troff_us.has_value());
  assert(timing.troff_us->scope == RawSdpAttributeScope::Session);
  assert(timing.troff_us->value == 42);

  assert(timing.cmax.has_value());
  assert(timing.cmax->scope == RawSdpAttributeScope::Session);
  assert(timing.cmax->value == 7);
}

static void test_media_scoped_timing_values_override_session_scoped_values() {
  RawVideoSdpMediaSection raw{};
  raw.session_mediaclk = RawSdpScopedAttributeValue{.value = "sender", .scope = RawSdpAttributeScope::Session};
  raw.media_mediaclk = RawSdpScopedAttributeValue{.value = "direct=0", .scope = RawSdpAttributeScope::Media};

  raw.session_tsmode = RawSdpScopedAttributeValue{.value = "SAMP", .scope = RawSdpAttributeScope::Session};
  raw.media_tsmode = RawSdpScopedAttributeValue{.value = "PRES", .scope = RawSdpAttributeScope::Media};

  auto parsed = parse_video_sdp_timing_attributes(raw);

  assert(parsed.has_value());

  const auto &timing = *parsed;

  assert(timing.media_clock.has_value());
  assert(timing.media_clock->scope == RawSdpAttributeScope::Media);
  assert(timing.media_clock->value.kind == RawVideoSdpMediaClock::Kind::Direct);

  assert(timing.timestamp_mode.has_value());
  assert(timing.timestamp_mode->scope == RawSdpAttributeScope::Media);
  assert(timing.timestamp_mode->value.raw_token == "PRES");
}

static void test_aggregate_parser_leaves_absent_fields_empty() {
  RawVideoSdpMediaSection raw{};

  auto parsed = parse_video_sdp_timing_attributes(raw);

  assert(parsed.has_value());

  const auto &timing = *parsed;
  assert(!timing.reference_clock.has_value());
  assert(!timing.media_clock.has_value());
  assert(!timing.timestamp_mode.has_value());
  assert(!timing.ts_delay_sender_ticks.has_value());
  assert(!timing.sender_type.has_value());
  assert(!timing.troff_us.has_value());
  assert(!timing.cmax.has_value());
}

static void test_aggregate_parser_propagates_error_from_legacy_field() {
  RawVideoSdpMediaSection raw{};
  raw.tsdelay = std::string("not-a-number");

  auto parsed = parse_video_sdp_timing_attributes(raw);

  assert(!parsed.has_value());
  assert(parsed.error() == Error::InvalidValue);
}

static void test_aggregate_parser_propagates_error_from_scoped_field() {
  RawVideoSdpMediaSection raw{};
  raw.session_tsdelay = RawSdpScopedAttributeValue{.value = "not-a-number", .scope = RawSdpAttributeScope::Session};

  auto parsed = parse_video_sdp_timing_attributes(raw);

  assert(!parsed.has_value());
  assert(parsed.error() == Error::InvalidValue);
}

int main() {
  test_parses_known_ptp_reference_clock();
  test_parses_ptp_reference_clock_without_domain();
  test_rejects_ptp_reference_clock_without_gmid();
  test_rejects_ptp_reference_clock_without_version();
  test_parses_localmac_reference_clock();
  test_preserves_unknown_reference_clock_as_other();
  test_preserves_unknown_reference_clock_without_equals_as_other();
  test_rejects_malformed_ptp_reference_clock_domain();

  test_parses_known_media_clock_direct();
  test_parses_large_u64_media_clock_direct();
  test_parses_known_media_clock_sender();
  test_preserves_unknown_media_clock_as_other();
  test_preserves_unknown_media_clock_without_equals_as_other();
  test_rejects_malformed_media_clock_direct_value();

  test_parses_timestamp_mode_token();
  test_rejects_empty_timestamp_mode_token();
  test_parses_sender_type_token();

  test_parses_numeric_timing_values();
  test_parses_large_u64_tsdelay();
  test_rejects_malformed_numeric_timing_values();

  test_parses_aggregate_timing_attributes_from_legacy_raw_media_section_fields();
  test_parses_aggregate_timing_attributes_from_scoped_session_fields();
  test_media_scoped_timing_values_override_session_scoped_values();
  test_aggregate_parser_leaves_absent_fields_empty();
  test_aggregate_parser_propagates_error_from_legacy_field();
  test_aggregate_parser_propagates_error_from_scoped_field();

  return 0;
}
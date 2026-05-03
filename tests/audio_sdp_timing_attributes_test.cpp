#include <st2110/audio_sdp_timing_attributes.hpp>

#include <cassert>
#include <optional>
#include <string>

using namespace st2110;

static void test_parses_known_ptp_reference_clock() {
    auto parsed = parse_audio_sdp_reference_clock("ptp=IEEE1588-2008:00-11-22-33-44-55-66-77:127");

    assert(parsed.has_value());

    const auto &refclk = *parsed;
    assert(refclk.kind == RawAudioSdpReferenceClock::Kind::Ptp);
    assert(refclk.raw_value == "ptp=IEEE1588-2008:00-11-22-33-44-55-66-77:127");
    assert(refclk.ptp.has_value());
    assert(!refclk.local_mac.has_value());

    assert(refclk.ptp->version == "IEEE1588-2008");
    assert(refclk.ptp->gmid == "00-11-22-33-44-55-66-77");
    assert(refclk.ptp->domain.has_value());
    assert(*refclk.ptp->domain == 127);
}

static void test_parses_ptp_reference_clock_without_domain() {
    auto parsed = parse_audio_sdp_reference_clock("ptp=IEEE1588-2008:00-11-22-33-44-55-66-77");

    assert(parsed.has_value());

    const auto &refclk = *parsed;
    assert(refclk.kind == RawAudioSdpReferenceClock::Kind::Ptp);
    assert(refclk.ptp.has_value());
    assert(refclk.ptp->version == "IEEE1588-2008");
    assert(refclk.ptp->gmid == "00-11-22-33-44-55-66-77");
    assert(!refclk.ptp->domain.has_value());
}

static void test_rejects_ptp_reference_clock_without_gmid() {
    auto parsed = parse_audio_sdp_reference_clock("ptp=IEEE1588-2008");

    assert(!parsed.has_value());
    assert(parsed.error() == Error::InvalidValue);
}

static void test_rejects_ptp_reference_clock_without_version() {
    auto parsed = parse_audio_sdp_reference_clock("ptp=:00-11-22-33-44-55-66-77");

    assert(!parsed.has_value());
    assert(parsed.error() == Error::InvalidValue);
}

static void test_parses_localmac_reference_clock() {
    auto parsed = parse_audio_sdp_reference_clock("localmac=aa-bb-cc-dd-ee-ff");

    assert(parsed.has_value());

    const auto &refclk = *parsed;
    assert(refclk.kind == RawAudioSdpReferenceClock::Kind::LocalMac);
    assert(refclk.raw_value == "localmac=aa-bb-cc-dd-ee-ff");
    assert(refclk.local_mac.has_value());
    assert(*refclk.local_mac == "aa-bb-cc-dd-ee-ff");
    assert(!refclk.ptp.has_value());
}

static void test_preserves_unknown_reference_clock_as_other() {
    auto parsed = parse_audio_sdp_reference_clock("some-future-clock=value");

    assert(parsed.has_value());

    const auto &refclk = *parsed;
    assert(refclk.kind == RawAudioSdpReferenceClock::Kind::Other);
    assert(refclk.raw_value == "some-future-clock=value");
    assert(!refclk.ptp.has_value());
    assert(!refclk.local_mac.has_value());
}

static void test_rejects_malformed_ptp_reference_clock_domain() {
    auto parsed = parse_audio_sdp_reference_clock("ptp=IEEE1588-2008:00-11-22-33-44-55-66-77:999");

    assert(!parsed.has_value());
    assert(parsed.error() == Error::InvalidValue);
}

static void test_parses_known_media_clock_direct() {
    auto parsed = parse_audio_sdp_media_clock("direct=123456");

    assert(parsed.has_value());

    const auto &mediaclk = *parsed;
    assert(mediaclk.kind == RawAudioSdpMediaClock::Kind::Direct);
    assert(mediaclk.raw_value == "direct=123456");
    assert(mediaclk.direct_offset.has_value());
    assert(*mediaclk.direct_offset == 123456);
}

static void test_parses_known_media_clock_sender() {
    auto parsed = parse_audio_sdp_media_clock("sender");

    assert(parsed.has_value());

    const auto &mediaclk = *parsed;
    assert(mediaclk.kind == RawAudioSdpMediaClock::Kind::Sender);
    assert(mediaclk.raw_value == "sender");
    assert(!mediaclk.direct_offset.has_value());
}

static void test_preserves_unknown_media_clock_as_other() {
    auto parsed = parse_audio_sdp_media_clock("capture=relative");

    assert(parsed.has_value());

    const auto &mediaclk = *parsed;
    assert(mediaclk.kind == RawAudioSdpMediaClock::Kind::Other);
    assert(mediaclk.raw_value == "capture=relative");
    assert(!mediaclk.direct_offset.has_value());
}

static void test_rejects_malformed_media_clock_direct_value() {
    auto parsed = parse_audio_sdp_media_clock("direct=abc");

    assert(!parsed.has_value());
    assert(parsed.error() == Error::InvalidValue);
}

static void test_parses_aggregate_timing_attributes_from_raw_audio_sdp_media_section() {
    RawAudioSdpMediaSection raw{};
    raw.unknown_session_attributes.push_back(
        RawAudioSdpAttribute{.name = "ts-refclk", .value = "ptp=IEEE1588-2008:00-11-22-33-44-55-66-77:127"});
    raw.unknown_attributes.push_back(RawAudioSdpAttribute{.name = "mediaclk", .value = "direct=0"});

    auto parsed = parse_audio_sdp_timing_attributes(raw);

    assert(parsed.has_value());

    const auto &timing = *parsed;

    assert(timing.reference_clock.has_value());
    assert(timing.reference_clock->scope == RawAudioSdpTimingAttributeScope::Session);
    assert(timing.reference_clock->value.kind == RawAudioSdpReferenceClock::Kind::Ptp);
    assert(timing.reference_clock->value.ptp.has_value());
    assert(timing.reference_clock->value.ptp->version == "IEEE1588-2008");
    assert(timing.reference_clock->value.ptp->gmid == "00-11-22-33-44-55-66-77");
    assert(timing.reference_clock->value.ptp->domain.has_value());
    assert(*timing.reference_clock->value.ptp->domain == 127);

    assert(timing.media_clock.has_value());
    assert(timing.media_clock->scope == RawAudioSdpTimingAttributeScope::Media);
    assert(timing.media_clock->value.kind == RawAudioSdpMediaClock::Kind::Direct);
    assert(timing.media_clock->value.direct_offset.has_value());
    assert(*timing.media_clock->value.direct_offset == 0);

    assert(raw_audio_sdp_has_reference_clock(timing));
    assert(raw_audio_sdp_has_media_level_mediaclk(timing));
}

static void test_media_scoped_mediaclk_overrides_session_scoped_value() {
    RawAudioSdpMediaSection raw{};
    raw.unknown_session_attributes.push_back(RawAudioSdpAttribute{.name = "mediaclk", .value = "sender"});
    raw.unknown_attributes.push_back(RawAudioSdpAttribute{.name = "mediaclk", .value = "direct=0"});

    auto parsed = parse_audio_sdp_timing_attributes(raw);

    assert(parsed.has_value());

    const auto &timing = *parsed;
    assert(timing.media_clock.has_value());
    assert(timing.media_clock->scope == RawAudioSdpTimingAttributeScope::Media);
    assert(timing.media_clock->value.kind == RawAudioSdpMediaClock::Kind::Direct);
    assert(timing.media_clock->value.direct_offset.has_value());
    assert(*timing.media_clock->value.direct_offset == 0);
}

static void test_session_level_only_mediaclk_is_not_media_level_mediaclk() {
    RawAudioSdpMediaSection raw{};
    raw.unknown_session_attributes.push_back(RawAudioSdpAttribute{.name = "mediaclk", .value = "direct=0"});

    auto parsed = parse_audio_sdp_timing_attributes(raw);

    assert(parsed.has_value());

    const auto &timing = *parsed;
    assert(timing.media_clock.has_value());
    assert(timing.media_clock->scope == RawAudioSdpTimingAttributeScope::Session);
    assert(!raw_audio_sdp_has_media_level_mediaclk(timing));
}

static void test_duplicate_session_ts_refclk_is_rejected() {
    RawAudioSdpMediaSection raw{};
    raw.unknown_session_attributes.push_back(
        RawAudioSdpAttribute{.name = "ts-refclk", .value = "ptp=IEEE1588-2008:00-11-22-33-44-55-66-77:127"});
    raw.unknown_session_attributes.push_back(
        RawAudioSdpAttribute{.name = "ts-refclk", .value = "ptp=IEEE1588-2008:10-11-22-33-44-55-66-77:1"});

    auto parsed = parse_audio_sdp_timing_attributes(raw);

    assert(!parsed.has_value());
    assert(parsed.error() == Error::InvalidValue);
}

static void test_duplicate_media_mediaclk_is_rejected() {
    RawAudioSdpMediaSection raw{};
    raw.unknown_attributes.push_back(RawAudioSdpAttribute{.name = "mediaclk", .value = "direct=0"});
    raw.unknown_attributes.push_back(RawAudioSdpAttribute{.name = "mediaclk", .value = "sender"});

    auto parsed = parse_audio_sdp_timing_attributes(raw);

    assert(!parsed.has_value());
    assert(parsed.error() == Error::InvalidValue);
}

static void test_aggregate_parser_leaves_absent_fields_empty() {
    RawAudioSdpMediaSection raw{};

    auto parsed = parse_audio_sdp_timing_attributes(raw);

    assert(parsed.has_value());

    const auto &timing = *parsed;
    assert(!timing.reference_clock.has_value());
    assert(!timing.media_clock.has_value());
    assert(!raw_audio_sdp_has_reference_clock(timing));
    assert(!raw_audio_sdp_has_media_level_mediaclk(timing));
}

int main() {
    test_parses_known_ptp_reference_clock();
    test_parses_ptp_reference_clock_without_domain();
    test_rejects_ptp_reference_clock_without_gmid();
    test_rejects_ptp_reference_clock_without_version();
    test_parses_localmac_reference_clock();
    test_preserves_unknown_reference_clock_as_other();
    test_rejects_malformed_ptp_reference_clock_domain();

    test_parses_known_media_clock_direct();
    test_parses_known_media_clock_sender();
    test_preserves_unknown_media_clock_as_other();
    test_rejects_malformed_media_clock_direct_value();

    test_parses_aggregate_timing_attributes_from_raw_audio_sdp_media_section();
    test_media_scoped_mediaclk_overrides_session_scoped_value();
    test_session_level_only_mediaclk_is_not_media_level_mediaclk();
    test_duplicate_session_ts_refclk_is_rejected();
    test_duplicate_media_mediaclk_is_rejected();
    test_aggregate_parser_leaves_absent_fields_empty();

    return 0;
}
#include "st2110/video_sdp_fmtp.hpp"
#include "st2110/video_sdp_signaling_adapter.hpp"
#include "st2110/video_signaling.hpp"

#include <cassert>
#include <string>

using namespace st2110;

static RawVideoSdpFmtpParameters make_base_raw_fmtp() {
    RawVideoSdpFmtpParameters raw{};
    raw.sampling = "YCbCr-4:2:2";
    raw.width = 1920;
    raw.height = 1080;
    raw.exactframerate = RawVideoSdpExactFrameRate{.numerator = 60000, .denominator = 1001};
    raw.depth = 10;
    raw.colorimetry = "BT709";
    raw.packing_mode = "2110GPM";
    raw.signal_standard = "ST2110-20:2017";
    raw.transfer_characteristic_system = std::string("SDR");
    raw.range = std::string("FULL");
    raw.interlace = false;
    raw.segmented = false;
    return raw;
}

static void test_maps_progressive_known_values() {
    auto raw = make_base_raw_fmtp();

    auto signaling_expected = video_stream_signaling_from_raw_video_sdp_fmtp(raw);
    assert(signaling_expected.has_value());

    const auto &signaling = *signaling_expected;

    assert(signaling.scan_mode == VideoScanMode::Progressive);
    assert(signaling.packing_mode == VideoPackingMode::Gpm);

    assert(signaling.media.width == 1920);
    assert(signaling.media.height == 1080);
    assert(signaling.media.fps_num == 60000);
    assert(signaling.media.fps_den == 1001);

    assert(signaling.media.sampling.known == VideoSampling::Known::YCbCr422);
    assert(!signaling.media.sampling.raw_token.has_value());

    assert(signaling.media.depth.bits == 10);
    assert(signaling.media.depth.floating_point == false);

    assert(signaling.media.colorimetry.known == VideoColorimetry::Known::Bt709);
    assert(!signaling.media.colorimetry.raw_token.has_value());

    assert(signaling.media.transfer_characteristic_system.has_value());
    assert(signaling.media.transfer_characteristic_system->known == VideoTransferCharacteristicSystem::Known::SDR);
    assert(!signaling.media.transfer_characteristic_system->raw_token.has_value());

    assert(signaling.media.signal_standard.has_value());
    assert(signaling.media.signal_standard->known == VideoSignalStandard::Known::St2110_20_2017);
    assert(!signaling.media.signal_standard->raw_token.has_value());

    assert(signaling.media.range.has_value());
    assert(signaling.media.range->known == VideoRange::Known::Full);
    assert(!signaling.media.range->raw_token.has_value());

    assert(validate_video_media_description(signaling.media) == Error::Ok);
}

static void test_maps_fullprotect_range_as_known_value() {
    auto raw = make_base_raw_fmtp();
    raw.range = std::string("FULLPROTECT");

    auto signaling_expected = video_stream_signaling_from_raw_video_sdp_fmtp(raw);
    assert(signaling_expected.has_value());

    const auto &signaling = *signaling_expected;
    assert(signaling.media.range.has_value());
    assert(signaling.media.range->known == VideoRange::Known::FullProtect);
    assert(!signaling.media.range->raw_token.has_value());

    assert(validate_video_media_description(signaling.media) == Error::Ok);
}

static void test_maps_interlaced_scan_mode() {
    auto raw = make_base_raw_fmtp();
    raw.interlace = true;
    raw.segmented = false;

    auto signaling_expected = video_stream_signaling_from_raw_video_sdp_fmtp(raw);
    assert(signaling_expected.has_value());
    assert(signaling_expected->scan_mode == VideoScanMode::Interlaced);
}

static void test_maps_psf_scan_mode() {
    auto raw = make_base_raw_fmtp();
    raw.interlace = true;
    raw.segmented = true;

    auto signaling_expected = video_stream_signaling_from_raw_video_sdp_fmtp(raw);
    assert(signaling_expected.has_value());
    assert(signaling_expected->scan_mode == VideoScanMode::PsF);
}

static void test_rejects_segmented_without_interlace() {
    auto raw = make_base_raw_fmtp();
    raw.interlace = false;
    raw.segmented = true;

    auto signaling_expected = video_stream_signaling_from_raw_video_sdp_fmtp(raw);
    assert(!signaling_expected.has_value());
    assert(signaling_expected.error() == Error::InvalidValue);
}

static void test_maps_bpm_without_runtime_reject() {
    auto raw = make_base_raw_fmtp();
    raw.packing_mode = "2110BPM";

    auto signaling_expected = video_stream_signaling_from_raw_video_sdp_fmtp(raw);
    assert(signaling_expected.has_value());
    assert(signaling_expected->packing_mode == VideoPackingMode::Bpm);
}

static void test_rejects_unknown_packing_mode() {
    auto raw = make_base_raw_fmtp();
    raw.packing_mode = "SOMETHING-ELSE";

    auto signaling_expected = video_stream_signaling_from_raw_video_sdp_fmtp(raw);
    assert(!signaling_expected.has_value());
    assert(signaling_expected.error() == Error::InvalidValue);
}

static void test_preserves_unknown_open_ended_tokens_as_other() {
    auto raw = make_base_raw_fmtp();

    raw.sampling = "CUSTOM-SAMPLING";
    raw.colorimetry = "CUSTOM-COLOR";
    raw.transfer_characteristic_system = std::string("CUSTOM-TCS");
    raw.signal_standard = "CUSTOM-SSN";
    raw.range = std::string("CUSTOM-RANGE");

    auto signaling_expected = video_stream_signaling_from_raw_video_sdp_fmtp(raw);
    assert(signaling_expected.has_value());

    const auto &signaling = *signaling_expected;

    assert(signaling.media.sampling.known == VideoSampling::Known::Other);
    assert(signaling.media.sampling.raw_token.has_value());
    assert(*signaling.media.sampling.raw_token == "CUSTOM-SAMPLING");

    assert(signaling.media.colorimetry.known == VideoColorimetry::Known::Other);
    assert(signaling.media.colorimetry.raw_token.has_value());
    assert(*signaling.media.colorimetry.raw_token == "CUSTOM-COLOR");

    assert(signaling.media.transfer_characteristic_system.has_value());
    assert(signaling.media.transfer_characteristic_system->known == VideoTransferCharacteristicSystem::Known::Other);
    assert(signaling.media.transfer_characteristic_system->raw_token.has_value());
    assert(*signaling.media.transfer_characteristic_system->raw_token == "CUSTOM-TCS");

    assert(signaling.media.signal_standard.has_value());
    assert(signaling.media.signal_standard->known == VideoSignalStandard::Known::Other);
    assert(signaling.media.signal_standard->raw_token.has_value());
    assert(*signaling.media.signal_standard->raw_token == "CUSTOM-SSN");

    assert(signaling.media.range.has_value());
    assert(signaling.media.range->known == VideoRange::Known::Other);
    assert(signaling.media.range->raw_token.has_value());
    assert(*signaling.media.range->raw_token == "CUSTOM-RANGE");

    assert(validate_video_media_description(signaling.media) == Error::Ok);
}

static void test_rejects_depth_that_does_not_fit_signaling_model() {
    auto raw = make_base_raw_fmtp();
    raw.depth = 256;

    auto signaling_expected = video_stream_signaling_from_raw_video_sdp_fmtp(raw);
    assert(!signaling_expected.has_value());
    assert(signaling_expected.error() == Error::InvalidValue);
}

int main() {
    test_maps_progressive_known_values();
    test_maps_fullprotect_range_as_known_value();
    test_maps_interlaced_scan_mode();
    test_maps_psf_scan_mode();
    test_rejects_segmented_without_interlace();
    test_maps_bpm_without_runtime_reject();
    test_rejects_unknown_packing_mode();
    test_preserves_unknown_open_ended_tokens_as_other();
    test_rejects_depth_that_does_not_fit_signaling_model();
    return 0;
}
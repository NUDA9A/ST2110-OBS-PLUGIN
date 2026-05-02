// tests/video_sdp_media_property_enum_coverage_test.cpp

#include <cassert>
#include <expected>
#include <string_view>

#include "st2110/error.hpp"
#include "st2110/pixel_format.hpp"
#include "st2110/signaling_structs.hpp"
#include "st2110/video_sdp_fmtp.hpp"
#include "st2110/video_sdp_signaling_adapter.hpp"
#include "st2110/video_signaling.hpp"

using namespace st2110;

namespace {
RawVideoSdpFmtpParameters make_base_raw_fmtp() {
    RawVideoSdpFmtpParameters raw{};

    raw.sampling = "YCbCr-4:2:2";
    raw.width = 1920;
    raw.height = 1080;
    raw.exactframerate = RawVideoSdpExactFrameRate{.numerator = 25, .denominator = 1};
    raw.depth = 8;
    raw.depth_floating_point = false;
    raw.colorimetry = "BT709";
    raw.packing_mode = "2110GPM";
    raw.signal_standard = "ST2110-20:2017";

    return raw;
}

VideoMediaDescription map_media_or_fail(const RawVideoSdpFmtpParameters &raw) {
    auto mapped = video_media_description_from_raw_video_sdp_fmtp(raw);
    assert(mapped.has_value());
    return *mapped;
}

void assert_no_raw_token(const VideoSampling &sampling) { assert(!sampling.raw_token.has_value()); }

void assert_no_raw_token(const VideoColorimetry &colorimetry) { assert(!colorimetry.raw_token.has_value()); }

void assert_no_raw_token(const VideoTransferCharacteristicSystem &tcs) { assert(!tcs.raw_token.has_value()); }

void assert_no_raw_token(const VideoRange &range) { assert(!range.raw_token.has_value()); }

void sampling_known_tokens_are_explicit_enums() {
    struct Case {
        std::string_view token;
        VideoSampling::Known expected;
    };

    const Case cases[] = {
        {"YCbCr-4:2:2", VideoSampling::Known::YCbCr422},
        {"YCbCr-4:4:4", VideoSampling::Known::YCbCr444},
        {"YCbCr-4:2:0", VideoSampling::Known::YCbCr420},
        {"RGB", VideoSampling::Known::RGB},
        {"XYZ", VideoSampling::Known::XYZ},
        {"KEY", VideoSampling::Known::Key},

        {"CLYCbCr-4:4:4", VideoSampling::Known::CLYCbCr444},
        {"CLYCbCr-4:2:2", VideoSampling::Known::CLYCbCr422},
        {"CLYCbCr-4:2:0", VideoSampling::Known::CLYCbCr420},
        {"ICtCp-4:4:4", VideoSampling::Known::ICtCp444},
        {"ICtCp-4:2:2", VideoSampling::Known::ICtCp422},
        {"ICtCp-4:2:0", VideoSampling::Known::ICtCp420},
    };

    for (const auto &c : cases) {
        auto raw = make_base_raw_fmtp();
        raw.sampling = std::string(c.token);

        const auto media = map_media_or_fail(raw);

        assert(media.sampling.known == c.expected);
        assert_no_raw_token(media.sampling);
        assert(validate_video_sampling(media.sampling) == Error::Ok);
    }
}

void colorimetry_known_tokens_are_explicit_enums() {
    struct Case {
        std::string_view token;
        VideoColorimetry::Known expected;
    };

    const Case cases[] = {
        {"BT601", VideoColorimetry::Known::Bt601},       {"BT709", VideoColorimetry::Known::Bt709},
        {"BT2020", VideoColorimetry::Known::Bt2020},     {"BT2100", VideoColorimetry::Known::Bt2100},
        {"ST2065-1", VideoColorimetry::Known::St2065_1},

        {"ST2065-3", VideoColorimetry::Known::St2065_3}, {"UNSPECIFIED", VideoColorimetry::Known::Unspecified},
        {"XYZ", VideoColorimetry::Known::Xyz},           {"ALPHA", VideoColorimetry::Known::Alpha},
    };

    for (const auto &c : cases) {
        auto raw = make_base_raw_fmtp();
        raw.colorimetry = std::string(c.token);

        const auto media = map_media_or_fail(raw);

        assert(media.colorimetry.known == c.expected);
        assert_no_raw_token(media.colorimetry);
        assert(validate_video_colorimetry(media.colorimetry) == Error::Ok);
    }
}

void transfer_characteristic_system_known_tokens_are_explicit_enums() {
    struct Case {
        std::string_view token;
        VideoTransferCharacteristicSystem::Known expected;
    };

    const Case cases[] = {
        {"SDR", VideoTransferCharacteristicSystem::Known::SDR},
        {"PQ", VideoTransferCharacteristicSystem::Known::PQ},
        {"HLG", VideoTransferCharacteristicSystem::Known::HLG},
        {"LINEAR", VideoTransferCharacteristicSystem::Known::Linear},

        {"BT2100LINPQ", VideoTransferCharacteristicSystem::Known::Bt2100LinPq},
        {"BT2100LINHLG", VideoTransferCharacteristicSystem::Known::Bt2100LinHlg},
        {"ST2065-1", VideoTransferCharacteristicSystem::Known::St2065_1},
        {"ST428-1", VideoTransferCharacteristicSystem::Known::St428_1},
        {"DENSITY", VideoTransferCharacteristicSystem::Known::Density},
        {"ST2115LOGS3", VideoTransferCharacteristicSystem::Known::St2115LogS3},
        {"UNSPECIFIED", VideoTransferCharacteristicSystem::Known::Unspecified},
    };

    for (const auto &c : cases) {
        auto raw = make_base_raw_fmtp();
        raw.transfer_characteristic_system = std::string(c.token);

        const auto media = map_media_or_fail(raw);

        assert(media.transfer_characteristic_system.has_value());
        assert(media.transfer_characteristic_system->known == c.expected);
        assert_no_raw_token(*media.transfer_characteristic_system);
        assert(validate_video_transfer_characteristic_system(*media.transfer_characteristic_system) == Error::Ok);
    }
}

void range_known_tokens_are_explicit_enums() {
    struct Case {
        std::string_view token;
        VideoRange::Known expected;
    };

    const Case cases[] = {
        {"NARROW", VideoRange::Known::Narrow},
        {"FULLPROTECT", VideoRange::Known::FullProtect},
        {"FULL", VideoRange::Known::Full},
    };

    for (const auto &c : cases) {
        auto raw = make_base_raw_fmtp();
        raw.range = std::string(c.token);

        const auto media = map_media_or_fail(raw);

        assert(media.range.has_value());
        assert(media.range->known == c.expected);
        assert_no_raw_token(*media.range);
        assert(validate_video_range(*media.range) == Error::Ok);
    }
}

void unknown_future_tokens_are_preserved_as_other() {
    {
        auto raw = make_base_raw_fmtp();
        raw.sampling = "FUTURE-SAMPLING";

        const auto media = map_media_or_fail(raw);

        assert(media.sampling.known == VideoSampling::Known::Other);
        assert(media.sampling.raw_token.has_value());
        assert(*media.sampling.raw_token == "FUTURE-SAMPLING");
        assert(validate_video_sampling(media.sampling) == Error::Ok);
    }

    {
        auto raw = make_base_raw_fmtp();
        raw.colorimetry = "FUTURE-COLORIMETRY";

        const auto media = map_media_or_fail(raw);

        assert(media.colorimetry.known == VideoColorimetry::Known::Other);
        assert(media.colorimetry.raw_token.has_value());
        assert(*media.colorimetry.raw_token == "FUTURE-COLORIMETRY");
        assert(validate_video_colorimetry(media.colorimetry) == Error::Ok);
    }

    {
        auto raw = make_base_raw_fmtp();
        raw.transfer_characteristic_system = "FUTURE-TCS";

        const auto media = map_media_or_fail(raw);

        assert(media.transfer_characteristic_system.has_value());
        assert(media.transfer_characteristic_system->known == VideoTransferCharacteristicSystem::Known::Other);
        assert(media.transfer_characteristic_system->raw_token.has_value());
        assert(*media.transfer_characteristic_system->raw_token == "FUTURE-TCS");
        assert(validate_video_transfer_characteristic_system(*media.transfer_characteristic_system) == Error::Ok);
    }

    {
        auto raw = make_base_raw_fmtp();
        raw.range = "FUTURE-RANGE";

        const auto media = map_media_or_fail(raw);

        assert(media.range.has_value());
        assert(media.range->known == VideoRange::Known::Other);
        assert(media.range->raw_token.has_value());
        assert(*media.range->raw_token == "FUTURE-RANGE");
        assert(validate_video_range(*media.range) == Error::Ok);
    }
}

void runtime_projection_remains_localized_for_known_but_unsupported_sampling() {
    auto raw = make_base_raw_fmtp();
    raw.sampling = "CLYCbCr-4:2:2";
    raw.depth = 8;
    raw.depth_floating_point = false;

    const auto media = map_media_or_fail(raw);

    assert(media.sampling.known == VideoSampling::Known::CLYCbCr422);
    assert(!media.sampling.raw_token.has_value());
    assert(validate_video_media_description(media) == Error::Ok);

    VideoStreamSignaling signaling{};
    signaling.media = media;

    const auto projected = pixel_format_from_video_stream_signaling(signaling);

    assert(!projected.has_value());
    assert(projected.error() == Error::Unsupported);
}

void existing_runtime_projection_for_ycbcr_422_8bit_still_works() {
    auto raw = make_base_raw_fmtp();
    raw.sampling = "YCbCr-4:2:2";
    raw.depth = 8;
    raw.depth_floating_point = false;

    const auto media = map_media_or_fail(raw);

    assert(validate_video_media_description(media) == Error::Ok);

    VideoStreamSignaling signaling{};
    signaling.media = media;

    const auto projected = pixel_format_from_video_stream_signaling(signaling);

    assert(projected.has_value());
    assert(*projected == PixelFormat::UYVY);
}
} // namespace

int main() {
    sampling_known_tokens_are_explicit_enums();
    colorimetry_known_tokens_are_explicit_enums();
    transfer_characteristic_system_known_tokens_are_explicit_enums();
    range_known_tokens_are_explicit_enums();
    unknown_future_tokens_are_preserved_as_other();
    runtime_projection_remains_localized_for_known_but_unsupported_sampling();
    existing_runtime_projection_for_ycbcr_422_8bit_still_works();

    return 0;
}
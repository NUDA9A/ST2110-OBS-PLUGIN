#include "st2110/video_signaling.hpp"

#include <cassert>
#include <optional>
#include <string>

using namespace st2110;

static ReferenceClock make_valid_reference_clock() {
    ReferenceClock clock{};
    clock.kind = ReferenceClockKind::Ptp;
    clock.ptp = PtpReferenceClock{};
    return clock;
}

static VideoStreamSignaling make_valid_signaling() {
    VideoStreamSignaling signaling{};

    signaling.media.sampling = VideoSampling{VideoSampling::Known::YCbCr422, std::nullopt};
    signaling.media.depth = VideoBitDepth{10, false};
    signaling.media.colorimetry = VideoColorimetry{VideoColorimetry::Known::Bt709, std::nullopt};
    signaling.media.transfer_characteristic_system =
        VideoTransferCharacteristicSystem{VideoTransferCharacteristicSystem::Known::SDR, std::nullopt};
    signaling.media.signal_standard = VideoSignalStandard{VideoSignalStandard::Known::St2110_20_2022, std::nullopt};
    signaling.media.range = VideoRange{VideoRange::Known::Narrow, std::nullopt};

    signaling.media.width = 1920;
    signaling.media.height = 1080;
    signaling.media.fps_num = 30000;
    signaling.media.fps_den = 1001;

    signaling.reference_clock = make_valid_reference_clock();

    return signaling;
}

static void test_known_sampling_is_valid_without_raw_token() {
    const VideoSampling sampling{VideoSampling::Known::YCbCr444, std::nullopt};
    assert(validate_video_sampling(sampling) == Error::Ok);
}

static void test_other_sampling_requires_non_empty_raw_token() {
    {
        const VideoSampling sampling{VideoSampling::Known::Other, std::nullopt};
        assert(validate_video_sampling(sampling) == Error::InvalidValue);
    }
    {
        const VideoSampling sampling{VideoSampling::Known::Other, std::string{}};
        assert(validate_video_sampling(sampling) == Error::InvalidValue);
    }
    {
        const VideoSampling sampling{VideoSampling::Known::Other, std::string{"CUSTOM-SAMPLING"}};
        assert(validate_video_sampling(sampling) == Error::Ok);
    }
}

static void test_known_sampling_rejects_raw_token() {
    const VideoSampling sampling{VideoSampling::Known::YCbCr420, std::string{"should-not-be-present"}};
    assert(validate_video_sampling(sampling) == Error::InvalidValue);
}

static void test_bit_depth_accepts_supported_structural_values() {
    assert(validate_video_bit_depth(VideoBitDepth{8, false}) == Error::Ok);
    assert(validate_video_bit_depth(VideoBitDepth{10, false}) == Error::Ok);
    assert(validate_video_bit_depth(VideoBitDepth{12, false}) == Error::Ok);
    assert(validate_video_bit_depth(VideoBitDepth{16, false}) == Error::Ok);
    assert(validate_video_bit_depth(VideoBitDepth{16, true}) == Error::Ok);
}

static void test_bit_depth_rejects_invalid_values() {
    assert(validate_video_bit_depth(VideoBitDepth{9, false}) == Error::InvalidValue);
    assert(validate_video_bit_depth(VideoBitDepth{8, true}) == Error::InvalidValue);
    assert(validate_video_bit_depth(VideoBitDepth{12, true}) == Error::InvalidValue);
}

static void test_token_backed_video_media_fields_validate_known_and_other_cases() {
    {
        const VideoColorimetry colorimetry{VideoColorimetry::Known::Bt2020, std::nullopt};
        assert(validate_video_colorimetry(colorimetry) == Error::Ok);
    }
    {
        const VideoColorimetry colorimetry{VideoColorimetry::Known::Other, std::string{"CUSTOM-COLOR"}};
        assert(validate_video_colorimetry(colorimetry) == Error::Ok);
    }
    {
        const VideoColorimetry colorimetry{VideoColorimetry::Known::Other, std::nullopt};
        assert(validate_video_colorimetry(colorimetry) == Error::InvalidValue);
    }

    {
        const VideoTransferCharacteristicSystem tcs{VideoTransferCharacteristicSystem::Known::PQ, std::nullopt};
        assert(validate_video_transfer_characteristic_system(tcs) == Error::Ok);
    }
    {
        const VideoTransferCharacteristicSystem tcs{VideoTransferCharacteristicSystem::Known::Other,
                                                    std::string{"CUSTOM-TCS"}};
        assert(validate_video_transfer_characteristic_system(tcs) == Error::Ok);
    }
    {
        const VideoTransferCharacteristicSystem tcs{VideoTransferCharacteristicSystem::Known::Other, std::nullopt};
        assert(validate_video_transfer_characteristic_system(tcs) == Error::InvalidValue);
    }

    {
        const VideoSignalStandard ssn{VideoSignalStandard::Known::St2110_20_2022, std::nullopt};
        assert(validate_video_signal_standard(ssn) == Error::Ok);
    }
    {
        const VideoSignalStandard ssn{VideoSignalStandard::Known::Other, std::string{"ST2110-20:2099"}};
        assert(validate_video_signal_standard(ssn) == Error::Ok);
    }
    {
        const VideoSignalStandard ssn{VideoSignalStandard::Known::Other, std::nullopt};
        assert(validate_video_signal_standard(ssn) == Error::InvalidValue);
    }

    {
        const VideoRange range{VideoRange::Known::Full, std::nullopt};
        assert(validate_video_range(range) == Error::Ok);
    }
    {
        const VideoRange range{VideoRange::Known::Other, std::string{"CUSTOM-RANGE"}};
        assert(validate_video_range(range) == Error::Ok);
    }
    {
        const VideoRange range{VideoRange::Known::Other, std::nullopt};
        assert(validate_video_range(range) == Error::InvalidValue);
    }
}

static void test_video_stream_signaling_accepts_extended_media_properties() {
    VideoStreamSignaling signaling = make_valid_signaling();
    assert(validate_video_stream_signaling(signaling) == Error::Ok);
}

static void test_video_stream_signaling_accepts_absent_optional_media_fields() {
    VideoStreamSignaling signaling = make_valid_signaling();
    signaling.media.transfer_characteristic_system = std::nullopt;
    signaling.media.signal_standard = std::nullopt;
    signaling.media.range = std::nullopt;

    assert(validate_video_stream_signaling(signaling) == Error::Ok);
}

static void test_video_stream_signaling_rejects_invalid_sampling() {
    VideoStreamSignaling signaling = make_valid_signaling();
    signaling.media.sampling = VideoSampling{VideoSampling::Known::Other, std::nullopt};

    assert(validate_video_stream_signaling(signaling) == Error::InvalidValue);
}

static void test_video_stream_signaling_rejects_invalid_bit_depth() {
    VideoStreamSignaling signaling = make_valid_signaling();
    signaling.media.depth = VideoBitDepth{9, false};

    assert(validate_video_stream_signaling(signaling) == Error::InvalidValue);
}

static void test_video_stream_signaling_rejects_invalid_optional_media_field() {
    VideoStreamSignaling signaling = make_valid_signaling();
    signaling.media.transfer_characteristic_system =
        VideoTransferCharacteristicSystem{VideoTransferCharacteristicSystem::Known::Other, std::nullopt};

    assert(validate_video_stream_signaling(signaling) == Error::InvalidValue);
}

static void test_pixel_format_projection_accepts_ycbcr422_8bit() {
    VideoStreamSignaling signaling = make_valid_signaling();
    signaling.media.depth = VideoBitDepth{8, false};

    const auto projected = pixel_format_from_video_stream_signaling(signaling);
    assert(projected.has_value());
    assert(*projected == PixelFormat::UYVY);
}

static void test_pixel_format_projection_rejects_structurally_valid_but_unsupported_media() {
    {
        VideoStreamSignaling signaling = make_valid_signaling();
        signaling.media.depth = VideoBitDepth{10, false};

        const auto projected = pixel_format_from_video_stream_signaling(signaling);
        assert(!projected.has_value());
        assert(projected.error() == Error::Unsupported);
    }

    {
        VideoStreamSignaling signaling = make_valid_signaling();
        signaling.media.sampling = VideoSampling{VideoSampling::Known::RGB, std::nullopt};
        signaling.media.depth = VideoBitDepth{8, false};

        const auto projected = pixel_format_from_video_stream_signaling(signaling);
        assert(!projected.has_value());
        assert(projected.error() == Error::Unsupported);
    }
}

int main() {
    test_known_sampling_is_valid_without_raw_token();
    test_other_sampling_requires_non_empty_raw_token();
    test_known_sampling_rejects_raw_token();
    test_bit_depth_accepts_supported_structural_values();
    test_bit_depth_rejects_invalid_values();
    test_token_backed_video_media_fields_validate_known_and_other_cases();
    test_video_stream_signaling_accepts_extended_media_properties();
    test_video_stream_signaling_accepts_absent_optional_media_fields();
    test_video_stream_signaling_rejects_invalid_sampling();
    test_video_stream_signaling_rejects_invalid_bit_depth();
    test_video_stream_signaling_rejects_invalid_optional_media_field();
    test_pixel_format_projection_accepts_ycbcr422_8bit();
    test_pixel_format_projection_rejects_structurally_valid_but_unsupported_media();
    return 0;
}
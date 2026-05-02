#include "st2110/video_signaling.hpp"

#include <cassert>
#include <cstdint>
#include <optional>
#include <string>

using namespace st2110;

static ReferenceClock make_valid_reference_clock() {
    ReferenceClock clock{};
    clock.kind = ReferenceClockKind::Ptp;

    PtpReferenceClock ptp{};
    ptp.clock_identity = {0x39, 0xA7, 0x94, 0xFF, 0xFE, 0x07, 0xCB, 0xD0};
    ptp.domain_number = 127;
    ptp.traceable = false;

    clock.ptp = ptp;
    return clock;
}

static VideoSignalStandard make_signal_standard(VideoSignalStandard::Known known) {
    return VideoSignalStandard{known, std::nullopt};
}

static VideoTransferCharacteristicSystem make_tcs(VideoTransferCharacteristicSystem::Known known) {
    return VideoTransferCharacteristicSystem{known, std::nullopt};
}

static VideoRange make_range(VideoRange::Known known) { return VideoRange{known, std::nullopt}; }

static VideoPixelAspectRatio make_par(uint32_t width, uint32_t height) {
    return VideoPixelAspectRatio{.width = width, .height = height};
}

static VideoStreamSignaling make_valid_signaling() {
    VideoStreamSignaling signaling{};

    signaling.media.sampling = VideoSampling{VideoSampling::Known::YCbCr422, std::nullopt};
    signaling.media.depth = VideoBitDepth{10, false};
    signaling.media.colorimetry = VideoColorimetry{VideoColorimetry::Known::Bt709, std::nullopt};
    signaling.media.transfer_characteristic_system =
        VideoTransferCharacteristicSystem{VideoTransferCharacteristicSystem::Known::SDR, std::nullopt};
    signaling.media.signal_standard = VideoSignalStandard{VideoSignalStandard::Known::St2110_20_2017, std::nullopt};
    signaling.media.range = VideoRange{VideoRange::Known::Narrow, std::nullopt};
    signaling.media.pixel_aspect_ratio = make_par(1, 1);

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
        const VideoSignalStandard ssn{VideoSignalStandard::Known::St2110_20_2017, std::nullopt};
        assert(validate_video_signal_standard(ssn) == Error::Ok);
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
        const VideoRange range{VideoRange::Known::Narrow, std::nullopt};
        assert(validate_video_range(range) == Error::Ok);
    }
    {
        const VideoRange range{VideoRange::Known::FullProtect, std::nullopt};
        assert(validate_video_range(range) == Error::Ok);
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

static void test_pixel_aspect_ratio_accepts_valid_values() {
    assert(validate_video_pixel_aspect_ratio(make_par(1, 1)) == Error::Ok);
    assert(validate_video_pixel_aspect_ratio(make_par(12, 11)) == Error::Ok);
}

static void test_pixel_aspect_ratio_rejects_invalid_values() {
    assert(validate_video_pixel_aspect_ratio(make_par(0, 1)) == Error::InvalidValue);
    assert(validate_video_pixel_aspect_ratio(make_par(1, 0)) == Error::InvalidValue);
    assert(validate_video_pixel_aspect_ratio(make_par(0, 0)) == Error::InvalidValue);
}

static void test_signaled_dimension_limits_accept_min_and_max_values() {
    assert(validate_video_media_description_dimensions(1, 1) == Error::Ok);
    assert(validate_video_media_description_dimensions(32767, 32767) == Error::Ok);
}

static void test_signaled_dimension_limits_reject_zero_and_overflow_values() {
    assert(validate_video_media_description_dimensions(0, 1) == Error::InvalidValue);
    assert(validate_video_media_description_dimensions(1, 0) == Error::InvalidValue);
    assert(validate_video_media_description_dimensions(32768, 1) == Error::InvalidValue);
    assert(validate_video_media_description_dimensions(1, 32768) == Error::InvalidValue);
}

static void test_video_stream_signaling_accepts_extended_media_properties() {
    VideoStreamSignaling signaling = make_valid_signaling();
    assert(validate_video_stream_signaling(signaling) == Error::Ok);
}

static void test_video_stream_signaling_accepts_absent_optional_tcs_and_range_fields() {
    VideoStreamSignaling signaling = make_valid_signaling();
    signaling.media.transfer_characteristic_system = std::nullopt;
    signaling.media.range = std::nullopt;

    assert(validate_video_stream_signaling(signaling) == Error::Ok);
}

static void test_video_stream_signaling_rejects_missing_signal_standard() {
    VideoStreamSignaling signaling = make_valid_signaling();
    signaling.media.signal_standard = std::nullopt;

    assert(validate_video_stream_signaling(signaling) == Error::InvalidValue);
}

static void test_video_stream_signaling_accepts_non_square_pixel_aspect_ratio() {
    VideoStreamSignaling signaling = make_valid_signaling();
    signaling.media.pixel_aspect_ratio = make_par(12, 11);

    assert(validate_video_stream_signaling(signaling) == Error::Ok);
}

static void test_video_stream_signaling_rejects_invalid_pixel_aspect_ratio() {
    VideoStreamSignaling signaling = make_valid_signaling();
    signaling.media.pixel_aspect_ratio = make_par(0, 1);

    assert(validate_video_stream_signaling(signaling) == Error::InvalidValue);
}

static void test_video_stream_signaling_accepts_signaled_dimension_limits() {
    {
        VideoStreamSignaling signaling = make_valid_signaling();
        signaling.media.width = 1;
        signaling.media.height = 1;

        assert(validate_video_stream_signaling(signaling) == Error::Ok);
    }

    {
        VideoStreamSignaling signaling = make_valid_signaling();
        signaling.media.width = 32767;
        signaling.media.height = 32767;

        assert(validate_video_stream_signaling(signaling) == Error::Ok);
    }
}

static void test_video_stream_signaling_rejects_out_of_range_signaled_dimensions() {
    {
        VideoStreamSignaling signaling = make_valid_signaling();
        signaling.media.width = 0;
        signaling.media.height = 1080;

        assert(validate_video_stream_signaling(signaling) == Error::InvalidValue);
    }

    {
        VideoStreamSignaling signaling = make_valid_signaling();
        signaling.media.width = 1920;
        signaling.media.height = 0;

        assert(validate_video_stream_signaling(signaling) == Error::InvalidValue);
    }

    {
        VideoStreamSignaling signaling = make_valid_signaling();
        signaling.media.width = 32768;
        signaling.media.height = 1080;

        assert(validate_video_stream_signaling(signaling) == Error::InvalidValue);
    }

    {
        VideoStreamSignaling signaling = make_valid_signaling();
        signaling.media.width = 1920;
        signaling.media.height = 32768;

        assert(validate_video_stream_signaling(signaling) == Error::InvalidValue);
    }
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

static void test_video_stream_signaling_rejects_bt709_sdr_with_st2110_20_2022() {
    VideoStreamSignaling signaling = make_valid_signaling();
    signaling.media.signal_standard = make_signal_standard(VideoSignalStandard::Known::St2110_20_2022);

    assert(validate_video_stream_signaling(signaling) == Error::InvalidValue);
}

static void test_video_stream_signaling_accepts_alpha_with_st2110_20_2022() {
    VideoStreamSignaling signaling = make_valid_signaling();
    signaling.media.sampling = VideoSampling{VideoSampling::Known::Key, std::nullopt};
    signaling.media.colorimetry = VideoColorimetry{VideoColorimetry::Known::Alpha, std::nullopt};
    signaling.media.transfer_characteristic_system = std::nullopt;
    signaling.media.signal_standard = make_signal_standard(VideoSignalStandard::Known::St2110_20_2022);

    assert(validate_video_stream_signaling(signaling) == Error::Ok);
}

static void test_video_stream_signaling_rejects_alpha_with_st2110_20_2017() {
    VideoStreamSignaling signaling = make_valid_signaling();
    signaling.media.sampling = VideoSampling{VideoSampling::Known::Key, std::nullopt};
    signaling.media.colorimetry = VideoColorimetry{VideoColorimetry::Known::Alpha, std::nullopt};
    signaling.media.transfer_characteristic_system = std::nullopt;
    signaling.media.signal_standard = make_signal_standard(VideoSignalStandard::Known::St2110_20_2017);

    assert(validate_video_stream_signaling(signaling) == Error::InvalidValue);
}

static void test_video_stream_signaling_accepts_st2115logs3_with_st2110_20_2022() {
    VideoStreamSignaling signaling = make_valid_signaling();
    signaling.media.transfer_characteristic_system = make_tcs(VideoTransferCharacteristicSystem::Known::St2115LogS3);
    signaling.media.signal_standard = make_signal_standard(VideoSignalStandard::Known::St2110_20_2022);

    assert(validate_video_stream_signaling(signaling) == Error::Ok);
}

static void test_video_stream_signaling_rejects_st2115logs3_with_st2110_20_2017() {
    VideoStreamSignaling signaling = make_valid_signaling();
    signaling.media.transfer_characteristic_system = make_tcs(VideoTransferCharacteristicSystem::Known::St2115LogS3);
    signaling.media.signal_standard = make_signal_standard(VideoSignalStandard::Known::St2110_20_2017);

    assert(validate_video_stream_signaling(signaling) == Error::InvalidValue);
}

static void test_video_stream_signaling_accepts_bt2100_with_full_range() {
    VideoStreamSignaling signaling = make_valid_signaling();
    signaling.media.colorimetry = VideoColorimetry{VideoColorimetry::Known::Bt2100, std::nullopt};
    signaling.media.range = make_range(VideoRange::Known::Full);

    assert(validate_video_stream_signaling(signaling) == Error::Ok);
}

static void test_video_stream_signaling_rejects_bt2100_with_fullprotect_range() {
    VideoStreamSignaling signaling = make_valid_signaling();
    signaling.media.colorimetry = VideoColorimetry{VideoColorimetry::Known::Bt2100, std::nullopt};
    signaling.media.range = make_range(VideoRange::Known::FullProtect);

    assert(validate_video_stream_signaling(signaling) == Error::InvalidValue);
}

static void test_video_stream_signaling_accepts_non_bt2100_fullprotect_range() {
    VideoStreamSignaling signaling = make_valid_signaling();
    signaling.media.colorimetry = VideoColorimetry{VideoColorimetry::Known::Bt709, std::nullopt};
    signaling.media.range = make_range(VideoRange::Known::FullProtect);

    assert(validate_video_stream_signaling(signaling) == Error::Ok);
}

static void test_pixel_format_projection_accepts_ycbcr422_8bit() {
    VideoStreamSignaling signaling = make_valid_signaling();
    signaling.media.depth = VideoBitDepth{8, false};

    const auto projected = pixel_format_from_video_stream_signaling(signaling);
    assert(projected.has_value());
    assert(*projected == PixelFormat::UYVY);
}

static void test_pixel_format_projection_remains_independent_from_pixel_aspect_ratio() {
    VideoStreamSignaling signaling = make_valid_signaling();
    signaling.media.depth = VideoBitDepth{8, false};
    signaling.media.pixel_aspect_ratio = make_par(12, 11);

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

    {
        VideoStreamSignaling signaling = make_valid_signaling();
        signaling.media.sampling = VideoSampling{VideoSampling::Known::Key, std::nullopt};
        signaling.media.colorimetry = VideoColorimetry{VideoColorimetry::Known::Alpha, std::nullopt};
        signaling.media.transfer_characteristic_system = std::nullopt;
        signaling.media.signal_standard = make_signal_standard(VideoSignalStandard::Known::St2110_20_2022);
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
    test_pixel_aspect_ratio_accepts_valid_values();
    test_pixel_aspect_ratio_rejects_invalid_values();
    test_signaled_dimension_limits_accept_min_and_max_values();
    test_signaled_dimension_limits_reject_zero_and_overflow_values();
    test_video_stream_signaling_accepts_extended_media_properties();
    test_video_stream_signaling_accepts_absent_optional_tcs_and_range_fields();
    test_video_stream_signaling_rejects_missing_signal_standard();
    test_video_stream_signaling_accepts_non_square_pixel_aspect_ratio();
    test_video_stream_signaling_rejects_invalid_pixel_aspect_ratio();
    test_video_stream_signaling_accepts_signaled_dimension_limits();
    test_video_stream_signaling_rejects_out_of_range_signaled_dimensions();
    test_video_stream_signaling_rejects_invalid_sampling();
    test_video_stream_signaling_rejects_invalid_bit_depth();
    test_video_stream_signaling_rejects_invalid_optional_media_field();
    test_video_stream_signaling_rejects_bt709_sdr_with_st2110_20_2022();
    test_video_stream_signaling_accepts_alpha_with_st2110_20_2022();
    test_video_stream_signaling_rejects_alpha_with_st2110_20_2017();
    test_video_stream_signaling_accepts_st2115logs3_with_st2110_20_2022();
    test_video_stream_signaling_rejects_st2115logs3_with_st2110_20_2017();
    test_video_stream_signaling_accepts_bt2100_with_full_range();
    test_video_stream_signaling_rejects_bt2100_with_fullprotect_range();
    test_video_stream_signaling_accepts_non_bt2100_fullprotect_range();
    test_pixel_format_projection_accepts_ycbcr422_8bit();
    test_pixel_format_projection_remains_independent_from_pixel_aspect_ratio();
    test_pixel_format_projection_rejects_structurally_valid_but_unsupported_media();
    return 0;
}
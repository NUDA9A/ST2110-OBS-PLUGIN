#include <array>
#include <cassert>
#include <optional>
#include <string>
#include <string_view>

#include <st2110/video_sdp_ingestion.hpp>
#include <st2110/video_signaling.hpp>

static st2110::VideoSignalStandard make_signal_standard(st2110::VideoSignalStandard::Known known) {
    return st2110::VideoSignalStandard{known, std::nullopt};
}

static st2110::VideoTransferCharacteristicSystem make_tcs(st2110::VideoTransferCharacteristicSystem::Known known) {
    return st2110::VideoTransferCharacteristicSystem{known, std::nullopt};
}

static st2110::VideoRange make_range(st2110::VideoRange::Known known) {
    return st2110::VideoRange{known, std::nullopt};
}

static st2110::VideoStreamSignaling make_valid_video_signaling() {
    st2110::VideoStreamSignaling signaling{};

    signaling.media.sampling.known = st2110::VideoSampling::Known::YCbCr422;
    signaling.media.width = 1920;
    signaling.media.height = 1080;
    signaling.media.fps_num = 25;
    signaling.media.fps_den = 1;
    signaling.media.depth.bits = 8;
    signaling.media.depth.floating_point = false;
    signaling.media.colorimetry.known = st2110::VideoColorimetry::Known::Bt709;

    st2110::VideoSignalStandard ssn{};
    ssn.known = st2110::VideoSignalStandard::Known::St2110_20_2017;
    signaling.media.signal_standard = ssn;

    signaling.scan_mode = st2110::VideoScanMode::Progressive;
    signaling.packing_mode = st2110::VideoPackingMode::Gpm;

    signaling.media_clock_mode = st2110::MediaClockMode::Direct;
    signaling.timestamp_mode = st2110::TimestampMode::New;
    signaling.sender_type = st2110::VideoSenderType::Narrow;

    signaling.reference_clock.kind = st2110::ReferenceClockKind::Other;
    signaling.reference_clock.raw_token = std::string{"test-clock"};

    return signaling;
}

static void assert_runtime_projection_is_unsupported(const st2110::VideoStreamSignaling &signaling) {
    auto projected = st2110::pixel_format_from_video_stream_signaling(signaling);

    assert(!projected.has_value());
    assert(projected.error() == st2110::Error::Unsupported);
}

static void test_progressive_420_sampling_is_structurally_valid_but_runtime_unsupported() {
    const std::array<st2110::VideoSampling::Known, 3> variants{st2110::VideoSampling::Known::YCbCr420,
                                                               st2110::VideoSampling::Known::CLYCbCr420,
                                                               st2110::VideoSampling::Known::ICtCp420};

    for (const auto sampling : variants) {
        auto signaling = make_valid_video_signaling();
        signaling.media.sampling.known = sampling;
        signaling.scan_mode = st2110::VideoScanMode::Progressive;

        assert(st2110::validate_video_stream_signaling(signaling) == st2110::Error::Ok);
        assert_runtime_projection_is_unsupported(signaling);
    }
}

static void test_interlaced_and_psf_420_sampling_are_structurally_rejected() {
    const std::array<st2110::VideoSampling::Known, 3> variants{st2110::VideoSampling::Known::YCbCr420,
                                                               st2110::VideoSampling::Known::CLYCbCr420,
                                                               st2110::VideoSampling::Known::ICtCp420};

    for (const auto sampling : variants) {
        {
            auto signaling = make_valid_video_signaling();
            signaling.media.sampling.known = sampling;
            signaling.scan_mode = st2110::VideoScanMode::Interlaced;

            assert(st2110::validate_video_stream_signaling(signaling) == st2110::Error::InvalidValue);
        }

        {
            auto signaling = make_valid_video_signaling();
            signaling.media.sampling.known = sampling;
            signaling.scan_mode = st2110::VideoScanMode::PsF;

            assert(st2110::validate_video_stream_signaling(signaling) == st2110::Error::InvalidValue);
        }
    }
}

static void test_key_sampling_requires_alpha_colorimetry_and_no_tcs() {
    {
        auto signaling = make_valid_video_signaling();
        signaling.media.sampling.known = st2110::VideoSampling::Known::Key;
        signaling.media.colorimetry.known = st2110::VideoColorimetry::Known::Alpha;
        signaling.media.transfer_characteristic_system = std::nullopt;
        signaling.media.signal_standard = make_signal_standard(st2110::VideoSignalStandard::Known::St2110_20_2022);

        assert(st2110::validate_video_stream_signaling(signaling) == st2110::Error::Ok);
        assert_runtime_projection_is_unsupported(signaling);
    }

    {
        auto signaling = make_valid_video_signaling();
        signaling.media.sampling.known = st2110::VideoSampling::Known::Key;
        signaling.media.colorimetry.known = st2110::VideoColorimetry::Known::Bt709;
        signaling.media.transfer_characteristic_system = std::nullopt;

        assert(st2110::validate_video_stream_signaling(signaling) == st2110::Error::InvalidValue);
    }

    {
        auto signaling = make_valid_video_signaling();
        signaling.media.sampling.known = st2110::VideoSampling::Known::Key;
        signaling.media.colorimetry.known = st2110::VideoColorimetry::Known::Alpha;

        st2110::VideoTransferCharacteristicSystem tcs{};
        tcs.known = st2110::VideoTransferCharacteristicSystem::Known::SDR;
        signaling.media.transfer_characteristic_system = tcs;
        signaling.media.signal_standard = make_signal_standard(st2110::VideoSignalStandard::Known::St2110_20_2022);

        assert(st2110::validate_video_stream_signaling(signaling) == st2110::Error::InvalidValue);
    }
}

static void test_bt709_sdr_ssn_cross_field_validation_accepts_2017_and_2022() {
    {
        auto signaling = make_valid_video_signaling();
        signaling.media.transfer_characteristic_system =
            make_tcs(st2110::VideoTransferCharacteristicSystem::Known::SDR);
        signaling.media.signal_standard = make_signal_standard(st2110::VideoSignalStandard::Known::St2110_20_2017);

        assert(st2110::validate_video_stream_signaling(signaling) == st2110::Error::Ok);
    }

    {
        auto signaling = make_valid_video_signaling();
        signaling.media.transfer_characteristic_system =
            make_tcs(st2110::VideoTransferCharacteristicSystem::Known::SDR);
        signaling.media.signal_standard = make_signal_standard(st2110::VideoSignalStandard::Known::St2110_20_2022);

        assert(st2110::validate_video_stream_signaling(signaling) == st2110::Error::Ok);
    }
}

static void test_alpha_requires_st2110_20_2022() {
    {
        auto signaling = make_valid_video_signaling();
        signaling.media.sampling.known = st2110::VideoSampling::Known::Key;
        signaling.media.colorimetry.known = st2110::VideoColorimetry::Known::Alpha;
        signaling.media.transfer_characteristic_system = std::nullopt;
        signaling.media.signal_standard = make_signal_standard(st2110::VideoSignalStandard::Known::St2110_20_2017);

        assert(st2110::validate_video_stream_signaling(signaling) == st2110::Error::InvalidValue);
    }

    {
        auto signaling = make_valid_video_signaling();
        signaling.media.sampling.known = st2110::VideoSampling::Known::Key;
        signaling.media.colorimetry.known = st2110::VideoColorimetry::Known::Alpha;
        signaling.media.transfer_characteristic_system = std::nullopt;
        signaling.media.signal_standard = make_signal_standard(st2110::VideoSignalStandard::Known::St2110_20_2022);

        assert(st2110::validate_video_stream_signaling(signaling) == st2110::Error::Ok);
        assert_runtime_projection_is_unsupported(signaling);
    }
}

static void test_st2115logs3_requires_st2110_20_2022() {
    {
        auto signaling = make_valid_video_signaling();
        signaling.media.transfer_characteristic_system =
            make_tcs(st2110::VideoTransferCharacteristicSystem::Known::St2115LogS3);
        signaling.media.signal_standard = make_signal_standard(st2110::VideoSignalStandard::Known::St2110_20_2017);

        assert(st2110::validate_video_stream_signaling(signaling) == st2110::Error::InvalidValue);
    }

    {
        auto signaling = make_valid_video_signaling();
        signaling.media.transfer_characteristic_system =
            make_tcs(st2110::VideoTransferCharacteristicSystem::Known::St2115LogS3);
        signaling.media.signal_standard = make_signal_standard(st2110::VideoSignalStandard::Known::St2110_20_2022);

        assert(st2110::validate_video_stream_signaling(signaling) == st2110::Error::Ok);
    }
}

static void test_range_cross_field_validation() {
    {
        auto signaling = make_valid_video_signaling();
        signaling.media.colorimetry.known = st2110::VideoColorimetry::Known::Bt2100;
        signaling.media.range = make_range(st2110::VideoRange::Known::Full);

        assert(st2110::validate_video_stream_signaling(signaling) == st2110::Error::Ok);
    }

    {
        auto signaling = make_valid_video_signaling();
        signaling.media.colorimetry.known = st2110::VideoColorimetry::Known::Bt2100;
        signaling.media.range = make_range(st2110::VideoRange::Known::FullProtect);

        assert(st2110::validate_video_stream_signaling(signaling) == st2110::Error::InvalidValue);
    }

    {
        auto signaling = make_valid_video_signaling();
        signaling.media.colorimetry.known = st2110::VideoColorimetry::Known::Bt709;
        signaling.media.range = make_range(st2110::VideoRange::Known::FullProtect);

        assert(st2110::validate_video_stream_signaling(signaling) == st2110::Error::Ok);
    }

    {
        auto signaling = make_valid_video_signaling();
        signaling.media.colorimetry.known = st2110::VideoColorimetry::Known::Bt2100;
        signaling.media.range = std::nullopt;

        assert(st2110::validate_video_stream_signaling(signaling) == st2110::Error::Ok);
    }

    {
        auto signaling = make_valid_video_signaling();
        signaling.media.colorimetry =
            st2110::VideoColorimetry{st2110::VideoColorimetry::Known::Other, std::string{"FUTURE-COLOR"}};
        signaling.media.range = st2110::VideoRange{st2110::VideoRange::Known::Other, std::string{"FUTURE-RANGE"}};

        assert(st2110::validate_video_stream_signaling(signaling) == st2110::Error::Ok);
    }
}

static std::string make_sdp_with_fmtp(std::string_view fmtp_payload) {
    return std::string{"v=0\n"
                       "o=- 1 1 IN IP4 192.0.2.1\n"
                       "s=ST2110 test\n"
                       "m=video 50000 RTP/AVP 112\n"
                       "a=ts-refclk:ptp=IEEE1588-2008:traceable\n"
                       "a=mediaclk:direct=0\n"
                       "a=rtpmap:112 raw/90000\n"
                       "a=fmtp:112 "} +
           std::string{fmtp_payload} + "; TP=2110TPN\n";
}

static void test_sdp_ingestion_applies_420_cross_field_validation() {
    const std::string progressive_420_sdp =
        make_sdp_with_fmtp("sampling=YCbCr-4:2:0; width=1920; height=1080; exactframerate=25; "
                           "depth=8; colorimetry=BT709; PM=2110GPM; SSN=ST2110-20:2017");

    auto progressive = st2110::parse_video_stream_signaling_from_sdp(progressive_420_sdp, 112);

    assert(progressive.has_value());
    assert(progressive->media.sampling.known == st2110::VideoSampling::Known::YCbCr420);
    assert(progressive->scan_mode == st2110::VideoScanMode::Progressive);
    assert(progressive->sender_type == st2110::VideoSenderType::Narrow);
    assert_runtime_projection_is_unsupported(*progressive);

    const std::string interlaced_420_sdp =
        make_sdp_with_fmtp("sampling=YCbCr-4:2:0; width=1920; height=1080; exactframerate=25; "
                           "depth=8; colorimetry=BT709; PM=2110GPM; SSN=ST2110-20:2017; interlace");

    auto interlaced = st2110::parse_video_stream_signaling_from_sdp(interlaced_420_sdp, 112);

    assert(!interlaced.has_value());
    assert(interlaced.error() == st2110::Error::InvalidValue);
}

static void test_sdp_ingestion_applies_key_cross_field_validation() {
    const std::string valid_key_sdp =
        make_sdp_with_fmtp("sampling=KEY; width=1920; height=1080; exactframerate=25; "
                           "depth=8; colorimetry=ALPHA; PM=2110GPM; SSN=ST2110-20:2022");

    auto valid_key = st2110::parse_video_stream_signaling_from_sdp(valid_key_sdp, 112);

    assert(valid_key.has_value());
    assert(valid_key->media.sampling.known == st2110::VideoSampling::Known::Key);
    assert(valid_key->media.colorimetry.known == st2110::VideoColorimetry::Known::Alpha);
    assert(!valid_key->media.transfer_characteristic_system.has_value());
    assert(valid_key->sender_type == st2110::VideoSenderType::Narrow);
    assert_runtime_projection_is_unsupported(*valid_key);

    const std::string non_alpha_key_sdp =
        make_sdp_with_fmtp("sampling=KEY; width=1920; height=1080; exactframerate=25; "
                           "depth=8; colorimetry=BT709; PM=2110GPM; SSN=ST2110-20:2022");

    auto non_alpha_key = st2110::parse_video_stream_signaling_from_sdp(non_alpha_key_sdp, 112);

    assert(!non_alpha_key.has_value());
    assert(non_alpha_key.error() == st2110::Error::InvalidValue);

    const std::string key_with_tcs_sdp =
        make_sdp_with_fmtp("sampling=KEY; width=1920; height=1080; exactframerate=25; "
                           "depth=8; colorimetry=ALPHA; TCS=SDR; PM=2110GPM; SSN=ST2110-20:2022");

    auto key_with_tcs = st2110::parse_video_stream_signaling_from_sdp(key_with_tcs_sdp, 112);

    assert(!key_with_tcs.has_value());
    assert(key_with_tcs.error() == st2110::Error::InvalidValue);
}

static void test_sdp_ingestion_applies_ssn_cross_field_validation() {
    {
        const std::string sdp = make_sdp_with_fmtp("sampling=YCbCr-4:2:2; width=1920; height=1080; exactframerate=25; "
                                                   "depth=8; colorimetry=BT709; TCS=SDR; PM=2110GPM; "
                                                   "SSN=ST2110-20:2017");

        auto signaling = st2110::parse_video_stream_signaling_from_sdp(sdp, 112);
        assert(signaling.has_value());
        assert(signaling->media.signal_standard.has_value());
        assert(signaling->media.signal_standard->known == st2110::VideoSignalStandard::Known::St2110_20_2017);
        assert(signaling->sender_type == st2110::VideoSenderType::Narrow);
    }

    {
        const std::string sdp = make_sdp_with_fmtp("sampling=YCbCr-4:2:2; width=1920; height=1080; exactframerate=25; "
                                                   "depth=8; colorimetry=BT709; TCS=SDR; PM=2110GPM; "
                                                   "SSN=ST2110-20:2022");

        auto signaling = st2110::parse_video_stream_signaling_from_sdp(sdp, 112);
        assert(signaling.has_value());
        assert(signaling->media.signal_standard.has_value());
        assert(signaling->media.signal_standard->known == st2110::VideoSignalStandard::Known::St2110_20_2022);
        assert(signaling->sender_type == st2110::VideoSenderType::Narrow);
    }

    {
        const std::string sdp = make_sdp_with_fmtp("sampling=KEY; width=1920; height=1080; exactframerate=25; "
                                                   "depth=8; colorimetry=ALPHA; PM=2110GPM; "
                                                   "SSN=ST2110-20:2017");

        auto signaling = st2110::parse_video_stream_signaling_from_sdp(sdp, 112);
        assert(!signaling.has_value());
        assert(signaling.error() == st2110::Error::InvalidValue);
    }

    {
        const std::string sdp = make_sdp_with_fmtp("sampling=KEY; width=1920; height=1080; exactframerate=25; "
                                                   "depth=8; colorimetry=ALPHA; PM=2110GPM; "
                                                   "SSN=ST2110-20:2022");

        auto signaling = st2110::parse_video_stream_signaling_from_sdp(sdp, 112);
        assert(signaling.has_value());
        assert(signaling->sender_type == st2110::VideoSenderType::Narrow);
        assert_runtime_projection_is_unsupported(*signaling);
    }

    {
        const std::string sdp = make_sdp_with_fmtp("sampling=YCbCr-4:2:2; width=1920; height=1080; exactframerate=25; "
                                                   "depth=8; colorimetry=BT709; TCS=ST2115LOGS3; PM=2110GPM; "
                                                   "SSN=ST2110-20:2017");

        auto signaling = st2110::parse_video_stream_signaling_from_sdp(sdp, 112);
        assert(!signaling.has_value());
        assert(signaling.error() == st2110::Error::InvalidValue);
    }

    {
        const std::string sdp = make_sdp_with_fmtp("sampling=YCbCr-4:2:2; width=1920; height=1080; exactframerate=25; "
                                                   "depth=8; colorimetry=BT709; TCS=ST2115LOGS3; PM=2110GPM; "
                                                   "SSN=ST2110-20:2022");

        auto signaling = st2110::parse_video_stream_signaling_from_sdp(sdp, 112);
        assert(signaling.has_value());
        assert(signaling->media.transfer_characteristic_system.has_value());
        assert(signaling->media.transfer_characteristic_system->known ==
               st2110::VideoTransferCharacteristicSystem::Known::St2115LogS3);
        assert(signaling->sender_type == st2110::VideoSenderType::Narrow);
    }
}

static void test_sdp_ingestion_applies_range_cross_field_validation() {
    {
        const std::string sdp = make_sdp_with_fmtp("sampling=YCbCr-4:2:2; width=1920; height=1080; exactframerate=25; "
                                                   "depth=8; colorimetry=BT2100; RANGE=FULL; PM=2110GPM; "
                                                   "SSN=ST2110-20:2017");

        auto signaling = st2110::parse_video_stream_signaling_from_sdp(sdp, 112);
        assert(signaling.has_value());
        assert(signaling->media.range.has_value());
        assert(signaling->media.range->known == st2110::VideoRange::Known::Full);
        assert(signaling->sender_type == st2110::VideoSenderType::Narrow);
    }

    {
        const std::string sdp = make_sdp_with_fmtp("sampling=YCbCr-4:2:2; width=1920; height=1080; exactframerate=25; "
                                                   "depth=8; colorimetry=BT2100; RANGE=FULLPROTECT; PM=2110GPM; "
                                                   "SSN=ST2110-20:2017");

        auto signaling = st2110::parse_video_stream_signaling_from_sdp(sdp, 112);
        assert(!signaling.has_value());
        assert(signaling.error() == st2110::Error::InvalidValue);
    }

    {
        const std::string sdp = make_sdp_with_fmtp("sampling=YCbCr-4:2:2; width=1920; height=1080; exactframerate=25; "
                                                   "depth=8; colorimetry=BT709; RANGE=FULLPROTECT; PM=2110GPM; "
                                                   "SSN=ST2110-20:2017");

        auto signaling = st2110::parse_video_stream_signaling_from_sdp(sdp, 112);
        assert(signaling.has_value());
        assert(signaling->media.range.has_value());
        assert(signaling->media.range->known == st2110::VideoRange::Known::FullProtect);
        assert(signaling->sender_type == st2110::VideoSenderType::Narrow);
    }

    {
        const std::string sdp = make_sdp_with_fmtp("sampling=YCbCr-4:2:2; width=1920; height=1080; exactframerate=25; "
                                                   "depth=8; colorimetry=BT709; RANGE=FUTURE-RANGE; PM=2110GPM; "
                                                   "SSN=ST2110-20:2017");

        auto signaling = st2110::parse_video_stream_signaling_from_sdp(sdp, 112);
        assert(signaling.has_value());
        assert(signaling->media.range.has_value());
        assert(signaling->media.range->known == st2110::VideoRange::Known::Other);
        assert(signaling->media.range->raw_token.has_value());
        assert(*signaling->media.range->raw_token == "FUTURE-RANGE");
        assert(signaling->sender_type == st2110::VideoSenderType::Narrow);
    }
}

int main() {
    test_progressive_420_sampling_is_structurally_valid_but_runtime_unsupported();
    test_interlaced_and_psf_420_sampling_are_structurally_rejected();
    test_key_sampling_requires_alpha_colorimetry_and_no_tcs();

    test_bt709_sdr_ssn_cross_field_validation_accepts_2017_and_2022();
    test_alpha_requires_st2110_20_2022();
    test_st2115logs3_requires_st2110_20_2022();
    test_range_cross_field_validation();

    test_sdp_ingestion_applies_420_cross_field_validation();
    test_sdp_ingestion_applies_key_cross_field_validation();
    test_sdp_ingestion_applies_ssn_cross_field_validation();
    test_sdp_ingestion_applies_range_cross_field_validation();

    return 0;
}
#include "st2110/video_sdp_fmtp.hpp"

#include <cassert>
#include <string_view>

namespace {
    constexpr std::string_view valid_minimal_fmtp =
    "sampling=YCbCr-4:2:2; "
    "width=1920; "
    "height=1080; "
    "exactframerate=30000/1001; "
    "depth=10; "
    "colorimetry=BT709; "
    "PM=2110GPM; "
    "SSN=ST2110-20:2017";

    [[nodiscard]] std::string with_required_prefix(std::string_view suffix) {
        std::string out{valid_minimal_fmtp};

        if (!suffix.empty()) {
            out += "; ";
            out += suffix;
        }

        return out;
    }

    void accepts_valid_strict_fmtp_payload() {
        const auto parsed = st2110::parse_video_sdp_fmtp_payload(valid_minimal_fmtp);

        assert(parsed.has_value());
        assert(parsed->sampling == "YCbCr-4:2:2");
        assert(parsed->width == 1920);
        assert(parsed->height == 1080);
        assert(parsed->exactframerate.numerator == 30000);
        assert(parsed->exactframerate.denominator == 1001);
        assert(parsed->depth == 10);
        assert(!parsed->depth_floating_point);
        assert(parsed->colorimetry == "BT709");
        assert(parsed->packing_mode == "2110GPM");
        assert(parsed->signal_standard == "ST2110-20:2017");
    }

    void accepts_valid_unknown_parameters_and_flags() {
        const auto payload = with_required_prefix("X-PRIVATE=abc123; X-FLAG");
        const auto parsed = st2110::parse_video_sdp_fmtp_payload(payload);

        assert(parsed.has_value());
        assert(parsed->unknown_parameters.size() == 2);

        assert(parsed->unknown_parameters[0].name == "X-PRIVATE");
        assert(parsed->unknown_parameters[0].value.has_value());
        assert(*parsed->unknown_parameters[0].value == "abc123");

        assert(parsed->unknown_parameters[1].name == "X-FLAG");
        assert(!parsed->unknown_parameters[1].value.has_value());
    }

    void rejects_missing_whitespace_after_semicolon() {
        const std::string payload =
                "sampling=YCbCr-4:2:2;"
                "width=1920; "
                "height=1080; "
                "exactframerate=30000/1001; "
                "depth=10; "
                "colorimetry=BT709; "
                "PM=2110GPM; "
                "SSN=ST2110-20:2017";

        const auto parsed = st2110::parse_video_sdp_fmtp_payload(payload);

        assert(!parsed.has_value());
        assert(parsed.error() == st2110::Error::InvalidValue);
    }

    void rejects_empty_parameters_from_doubled_separator() {
        const std::string payload =
                "sampling=YCbCr-4:2:2; ; "
                "width=1920; "
                "height=1080; "
                "exactframerate=30000/1001; "
                "depth=10; "
                "colorimetry=BT709; "
                "PM=2110GPM; "
                "SSN=ST2110-20:2017";

        const auto parsed = st2110::parse_video_sdp_fmtp_payload(payload);

        assert(!parsed.has_value());
        assert(parsed.error() == st2110::Error::InvalidValue);
    }

    void rejects_trailing_empty_parameter() {
        const std::string payload = std::string(valid_minimal_fmtp) + "; ";

        const auto parsed = st2110::parse_video_sdp_fmtp_payload(payload);

        assert(!parsed.has_value());
        assert(parsed.error() == st2110::Error::InvalidValue);
    }

    void rejects_whitespace_before_equals() {
        const std::string payload =
                "sampling=YCbCr-4:2:2; "
                "width =1920; "
                "height=1080; "
                "exactframerate=30000/1001; "
                "depth=10; "
                "colorimetry=BT709; "
                "PM=2110GPM; "
                "SSN=ST2110-20:2017";

        const auto parsed = st2110::parse_video_sdp_fmtp_payload(payload);

        assert(!parsed.has_value());
        assert(parsed.error() == st2110::Error::InvalidValue);
    }

    void rejects_whitespace_after_equals() {
        const std::string payload =
                "sampling=YCbCr-4:2:2; "
                "width= 1920; "
                "height=1080; "
                "exactframerate=30000/1001; "
                "depth=10; "
                "colorimetry=BT709; "
                "PM=2110GPM; "
                "SSN=ST2110-20:2017";

        const auto parsed = st2110::parse_video_sdp_fmtp_payload(payload);

        assert(!parsed.has_value());
        assert(parsed.error() == st2110::Error::InvalidValue);
    }

    void rejects_unknown_parameter_with_whitespace_around_equals() {
        const auto payload = with_required_prefix("X-PRIVATE =abc123");
        const auto parsed = st2110::parse_video_sdp_fmtp_payload(payload);

        assert(!parsed.has_value());
        assert(parsed.error() == st2110::Error::InvalidValue);
    }

    void rejects_duplicate_known_field_as_before() {
        const auto payload = with_required_prefix("width=1280");
        const auto parsed = st2110::parse_video_sdp_fmtp_payload(payload);

        assert(!parsed.has_value());
        assert(parsed.error() == st2110::Error::InvalidValue);
    }

    void accepts_existing_depth_16f_path() {
        const std::string payload =
                "sampling=YCbCr-4:2:2; "
                "width=1920; "
                "height=1080; "
                "exactframerate=50; "
                "depth=16f; "
                "colorimetry=BT709; "
                "PM=2110GPM; "
                "SSN=ST2110-20:2017";

        const auto parsed = st2110::parse_video_sdp_fmtp_payload(payload);

        assert(parsed.has_value());
        assert(parsed->depth == 16);
        assert(parsed->depth_floating_point);
        assert(parsed->exactframerate.numerator == 50);
        assert(parsed->exactframerate.denominator == 1);
    }
}

int main() {
    accepts_valid_strict_fmtp_payload();
    accepts_valid_unknown_parameters_and_flags();
    rejects_missing_whitespace_after_semicolon();
    rejects_empty_parameters_from_doubled_separator();
    rejects_trailing_empty_parameter();
    rejects_whitespace_before_equals();
    rejects_whitespace_after_equals();
    rejects_unknown_parameter_with_whitespace_around_equals();
    rejects_duplicate_known_field_as_before();
    accepts_existing_depth_16f_path();

    return 0;
}
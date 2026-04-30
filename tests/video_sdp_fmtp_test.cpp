#include "st2110/video_sdp_fmtp.hpp"

#include <cassert>
#include <optional>
#include <string_view>

using namespace st2110;

namespace {
void test_parse_fmtp_payload_with_required_optional_flags_and_unknowns() {
    const std::string_view payload = "sampling=YCbCr-4:2:2; "
                                     "width=1920; "
                                     "height=1080; "
                                     "exactframerate=60000/1001; "
                                     "depth=10; "
                                     "colorimetry=BT709; "
                                     "PM=2110GPM; "
                                     "SSN=ST2110-20:2017; "
                                     "TCS=SDR; "
                                     "RANGE=NARROW; "
                                     "interlace; "
                                     "segmented; "
                                     "FOO=bar; "
                                     "BAR";

    auto parsed = parse_video_sdp_fmtp_payload(payload);
    assert(parsed.has_value());

    const auto &fmtp = *parsed;

    assert(fmtp.sampling == "YCbCr-4:2:2");
    assert(fmtp.width == 1920);
    assert(fmtp.height == 1080);
    assert(fmtp.exactframerate.numerator == 60000);
    assert(fmtp.exactframerate.denominator == 1001);
    assert(fmtp.depth == 10);
    assert(fmtp.colorimetry == "BT709");
    assert(fmtp.packing_mode == "2110GPM");
    assert(fmtp.signal_standard == "ST2110-20:2017");

    assert(fmtp.transfer_characteristic_system.has_value());
    assert(*fmtp.transfer_characteristic_system == "SDR");

    assert(fmtp.range.has_value());
    assert(*fmtp.range == "NARROW");

    assert(fmtp.interlace);
    assert(fmtp.segmented);

    assert(fmtp.unknown_parameters.size() == 2);

    assert(fmtp.unknown_parameters[0].name == "FOO");
    assert(fmtp.unknown_parameters[0].value.has_value());
    assert(*fmtp.unknown_parameters[0].value == "bar");

    assert(fmtp.unknown_parameters[1].name == "BAR");
    assert(!fmtp.unknown_parameters[1].value.has_value());
}

void test_parse_fmtp_payload_accepts_integer_exactframerate() {
    const std::string_view payload = "sampling=YCbCr-4:2:2; "
                                     "width=1280; "
                                     "height=720; "
                                     "exactframerate=30000; "
                                     "depth=8; "
                                     "colorimetry=BT709; "
                                     "PM=2110GPM; "
                                     "SSN=ST2110-20:2022";

    auto parsed = parse_video_sdp_fmtp_payload(payload);
    assert(parsed.has_value());

    const auto &fmtp = *parsed;
    assert(fmtp.exactframerate.numerator == 30000);
    assert(fmtp.exactframerate.denominator == 1);
}

void test_parse_fmtp_attribute_parses_matching_payload_type() {
    const std::string_view line = "a=fmtp:112 "
                                  "sampling=YCbCr-4:2:2; "
                                  "width=1920; "
                                  "height=1080; "
                                  "exactframerate=60000/1001; "
                                  "depth=10; "
                                  "colorimetry=BT709; "
                                  "PM=2110GPM; "
                                  "SSN=ST2110-20:2017";

    auto parsed = parse_video_sdp_fmtp_attribute(line, 112);
    assert(parsed.has_value());
    assert(parsed->has_value());

    const auto &fmtp = **parsed;
    assert(fmtp.width == 1920);
    assert(fmtp.height == 1080);
    assert(fmtp.exactframerate.numerator == 60000);
    assert(fmtp.exactframerate.denominator == 1001);
}

void test_parse_fmtp_attribute_returns_nullopt_for_payload_type_mismatch() {
    const std::string_view line = "a=fmtp:113 "
                                  "sampling=YCbCr-4:2:2; "
                                  "width=1920; "
                                  "height=1080; "
                                  "exactframerate=60000/1001; "
                                  "depth=10; "
                                  "colorimetry=BT709; "
                                  "PM=2110GPM; "
                                  "SSN=ST2110-20:2017";

    auto parsed = parse_video_sdp_fmtp_attribute(line, 112);
    assert(parsed.has_value());
    assert(!parsed->has_value());
}

void test_parse_fmtp_payload_rejects_malformed_numeric_value() {
    const std::string_view payload = "sampling=YCbCr-4:2:2; "
                                     "width=abc; "
                                     "height=1080; "
                                     "exactframerate=60000/1001; "
                                     "depth=10; "
                                     "colorimetry=BT709; "
                                     "PM=2110GPM; "
                                     "SSN=ST2110-20:2017";

    auto parsed = parse_video_sdp_fmtp_payload(payload);
    assert(!parsed.has_value());
    assert(parsed.error() == Error::InvalidValue);
}

void test_parse_fmtp_payload_rejects_malformed_exactframerate() {
    const std::string_view payload = "sampling=YCbCr-4:2:2; "
                                     "width=1920; "
                                     "height=1080; "
                                     "exactframerate=60000/; "
                                     "depth=10; "
                                     "colorimetry=BT709; "
                                     "PM=2110GPM; "
                                     "SSN=ST2110-20:2017";

    auto parsed = parse_video_sdp_fmtp_payload(payload);
    assert(!parsed.has_value());
    assert(parsed.error() == Error::InvalidValue);
}

void test_parse_fmtp_payload_rejects_duplicate_known_key() {
    const std::string_view payload = "sampling=YCbCr-4:2:2; "
                                     "sampling=RGB; "
                                     "width=1920; "
                                     "height=1080; "
                                     "exactframerate=60000/1001; "
                                     "depth=10; "
                                     "colorimetry=BT709; "
                                     "PM=2110GPM; "
                                     "SSN=ST2110-20:2017";

    auto parsed = parse_video_sdp_fmtp_payload(payload);
    assert(!parsed.has_value());
    assert(parsed.error() == Error::InvalidValue);
}

void test_parse_fmtp_payload_rejects_duplicate_flag() {
    const std::string_view payload = "sampling=YCbCr-4:2:2; "
                                     "width=1920; "
                                     "height=1080; "
                                     "exactframerate=60000/1001; "
                                     "depth=10; "
                                     "colorimetry=BT709; "
                                     "PM=2110GPM; "
                                     "SSN=ST2110-20:2017; "
                                     "interlace; "
                                     "interlace";

    auto parsed = parse_video_sdp_fmtp_payload(payload);
    assert(!parsed.has_value());
    assert(parsed.error() == Error::InvalidValue);
}

void test_parse_fmtp_payload_rejects_missing_required_key() {
    const std::string_view payload = "sampling=YCbCr-4:2:2; "
                                     "width=1920; "
                                     "height=1080; "
                                     "depth=10; "
                                     "colorimetry=BT709; "
                                     "PM=2110GPM; "
                                     "SSN=ST2110-20:2017";

    auto parsed = parse_video_sdp_fmtp_payload(payload);
    assert(!parsed.has_value());
    assert(parsed.error() == Error::InvalidValue);
}
} // namespace

int main() {
    test_parse_fmtp_payload_with_required_optional_flags_and_unknowns();
    test_parse_fmtp_payload_accepts_integer_exactframerate();
    test_parse_fmtp_attribute_parses_matching_payload_type();
    test_parse_fmtp_attribute_returns_nullopt_for_payload_type_mismatch();
    test_parse_fmtp_payload_rejects_malformed_numeric_value();
    test_parse_fmtp_payload_rejects_malformed_exactframerate();
    test_parse_fmtp_payload_rejects_duplicate_known_key();
    test_parse_fmtp_payload_rejects_duplicate_flag();
    test_parse_fmtp_payload_rejects_missing_required_key();
    return 0;
}
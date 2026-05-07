// tests/video_sdp_optional_sender_timing_test.cpp

#include "st2110/ingress/video/video_sdp_ingestion.hpp"
#include "st2110/video_receiver_timing_signaling.hpp"
#include "st2110/video_signaling.hpp"

#include <cassert>
#include <iostream>
#include <string_view>

namespace {
constexpr std::string_view base_sdp_prefix = "v=0\r\n"
                                             "o=- 0 0 IN IP4 127.0.0.1\r\n"
                                             "s=ST2110 optional sender timing test\r\n"
                                             "t=0 0\r\n"
                                             "m=video 5004 RTP/AVP 96\r\n"
                                             "a=rtpmap:96 raw/90000\r\n";

constexpr std::string_view base_sdp_suffix = "a=ts-refclk:ptp=IEEE1588-2008:00-11-22-33-44-55-66-77\r\n"
                                             "a=mediaclk:direct=0\r\n";

std::string make_sdp(std::string_view fmtp_extra) {
    std::string sdp;
    sdp += base_sdp_prefix;
    sdp += "a=fmtp:96 sampling=YCbCr-4:2:2; width=1920; height=1080; "
           "exactframerate=25; depth=8; colorimetry=BT709; "
           "PM=2110GPM; SSN=ST2110-20:2017";
    sdp += fmtp_extra;
    sdp += "\r\n";
    sdp += base_sdp_suffix;
    return sdp;
}

st2110::VideoReceiverTimingConfig timing_cfg_supporting_wide() {
    st2110::VideoReceiverTimingConfig cfg{};
    cfg.capability.supports_type_n = false;
    cfg.capability.supports_type_nl = false;
    cfg.capability.supports_type_w = true;
    cfg.requirements.consume_sender_cmax = true;
    return cfg;
}

st2110::VideoReceiverTimingConfig timing_cfg_rejecting_wide() {
    st2110::VideoReceiverTimingConfig cfg{};
    cfg.capability.supports_type_n = true;
    cfg.capability.supports_type_nl = true;
    cfg.capability.supports_type_w = false;
    cfg.requirements.consume_sender_cmax = true;
    return cfg;
}
} // namespace

int main() {
    {
        const auto sdp = make_sdp("; TP=2110TPW; CMAX=9000");

        auto parsed = st2110::parse_video_stream_signaling_from_sdp(sdp, 96);
        assert(parsed.has_value());
        assert(parsed->sender_type == st2110::VideoSenderType::Wide);
        assert(parsed->cmax.has_value());
        assert(*parsed->cmax == 9000);

        assert(st2110::validate_video_stream_signaling(*parsed) == st2110::Error::Ok);

        auto bootstrap = st2110::video_receiver_bootstrap_config_from_video_stream_signaling(
            *parsed, 5004, 96, "0.0.0.0", "239.1.1.1", st2110::PartialFramePolicy::Drop);
        assert(bootstrap.has_value());
    }

    {
        const auto sdp = make_sdp("; TP=2110TPW");

        auto parsed = st2110::parse_video_stream_signaling_from_sdp(sdp, 96);
        assert(parsed.has_value());
        assert(parsed->sender_type == st2110::VideoSenderType::Wide);
        assert(!parsed->cmax.has_value());

        assert(st2110::validate_video_stream_signaling(*parsed) == st2110::Error::Ok);

        auto bootstrap = st2110::video_receiver_bootstrap_config_from_video_stream_signaling(
            *parsed, 5004, 96, "0.0.0.0", "239.1.1.1", st2110::PartialFramePolicy::Drop);
        assert(bootstrap.has_value());

        auto timing_bootstrap = st2110::video_receiver_bootstrap_config_from_video_stream_signaling(
            *parsed, timing_cfg_supporting_wide(), 5004, 96, "0.0.0.0", "239.1.1.1", st2110::PartialFramePolicy::Drop);
        assert(timing_bootstrap.has_value());
    }

    {
        const auto sdp = make_sdp("; TP=2110TPW; CMAX=0");

        auto parsed = st2110::parse_video_stream_signaling_from_sdp(sdp, 96);
        assert(!parsed.has_value());
        assert(parsed.error() == st2110::Error::InvalidValue);
    }

    {
        const auto sdp = make_sdp("; TP=2110TPW");

        auto parsed = st2110::parse_video_stream_signaling_from_sdp(sdp, 96);
        assert(parsed.has_value());

        auto timing_bootstrap = st2110::video_receiver_bootstrap_config_from_video_stream_signaling(
            *parsed, timing_cfg_rejecting_wide(), 5004, 96, "0.0.0.0", "239.1.1.1", st2110::PartialFramePolicy::Drop);

        assert(!timing_bootstrap.has_value());
        assert(timing_bootstrap.error() == st2110::Error::Unsupported);
    }

    {
        const auto sdp = make_sdp("; TP=2110TPN; TROFF=11; CMAX=9000");

        auto parsed = st2110::parse_video_stream_signaling_from_sdp(sdp, 96);
        assert(parsed.has_value());
        assert(parsed->sender_type == st2110::VideoSenderType::Narrow);
        assert(parsed->troff_us.has_value());
        assert(*parsed->troff_us == 11);
        assert(parsed->cmax.has_value());
        assert(*parsed->cmax == 9000);
    }

    {
        const auto sdp = make_sdp("; TP=2110TPNL; TROFF=11; CMAX=9000");

        auto parsed = st2110::parse_video_stream_signaling_from_sdp(sdp, 96);
        assert(parsed.has_value());
        assert(parsed->sender_type == st2110::VideoSenderType::NarrowLinear);
        assert(parsed->troff_us.has_value());
        assert(*parsed->troff_us == 11);
        assert(parsed->cmax.has_value());
        assert(*parsed->cmax == 9000);
    }

    {
        const auto sdp = make_sdp("; TP=2110TPN; TROFF=0");

        auto parsed = st2110::parse_video_stream_signaling_from_sdp(sdp, 96);
        assert(!parsed.has_value());
        assert(parsed.error() == st2110::Error::InvalidValue);
    }

    std::cout << "video_sdp_optional_sender_timing_test passed\n";
    return 0;
}
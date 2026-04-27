#include "st2110/video_playout_timing.hpp"
#include "st2110/video_timestamp_mapping.hpp"
#include "st2110/video_frame.hpp"
#include "st2110/video_unit_reconstructor.hpp"

#include <cassert>
#include <cstdint>
#include <limits>

using namespace st2110;

static void validates_default_and_nonzero_link_offset_delay() {
    VideoReceiverPlayoutTimingConfig cfg{};

    assert(validate_video_receiver_playout_timing_config(cfg) == Error::Ok);

    cfg.link_offset_delay_ns = 5'000'000;
    assert(validate_video_receiver_playout_timing_config(cfg) == Error::Ok);
}

static void computes_reconstruction_time_from_mapped_media_timestamp() {
    const VideoReceiverPlayoutTimingConfig cfg{
            .link_offset_delay_ns = 5'000'000
    };

    auto decision = video_receiver_playout_timing_decision(
            1'000'000'000ULL,
            cfg
    );

    assert(decision.has_value());
    assert(decision->media_timestamp_ns == 1'000'000'000ULL);
    assert(decision->reconstruction_timestamp_ns == 1'005'000'000ULL);
}

static void rejects_reconstruction_timestamp_overflow() {
    const VideoReceiverPlayoutTimingConfig cfg{
            .link_offset_delay_ns = 1
    };

    auto decision = video_receiver_playout_timing_decision(
            std::numeric_limits<TimestampNs>::max(),
            cfg
    );

    assert(!decision.has_value());
    assert(decision.error() == Error::InvalidValue);
}

static void keeps_rtp_timestamp_mapping_separate_from_playout_timing() {
    VideoRtpTimestampMapper mapper(VideoRtpTimestampMapperConfig{
            .rtp_clock_rate = 90'000,
            .anchor_rtp_timestamp = 10'000,
            .anchor_timestamp_ns = 2'000'000'000ULL
    });

    auto mapped_media_timestamp = mapper.map(100'000);
    assert(mapped_media_timestamp.has_value());

    const VideoReceiverPlayoutTimingConfig cfg{
            .link_offset_delay_ns = 250'000
    };

    auto decision = video_receiver_playout_timing_decision(
            *mapped_media_timestamp,
            cfg
    );

    assert(decision.has_value());
    assert(decision->media_timestamp_ns == 3'000'000'000ULL);
    assert(decision->reconstruction_timestamp_ns == 3'000'250'000ULL);
}

static void attaches_playout_timing_to_reconstructed_frame_metadata() {
    ReconstructedVideoFrame frame{
            VideoFrame(2, 2, PixelFormat::UYVY),
            1234U,
            true
    };

    const VideoReceiverPlayoutTimingConfig cfg{
            .link_offset_delay_ns = 100
    };

    auto timing = video_reconstructed_frame_timing(
            frame,
            50'000,
            cfg
    );

    assert(timing.has_value());
    assert(timing->rtp_timestamp == 1234U);
    assert(timing->media_timestamp_ns == 50'000);
    assert(timing->reconstruction_timestamp_ns == 50'100);
}

int main() {
    validates_default_and_nonzero_link_offset_delay();
    computes_reconstruction_time_from_mapped_media_timestamp();
    rejects_reconstruction_timestamp_overflow();
    keeps_rtp_timestamp_mapping_separate_from_playout_timing();
    attaches_playout_timing_to_reconstructed_frame_metadata();

    return 0;
}
#include "obs-synchronized-frame-sink.hpp"

#include <stdexcept>

ObsSynchronizedFrameSink::ObsSynchronizedFrameSink(const st2110::SynchronizedFrameSinkConfig &cfg)
    : st2110::SynchronizedFrameSink(cfg) {}

void ObsSynchronizedFrameSink::deliver_video_frame_to_obs(st2110::VideoFrame &&frame,
                                                          st2110::FrameTimingMetadata timing,
                                                          st2110::TimestampNs media_timestamp_ns) {
    (void)frame;
    (void)timing;
    (void)media_timestamp_ns;

    throw std::logic_error("Implement me!");
}

void ObsSynchronizedFrameSink::deliver_audio_block_to_obs(st2110::AudioBuffer &&block,
                                                          st2110::FrameTimingMetadata timing,
                                                          st2110::TimestampNs media_timestamp_ns) {
    (void)block;
    (void)timing;
    (void)media_timestamp_ns;

    throw std::logic_error("Implement me!");
}
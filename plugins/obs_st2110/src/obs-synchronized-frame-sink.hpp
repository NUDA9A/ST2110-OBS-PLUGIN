#ifndef ST2110_OBS_PLUGIN_OBS_SYNCHRONIZED_FRAME_SINK_HPP
#define ST2110_OBS_PLUGIN_OBS_SYNCHRONIZED_FRAME_SINK_HPP

#include <st2110/delivery/synchronized_frame_sink.hpp>

class ObsSynchronizedFrameSink final : public st2110::SynchronizedFrameSink {
public:
    explicit ObsSynchronizedFrameSink(const st2110::SynchronizedFrameSinkConfig &cfg);

protected:
    void deliver_video_frame_to_obs(st2110::VideoFrame &&frame, st2110::FrameTimingMetadata timing,
                                    st2110::TimestampNs media_timestamp_ns) override;

    void deliver_audio_block_to_obs(st2110::AudioBuffer &&block, st2110::FrameTimingMetadata timing,
                                    st2110::TimestampNs media_timestamp_ns) override;
};

#endif // ST2110_OBS_PLUGIN_OBS_SYNCHRONIZED_FRAME_SINK_HPP
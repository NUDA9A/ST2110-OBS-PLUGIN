#ifndef ST2110_OBS_PLUGIN_SIGNALING_STRUCTS_HPP
#define ST2110_OBS_PLUGIN_SIGNALING_STRUCTS_HPP

#include "ingress/shared/packet_parse.hpp"
#include "receive/video/video_receive_pipeline.hpp"
#include "receive/video/video_reorder_policy.hpp"
#include "receive/video/video_timestamp_mapping.hpp"
#include "rx_config.hpp"
#include "video_receiver_timing.hpp"

namespace st2110 {
struct VideoReceiverBootstrapConfig {
    PacketParsePolicy packet_parse_policy{};
    RxVideoConfig rx_config{};
    VideoReceivePipelineConfig receive_pipeline_config{};
    VideoRtpTimestampMapperConfig timestamp_mapper_config{};
    VideoReorderBufferConfig reorder_buffer_config{};
    VideoReceiverTimingConfig timing_config{};
};
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_SIGNALING_STRUCTS_HPP
#ifndef ST2110_OBS_PLUGIN_SIGNALING_STRUCTS_HPP
#define ST2110_OBS_PLUGIN_SIGNALING_STRUCTS_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "ingress/shared/packet_parse.hpp"
#include "model/video/video_packing_mode.hpp"
#include "model/video/video_scan_mode.hpp"
#include "receive/video/video_receive_pipeline.hpp"
#include "receive/video/video_reorder_policy.hpp"
#include "receive/video/video_timestamp_mapping.hpp"
#include "rx_config.hpp"
#include "video_receive_capability.hpp"
#include "video_receiver_timing.hpp"

namespace st2110 {
enum class MediaClockMode { Direct, Sender };

enum class TimestampMode { Samp, New, Pres };

enum class ReferenceClockKind { LocalMac, Ptp, Other };

struct PtpReferenceClock {
    // IEEE1588 clock identity from ts-refclk:ptp=...
    // representation format can stay implementation-chosen for now
    std::array<uint8_t, 8> clock_identity{};
    uint16_t domain_number = 0;
    bool traceable = false;
};

struct LocalMacReferenceClock {
    std::array<uint8_t, 6> mac{};
};

struct ReferenceClock {
    ReferenceClockKind kind = ReferenceClockKind::Ptp;

    std::optional<PtpReferenceClock> ptp{};
    std::optional<LocalMacReferenceClock> local_mac{};

    // preserve future extensibility / unknown RFC forms
    std::optional<std::string> raw_token{};
};

enum class VideoSenderType { Narrow, NarrowLinear, Wide };

struct VideoStreamSignaling {
    VideoMediaDescription media{};
    VideoScanMode scan_mode = VideoScanMode::Progressive;
    VideoPackingMode packing_mode = VideoPackingMode::Gpm;
    std::optional<std::size_t> max_udp_datagram_bytes{};

    MediaClockMode media_clock_mode = MediaClockMode::Direct;
    TimestampMode timestamp_mode = TimestampMode::New;
    ReferenceClock reference_clock{};

    uint32_t ts_delay_sender_ticks = 0;

    VideoSenderType sender_type = VideoSenderType::Narrow;
    std::optional<uint32_t> troff_us{};
    std::optional<uint32_t> cmax{};
};

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
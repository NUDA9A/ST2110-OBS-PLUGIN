#ifndef ST2110_OBS_PLUGIN_BACKEND_HPP
#define ST2110_OBS_PLUGIN_BACKEND_HPP

#include "st2110/delivery/audio/audio_frame.hpp"
#include "st2110/delivery/video/video_frame.hpp"
#include "st2110/foundation/error.hpp"
#include "st2110/ingress/shared/packet_parse_stats.hpp"
#include "st2110/receive/shared/reorder_stats.hpp"
#include "st2110/receive/video/depacketizer_stats.hpp"
#include "st2110/rx_config.hpp"

#include <expected>

namespace st2110 {
using RxBackendLifecycleResult = std::expected<bool, Error>;

struct BackendStats {
    uint64_t datagrams_received = 0;
    uint64_t bytes_received = 0;

    uint64_t control_datagrams_ignored = 0;
    uint64_t nonmedia_datagrams_ignored = 0;

    uint64_t packets_parsed_ok = 0;
    uint64_t packets_rejected = 0;

    uint64_t frames_delivered = 0;
    uint64_t datagrams_dropped = 0;
    uint64_t media_units_delivered = 0;

    PacketParseStats packet_parse{};
    ReorderBufferStats reorder{};
    DepacketizerStats depacketizer{};
};

class IVideoFrameSink {
  public:
    virtual void on_video_frame(const VideoFrameView &frame) = 0;

    virtual ~IVideoFrameSink() = default;
};

class IAudioFrameSink {
  public:
    virtual void on_audio_frame(const AudioFrameView &frame) = 0;

    virtual ~IAudioFrameSink() = default;
};

class IRxBackend {
  public:
    virtual RxBackendLifecycleResult stop() = 0;

    virtual ~IRxBackend() = default;

    [[nodiscard]] virtual BackendStats stats() const = 0;
};

class IRxVideoBackend : public virtual IRxBackend {
  public:
    virtual RxBackendLifecycleResult start_video(IVideoFrameSink &sink) = 0;

    ~IRxVideoBackend() override = default;
};

class IRxAudioBackend : public virtual IRxBackend {
  public:
    virtual RxBackendLifecycleResult start_audio(IAudioFrameSink &sink) = 0;

    ~IRxAudioBackend() override = default;
};
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_BACKEND_HPP

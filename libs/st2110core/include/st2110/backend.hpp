#ifndef ST2110_OBS_PLUGIN_BACKEND_HPP
#define ST2110_OBS_PLUGIN_BACKEND_HPP
#include "video_frame.hpp"
#include "rx_config.hpp"

namespace st2110 {
class IFrameSink {
public:
    virtual void on_frame(const FrameView &frame) = 0;
    virtual ~IFrameSink() = default;
};

class IRxVideoBackend {
public:
    virtual const char *backend_name() const = 0;

    virtual void start(const RxVideoConfig &cfg, IFrameSink &sink) = 0;

    virtual void stop() = 0;

    virtual ~IRxVideoBackend() = default;
};

}

#endif //ST2110_OBS_PLUGIN_BACKEND_HPP

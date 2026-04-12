#ifndef ST2110_OBS_PLUGIN_BACKEND_HPP
#define ST2110_OBS_PLUGIN_BACKEND_HPP

#include "video_frame.hpp"
#include "rx_config.hpp"

namespace st2110 {
    class IVideoFrameSink {
    public:
        virtual void on_video_frame(const VideoFrameView &frame) = 0;

        virtual ~IVideoFrameSink() = default;
    };

    class IRxBackend {
    public:
        virtual const char *backend_name() const = 0;

        virtual void stop() = 0;

        virtual ~IRxBackend() = default;
    };

    class IRxVideoBackend : public IRxBackend {
    public:
        virtual void start(const RxVideoConfig &cfg, IVideoFrameSink &sink) = 0;

        ~IRxVideoBackend() override = default;
    };

}

#endif //ST2110_OBS_PLUGIN_BACKEND_HPP

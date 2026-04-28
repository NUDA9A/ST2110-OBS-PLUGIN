#ifndef ST2110_OBS_SOCKET_RX_VIDEO_BACKEND_HPP
#define ST2110_OBS_SOCKET_RX_VIDEO_BACKEND_HPP
#include "backend.hpp"
#include "backend_factory.hpp"

namespace st2110
{
    class SocketRxVideoBackend final : public IRxVideoBackend
    {
    public:
        [[nodiscard]] const char* backend_name() const override
        {
            return "socket";
        }

        void stop() override
        {
        }

        [[nodiscard]] RxBackendCapabilities capabilities() const override
        {
            return RxBackendCapabilities{.video_rx = true, .audio_rx = false};
        }

        void start_video(const RxVideoConfig& cfg, IVideoFrameSink& sink) override
        {
            (void)cfg;
            (void)sink;
        }
    };

    class SocketRxVideoBackendFactory final : public IRxBackendFactory
    {
    public:
        [[nodiscard]] RxBackendDescriptor descriptor() const override
        {
            return RxBackendDescriptor{
                .kind = RxBackendKind::Socket,
                .name = "socket",
                .capabilities = RxBackendCapabilities{
                    .video_rx = true, .audio_rx = false
                },
                .available = true
            };
        }

        [[nodiscard]] std::unique_ptr<IRxBackend> create_backend() const override
        {
            return std::unique_ptr<IRxBackend>(new SocketRxVideoBackend());
        }
    };
}

#endif //ST2110_OBS_SOCKET_RX_VIDEO_BACKEND_HPP

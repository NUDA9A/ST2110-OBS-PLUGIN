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

        RxBackendLifecycleResult stop() override
        {
            state_ = {};
            return state_;
        }

        [[nodiscard]] RxBackendCapabilities capabilities() const override
        {
            return RxBackendCapabilities{.video_rx = true, .audio_rx = false};
        }

        RxBackendLifecycleResult start_video(const RxVideoConfig& cfg, IVideoFrameSink& sink) override
        {
            if (state_.video_active)
            {
                return std::unexpected(Error::InvalidBackendState);
            }
            state_.video_active = true;
            (void)cfg;
            (void)sink;

            return state_;
        }

        [[nodiscard]] RxBackendState state() const override
        {
            return state_;
        }
    private:
        RxBackendState state_{};
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

#ifndef ST2110_OBS_PLUGIN_BACKEND_HPP
#define ST2110_OBS_PLUGIN_BACKEND_HPP

#include "audio_frame.hpp"
#include "rx_config.hpp"
#include "video_frame.hpp"
#include "error.hpp"

#include <expected>


namespace st2110
{
    enum class RxMediaKind { Video, Audio };

    struct RxBackendCapabilities
    {
        bool video_rx = false;
        bool audio_rx = false;
    };

    struct RxBackendState
    {
        bool video_active = false;
        bool audio_active = false;
    };

    using RxBackendLifecycleResult = std::expected<RxBackendState, Error>;

    inline bool backend_is_stopped(const RxBackendState& state)
    {
        return !state.video_active && !state.audio_active;
    }

    inline bool backend_media_active(const RxBackendState& state, RxMediaKind kind)
    {
        switch (kind)
        {
            case RxMediaKind::Video:
                return state.video_active;
            case RxMediaKind::Audio:
                return state.audio_active;
            default:
                return false;
        }
    }

    inline bool supports_media(const RxBackendCapabilities& capabilities, RxMediaKind media_kind) noexcept
    {
        switch (media_kind)
        {
        case RxMediaKind::Video:
            return capabilities.video_rx;
        case RxMediaKind::Audio:
            return capabilities.audio_rx;
        }

        return false;
    }

    class IVideoFrameSink
    {
    public:
        virtual void on_video_frame(const VideoFrameView& frame) = 0;

        virtual ~IVideoFrameSink() = default;
    };

    class IAudioFrameSink
    {
    public:
        virtual void on_audio_frame(const AudioFrameView& frame) = 0;

        virtual ~IAudioFrameSink() = default;
    };

    class IRxBackend
    {
    public:
        [[nodiscard]] virtual const char* backend_name() const = 0;

        virtual RxBackendLifecycleResult stop() = 0;

        [[nodiscard]] virtual RxBackendState state() const = 0;

        virtual ~IRxBackend() = default;

        [[nodiscard]] virtual RxBackendCapabilities capabilities() const = 0;
    };

    class IRxVideoBackend : public virtual IRxBackend
    {
    public:
        virtual RxBackendLifecycleResult start_video(const RxVideoConfig& cfg, IVideoFrameSink& sink) = 0;

        ~IRxVideoBackend() override = default;
    };

    class IRxAudioBackend : public virtual IRxBackend
    {
    public:
        virtual RxBackendLifecycleResult start_audio(const RxAudioConfig& cfg, IAudioFrameSink& sink) = 0;

        ~IRxAudioBackend() override = default;
    };
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_BACKEND_HPP

#ifndef ST2110_OBS_PLUGIN_SYNCHRONIZED_PLAYOUT_TUNING_HPP
#define ST2110_OBS_PLUGIN_SYNCHRONIZED_PLAYOUT_TUNING_HPP

#include <st2110/foundation/timestamp.hpp>
#include <st2110/receive/audio/audio_receive_bootstrap.hpp>
#include <st2110/receive/video/video_receive_bootstrap.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace st2110 {

inline constexpr TimestampNs synchronizedPlayoutVideoJitterMarginNs = 5'000'000ULL;
inline constexpr TimestampNs synchronizedPlayoutAudioMinimumDelayNs = 20'000'000ULL;
inline constexpr std::uint64_t synchronizedPlayoutAudioPacketTimeMultiplier = 20;

inline constexpr std::size_t synchronizedPlayoutMinimumVideoQueueFrames = 4;
inline constexpr std::size_t synchronizedPlayoutVideoQueueMarginFrames = 2;

inline constexpr std::size_t synchronizedPlayoutMinimumAudioQueueBlocks = 32;
inline constexpr std::size_t synchronizedPlayoutAudioQueueMarginBlocks = 8;

struct SynchronizedPlayoutTuning {
    TimestampNs playout_delay_ns = 0;
    std::size_t max_queued_video_frames = 0;
    std::size_t max_queued_audio_blocks = 0;
};

[[nodiscard]] inline std::uint64_t synchronized_playout_ceil_div_u64(const std::uint64_t numerator,
                                                                     const std::uint64_t denominator) noexcept {
    if (denominator == 0) {
        return 0;
    }

    return numerator / denominator + ((numerator % denominator) == 0 ? 0 : 1);
}

[[nodiscard]] inline TimestampNs
synchronized_playout_video_frame_period_ns(const VideoMediaDescription &media) noexcept {
    if (media.fps_num == 0 || media.fps_den == 0) {
        return 0;
    }

    constexpr std::uint64_t ns_per_second = 1'000'000'000ULL;
    return synchronized_playout_ceil_div_u64(ns_per_second * static_cast<std::uint64_t>(media.fps_den),
                                             static_cast<std::uint64_t>(media.fps_num));
}

[[nodiscard]] inline TimestampNs
synchronized_playout_audio_packet_time_ns(const AudioMediaDescription &media) noexcept {
    return static_cast<TimestampNs>(media.packet_time_us) * 1'000ULL;
}

[[nodiscard]] inline TimestampNs
derive_video_playout_delay_ns(const std::optional<VideoReceiveBootstrap> &video_bootstrap) noexcept {
    if (!video_bootstrap.has_value()) {
        return 0;
    }

    const TimestampNs frame_period_ns = synchronized_playout_video_frame_period_ns(video_bootstrap->stream.media);
    if (frame_period_ns == 0) {
        return synchronizedPlayoutVideoJitterMarginNs;
    }

    return frame_period_ns + synchronizedPlayoutVideoJitterMarginNs;
}

[[nodiscard]] inline TimestampNs
derive_audio_playout_delay_ns(const std::optional<AudioReceiveBootstrap> &audio_bootstrap) noexcept {
    if (!audio_bootstrap.has_value()) {
        return 0;
    }

    const TimestampNs packet_time_ns = synchronized_playout_audio_packet_time_ns(audio_bootstrap->stream.media);
    if (packet_time_ns == 0) {
        return synchronizedPlayoutAudioMinimumDelayNs;
    }

    return std::max(synchronizedPlayoutAudioMinimumDelayNs,
                    packet_time_ns * synchronizedPlayoutAudioPacketTimeMultiplier);
}

[[nodiscard]] inline std::size_t
derive_max_queued_video_frames(const std::optional<VideoReceiveBootstrap> &video_bootstrap,
                               const TimestampNs playout_delay_ns) noexcept {
    if (!video_bootstrap.has_value()) {
        return 0;
    }

    const TimestampNs frame_period_ns = synchronized_playout_video_frame_period_ns(video_bootstrap->stream.media);
    if (frame_period_ns == 0) {
        return synchronizedPlayoutMinimumVideoQueueFrames;
    }

    const auto delay_frames =
        static_cast<std::size_t>(synchronized_playout_ceil_div_u64(playout_delay_ns, frame_period_ns));

    return std::max(synchronizedPlayoutMinimumVideoQueueFrames,
                    delay_frames + synchronizedPlayoutVideoQueueMarginFrames);
}

[[nodiscard]] inline std::size_t
derive_max_queued_audio_blocks(const std::optional<AudioReceiveBootstrap> &audio_bootstrap,
                               const TimestampNs playout_delay_ns) noexcept {
    if (!audio_bootstrap.has_value()) {
        return 0;
    }

    const TimestampNs packet_time_ns = synchronized_playout_audio_packet_time_ns(audio_bootstrap->stream.media);
    if (packet_time_ns == 0) {
        return synchronizedPlayoutMinimumAudioQueueBlocks;
    }

    const auto delay_blocks =
        static_cast<std::size_t>(synchronized_playout_ceil_div_u64(playout_delay_ns, packet_time_ns));

    return std::max(synchronizedPlayoutMinimumAudioQueueBlocks,
                    delay_blocks + synchronizedPlayoutAudioQueueMarginBlocks);
}

[[nodiscard]] inline SynchronizedPlayoutTuning
derive_synchronized_playout_tuning(const std::optional<VideoReceiveBootstrap> &video_bootstrap,
                                   const std::optional<AudioReceiveBootstrap> &audio_bootstrap) noexcept {
    const TimestampNs video_delay_ns = derive_video_playout_delay_ns(video_bootstrap);
    const TimestampNs audio_delay_ns = derive_audio_playout_delay_ns(audio_bootstrap);

    SynchronizedPlayoutTuning result{
        .playout_delay_ns = std::max(video_delay_ns, audio_delay_ns),
        .max_queued_video_frames = 0,
        .max_queued_audio_blocks = 0,
    };

    result.max_queued_video_frames = derive_max_queued_video_frames(video_bootstrap, result.playout_delay_ns);
    result.max_queued_audio_blocks = derive_max_queued_audio_blocks(audio_bootstrap, result.playout_delay_ns);

    return result;
}

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_SYNCHRONIZED_PLAYOUT_TUNING_HPP
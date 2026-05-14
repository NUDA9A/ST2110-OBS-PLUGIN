#include "obs-synchronized-frame-sink.hpp"

#include <st2110/delivery/video/pixel_format.hpp>

#include <obs-module.h>

#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>

namespace {

[[nodiscard]] bool fits_obs_linesize(const std::size_t value) noexcept {
    return value <= std::numeric_limits<std::uint32_t>::max();
}

[[nodiscard]] std::optional<video_format> map_video_format_to_obs(const st2110::PixelFormat format) noexcept {
    switch (format) {
    case st2110::PixelFormat::UYVY:
        return VIDEO_FORMAT_UYVY;

    case st2110::PixelFormat::BGRA:
        return VIDEO_FORMAT_BGRA;

    case st2110::PixelFormat::V210:
        return VIDEO_FORMAT_V210;

    case st2110::PixelFormat::YUV420PLANAR8:
        return VIDEO_FORMAT_I420;

    case st2110::PixelFormat::YUV422PLANAR8:
        return VIDEO_FORMAT_I422;

    default:
        return std::nullopt;
    }
}

[[nodiscard]] speaker_layout map_audio_channels_to_obs_speakers(const std::uint16_t channels) {
    switch (channels) {
    case 1:
        return SPEAKERS_MONO;
    case 2:
        return SPEAKERS_STEREO;
    case 3:
        return SPEAKERS_2POINT1;
    case 4:
        return SPEAKERS_4POINT0;
    case 5:
        return SPEAKERS_4POINT1;
    case 6:
        return SPEAKERS_5POINT1;
    case 8:
        return SPEAKERS_7POINT1;
    default:
        throw std::runtime_error("Unsupported OBS audio channel layout");
    }
}

void fill_obs_video_color_fields(obs_source_frame &obs_frame) {
    if (!format_is_yuv(obs_frame.format)) {
        obs_frame.full_range = true;
        return;
    }

    /*
     * Current project VideoFrame does not yet carry colorimetry/range metadata.
     * Use a localized OBS handoff policy for now instead of widening the core
     * frame contract inside the OBS adapter.
     */
    obs_frame.full_range = false;

    (void)video_format_get_parameters_for_format(VIDEO_CS_709, VIDEO_RANGE_PARTIAL, obs_frame.format,
                                                 obs_frame.color_matrix, obs_frame.color_range_min,
                                                 obs_frame.color_range_max);
}

} // namespace

ObsSynchronizedFrameSink::ObsSynchronizedFrameSink(obs_source_t *source, const st2110::SynchronizedFrameSinkConfig &cfg)
    : st2110::SynchronizedFrameSink(cfg), source_(source) {}

void ObsSynchronizedFrameSink::deliver_video_frame_to_obs(st2110::VideoFrame &&frame,
                                                          st2110::FrameTimingMetadata timing,
                                                          st2110::TimestampNs media_timestamp_ns) {
    (void)timing;

    if (!source_) {
        throw std::runtime_error("OBS source is not available");
    }

    const auto obs_format = map_video_format_to_obs(frame.format());
    if (!obs_format.has_value()) {
        throw std::runtime_error("Unsupported OBS video pixel format");
    }

    obs_source_frame obs_frame{};
    obs_frame.width = frame.width();
    obs_frame.height = frame.height();
    obs_frame.timestamp = media_timestamp_ns;
    obs_frame.format = *obs_format;
    obs_frame.flip = false;

    for (std::size_t plane = 0; plane < frame.plane_count(); ++plane) {
        const std::size_t stride = frame.stride_bytes(plane);
        if (!fits_obs_linesize(stride)) {
            throw std::runtime_error("OBS video line size overflow");
        }

        obs_frame.data[plane] = frame.data(plane);
        obs_frame.linesize[plane] = static_cast<std::uint32_t>(stride);
    }

    fill_obs_video_color_fields(obs_frame);

    obs_source_output_video(source_, &obs_frame);
}

void ObsSynchronizedFrameSink::deliver_audio_block_to_obs(st2110::AudioBuffer &&block,
                                                          st2110::FrameTimingMetadata timing,
                                                          st2110::TimestampNs media_timestamp_ns) {
    (void)timing;

    if (!source_) {
        throw std::runtime_error("OBS source is not available");
    }

    obs_source_audio obs_audio{};
    obs_audio.data[0] = reinterpret_cast<const std::uint8_t *>(block.samples());
    obs_audio.frames = block.samples_per_channel();
    obs_audio.speakers = map_audio_channels_to_obs_speakers(block.channel_count());
    obs_audio.format = AUDIO_FORMAT_32BIT;
    obs_audio.samples_per_sec = block.sampling_rate_hz();
    obs_audio.timestamp = media_timestamp_ns;

    obs_source_output_audio(source_, &obs_audio);
}
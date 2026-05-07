#ifndef ST2110_OBS_PLUGIN_CONFIG_VALIDATION_HPP
#define ST2110_OBS_PLUGIN_CONFIG_VALIDATION_HPP

#include <cstdint>
#include <expected>
#include <limits>
#include <string_view>

#include "delivery/video/pixel_format.hpp"
#include "foundation/error.hpp"
#include "model/video/video_scan_mode.hpp"

namespace st2110::config_validation {
[[nodiscard]] inline std::expected<uint32_t, Error>
audio_samples_per_packet_from_rate_and_packet_time(uint32_t sampling_rate_hz, uint32_t packet_time_us) {
    constexpr uint64_t microseconds_per_second = 1'000'000;

    if (sampling_rate_hz == 0 || packet_time_us == 0) {
        return std::unexpected(Error::InvalidValue);
    }

    const uint64_t product = static_cast<uint64_t>(sampling_rate_hz) * static_cast<uint64_t>(packet_time_us);

    if ((product % microseconds_per_second) != 0) {
        return std::unexpected(Error::InvalidValue);
    }

    const uint64_t samples_per_packet = product / microseconds_per_second;

    if (samples_per_packet == 0 || samples_per_packet > std::numeric_limits<uint32_t>::max()) {
        return std::unexpected(Error::InvalidValue);
    }

    return static_cast<uint32_t>(samples_per_packet);
}

[[nodiscard]] inline bool is_non_empty(std::string_view s) { return !s.empty(); }

[[nodiscard]] inline bool is_dynamic_rtp_payload_type(uint8_t pt) { return pt >= 96 && pt <= 127; }

[[nodiscard]] inline Error validate_frame_rate(uint32_t num, uint32_t den) {
    if (num == 0 || den == 0) {
        return Error::InvalidValue;
    }
    return Error::Ok;
}

[[nodiscard]] inline Error validate_udp_port(uint16_t port) {
    if (port == 0) {
        return Error::InvalidValue;
    }
    return Error::Ok;
}

[[nodiscard]] inline Error validate_video_dimensions(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        return Error::InvalidValue;
    }
    return Error::Ok;
}

[[nodiscard]] inline Error validate_video_format_constraints(PixelFormat fmt, uint32_t width, uint32_t height) {
    const Error dims_err = validate_video_dimensions(width, height);
    if (dims_err != Error::Ok) {
        return dims_err;
    }

    switch (fmt) {
    case PixelFormat::UYVY:
        if ((width % 2) != 0) {
            return Error::InvalidValue;
        }
        return Error::Ok;
    default:
        return Error::Unsupported;
    }
}

[[nodiscard]] inline Error validate_video_scan_mode(VideoScanMode mode) {
    switch (mode) {
    case VideoScanMode::Progressive:
    case VideoScanMode::Interlaced:
    case VideoScanMode::PsF:
        return Error::Ok;
    default:
        return Error::InvalidValue;
    }
}
} // namespace st2110::config_validation

#endif // ST2110_OBS_PLUGIN_CONFIG_VALIDATION_HPP

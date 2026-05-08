#ifndef ST2110_OBS_DERIVED_VALUES_HPP
#define ST2110_OBS_DERIVED_VALUES_HPP

#include "st2110/foundation/error.hpp"

#include <cstdint>
#include <expected>
#include <limits>

namespace st2110 {

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

} // namespace st2110

#endif // ST2110_OBS_DERIVED_VALUES_HPP
#ifndef ST2110_OBS_PLUGIN_PACKET_ADMISSION_HPP
#define ST2110_OBS_PLUGIN_PACKET_ADMISSION_HPP

#include <cstdint>

#include "st2110/foundation/error.hpp"
#include "st2110/ingress/shared/packet_view.hpp"

namespace st2110 {

[[nodiscard]] inline Error validate_rtp_payload_type_admission(std::uint8_t parsed_payload_type,
                                                        std::uint8_t expected_payload_type) noexcept {
    if (parsed_payload_type != expected_payload_type) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}

[[nodiscard]] inline Error validate_video_packet_payload_type_admission(const PacketView &packet,
                                                                 std::uint8_t expected_payload_type) noexcept {
    return validate_rtp_payload_type_admission(packet.rtp.payload_type, expected_payload_type);
}

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_PACKET_ADMISSION_HPP
#ifndef ST2110_OBS_PLUGIN_PACKET_PARSE_HPP
#define ST2110_OBS_PLUGIN_PACKET_PARSE_HPP

#include "error.hpp"
#include "bytes.hpp"
#include "packet_view.hpp"

#include <cstdint>
#include <optional>
#include <expected>

namespace st2110 {
    struct PacketParsePolicy {
        std::optional<std::size_t> max_udp_payload_bytes{};
    };

    [[nodiscard]] inline Error validate_packet_parse_policy(
            ByteSpan udp_payload,
            const PacketParsePolicy& policy) {
        if (policy.max_udp_payload_bytes.has_value() && udp_payload.size() > *policy.max_udp_payload_bytes) {
            return Error::InvalidValue;
        }

        return Error::Ok;
    }

    [[nodiscard]] inline std::expected<PacketView, Error> parse_packet_view(
            ByteSpan udp_payload,
            const PacketParsePolicy& policy = {}) {
        if (Error err = validate_packet_parse_policy(udp_payload, policy); err != Error::Ok) {
            return std::unexpected(err);
        }

        return PacketView::from_udp_datagram(udp_payload);
    }

}

#endif //ST2110_OBS_PLUGIN_PACKET_PARSE_HPP

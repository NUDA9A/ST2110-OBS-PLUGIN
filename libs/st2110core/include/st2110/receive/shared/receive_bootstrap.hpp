#ifndef ST2110_OBS_RECEIVE_BOOTSTRAP_HPP
#define ST2110_OBS_RECEIVE_BOOTSTRAP_HPP

#include <st2110/ingress/shared/parsed_sdp.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace st2110 {
enum class ReceiveTopologyKind {
    SingleStream,
    RedundantPair,
};

struct ReceiveSignaledStream {
    std::uint8_t expected_payload_type = 0;
    StreamTimingSignaling timing{};
};

struct ReceiveRemoteLeg {
    std::optional<std::string> mid{};

    std::uint16_t udp_port = 0;
    ParsedSdpConnectionEndpoint destination{};

    SourceFilterSignaling source_filter{};
    std::size_t max_udp_datagram_bytes{};
};

struct ReceiveBootstrap {
    ReceiveTopologyKind topology = ReceiveTopologyKind::SingleStream;
    std::vector<ReceiveRemoteLeg> legs{};
};

inline ReceiveRemoteLeg project_receive_remote_leg(const ParsedSdpConnectionEndpoint &connection,
                                                   const std::uint16_t udp_port,
                                                   const StreamTransportSignaling &transport) {
    return ReceiveRemoteLeg{
        .mid = transport.mid,
        .udp_port = udp_port,
        .destination = connection,
        .source_filter = transport.source_filters[0],
        .max_udp_datagram_bytes = transport.max_udp_datagram_bytes.value_or(1460),
    };
}

inline ReceiveBootstrap project_video_receive_bootstrap(const ParsedSdpStreamSet &parsed) {
    std::vector<ReceiveRemoteLeg> legs(parsed.legs.size());
    for (std::size_t i = 0; i < legs.size(); ++i) {
        legs[i] = project_receive_remote_leg(parsed.legs[i].connection, parsed.legs[i].udp_port,
                                             parsed.legs[i].video_stream_signaling->transport);
    }

    return ReceiveBootstrap{
        .topology = parsed.is_duplicated() ? ReceiveTopologyKind::RedundantPair : ReceiveTopologyKind::SingleStream,
        .legs = std::move(legs),
    };
}

inline ReceiveBootstrap project_audio_receive_bootstrap(const ParsedSdpStreamSet &parsed) {
    std::vector<ReceiveRemoteLeg> legs(parsed.legs.size());
    for (std::size_t i = 0; i < legs.size(); ++i) {
        legs[i] = project_receive_remote_leg(parsed.legs[i].connection, parsed.legs[i].udp_port,
                                             parsed.legs[i].audio_stream_signaling->transport);
    }

    return ReceiveBootstrap{
        .topology = parsed.is_duplicated() ? ReceiveTopologyKind::RedundantPair : ReceiveTopologyKind::SingleStream,
        .legs = std::move(legs),
    };
}

inline ReceiveSignaledStream project_receive_signaled_stream(const std::uint8_t expected_payload_type,
                                                             const StreamTimingSignaling &timing) {
    return ReceiveSignaledStream{.expected_payload_type = expected_payload_type, .timing = timing};
}

} // namespace st2110

#endif // ST2110_OBS_RECEIVE_BOOTSTRAP_HPP

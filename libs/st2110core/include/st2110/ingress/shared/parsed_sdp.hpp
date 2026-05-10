#ifndef ST2110_OBS_PARSED_SDP_HPP
#define ST2110_OBS_PARSED_SDP_HPP

#include <st2110/model/audio/audio_signaling.hpp>
#include <st2110/model/video/video_signaling_types.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace st2110 {
struct ParsedSdpConnectionEndpoint {
    std::string network_type{};
    std::string address_type{};
    std::string destination_address{};

    std::optional<std::uint8_t> ttl{};
    std::optional<std::uint16_t> address_count{};
};

struct ParsedSdpStreamLeg {
    std::uint8_t expected_payload_type = 0;
    std::uint16_t udp_port = 0;

    ParsedSdpConnectionEndpoint connection{};

    std::optional<VideoStreamSignaling> video_stream_signaling{};
    std::optional<AudioStreamSignaling> audio_stream_signaling{};
};

struct ParsedSdpStreamSet {
    std::vector<ParsedSdpStreamLeg> legs{};

    [[nodiscard]] bool is_duplicated() const { return legs.size() == 2; }
};
} // namespace st2110

#endif // ST2110_OBS_PARSED_SDP_HPP

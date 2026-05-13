#ifndef ST2110_OBS_SOCKET_AUDIO_START_CONFIG_HPP
#define ST2110_OBS_SOCKET_AUDIO_START_CONFIG_HPP

#include <st2110/delivery/socket_start_config.hpp>

namespace st2110 {
struct SocketAudioStreamConfig {
    std::uint8_t expected_payload_type = 0;

    AudioMediaDescription media{};
};

struct SocketVideoStartConfig : SocketStartConfig {
    SocketAudioStreamConfig stream{};
};
}

#endif // ST2110_OBS_SOCKET_AUDIO_START_CONFIG_HPP

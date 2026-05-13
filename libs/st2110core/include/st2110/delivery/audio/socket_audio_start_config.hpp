#ifndef ST2110_OBS_SOCKET_AUDIO_START_CONFIG_HPP
#define ST2110_OBS_SOCKET_AUDIO_START_CONFIG_HPP

#include <st2110/contracts/settings.hpp>
#include <st2110/delivery/socket_start_config.hpp>
#include <st2110/model/audio/audio_signaling.hpp>
#include <st2110/receive/audio/audio_receive_bootstrap.hpp>
#include <st2110/receive/shared/receive_start_request.hpp>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <variant>

namespace st2110 {
struct SocketAudioStreamConfig {
    std::uint8_t expected_payload_type = 0;

    AudioMediaDescription media{};
    std::optional<AudioChannelOrder> channel_order{};

    std::uint32_t samples_per_packet = 0;
};

struct SocketAudioStartConfig : SocketStartConfig {
    SocketAudioStreamConfig stream{};
};

[[nodiscard]] inline SocketAudioStreamConfig make_socket_audio_stream_config(const AudioReceiveBootstrap &bootstrap) {
    const auto &media = bootstrap.stream.media;

    const auto samples_per_packet = audio_samples_per_packet_from_media_description(media);
    if (!samples_per_packet) {
        throw std::runtime_error("Can not derive audio samples per packet");
    }

    return SocketAudioStreamConfig{
        .expected_payload_type = bootstrap.stream.receive_signaled_stream.expected_payload_type,
        .media = media,
        .channel_order = bootstrap.stream.channel_order,
        .samples_per_packet = *samples_per_packet,
    };
}

inline SocketAudioStartConfig project_receive_start_request_to_socket_audio_start(const ReceiveStartRequest &request,
                                                                                  const Settings settings) {
    SocketAudioStartConfig res{};

    const auto &bootstrap = std::get<AudioReceiveBootstrap>(request.media);

    res.topology = bootstrap.receive_bootstrap.topology;
    res.reorder_buffer_config = settings.reorder_buffer_config;
    res.stream = make_socket_audio_stream_config(bootstrap);
    res.legs = make_socket_media_leg_configs(bootstrap.receive_bootstrap, request.local);

    return res;
}

} // namespace st2110

#endif // ST2110_OBS_SOCKET_AUDIO_START_CONFIG_HPP
#ifndef ST2110_OBS_SOCKET_AUDIO_START_CONFIG_HPP
#define ST2110_OBS_SOCKET_AUDIO_START_CONFIG_HPP

#include <st2110/backends/receive_local_policy.hpp>
#include <st2110/backends/socket/platform/socket_runtime.hpp>
#include <st2110/contracts/settings.hpp>
#include <st2110/delivery/socket_start_config.hpp>
#include <st2110/model/audio/audio_signaling.hpp>
#include <st2110/receive/audio/audio_receive_bootstrap.hpp>
#include <st2110/receive/shared/receive_start_request.hpp>

#include <cstddef>
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

[[nodiscard]] inline std::optional<SocketSourceFilter>
make_socket_source_filter(const SourceFilterSignaling &source_filter, const SocketAddressFamily family) {
    if (source_filter.source_addresses.empty()) {
        return std::nullopt;
    }

    return SocketSourceFilter{
        .family = family,
        .source_addresses = source_filter.source_addresses,
    };
}

[[nodiscard]] inline SocketAudioStreamConfig make_socket_audio_stream_config(const AudioReceiveBootstrap &bootstrap) {
    const auto &media = bootstrap.stream.media;

    auto samples_per_packet = audio_samples_per_packet_from_media_description(media);
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

    for (std::size_t i = 0; i < bootstrap.receive_bootstrap.legs.size(); ++i) {
        const auto &remote_leg = bootstrap.receive_bootstrap.legs[i];
        const auto &local_leg = request.local.legs[i];

        const auto source_filter = make_socket_source_filter(remote_leg.source_filter, local_leg.family);

        const auto open_config = build_socket_rx_open_config(remote_leg.udp_port, local_leg.local_ip,
                                                             remote_leg.destination.destination_address,
                                                             source_filter);
        if (!open_config) {
            throw std::runtime_error("Can not build socket open config");
        }

        res.legs.emplace_back(local_leg.family, local_leg.local_ip, remote_leg.destination.destination_address,
                              remote_leg.udp_port, remote_leg.source_filter, remote_leg.max_udp_datagram_bytes,
                              *open_config);
    }

    return res;
}

} // namespace st2110

#endif // ST2110_OBS_SOCKET_AUDIO_START_CONFIG_HPP
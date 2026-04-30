#ifndef ST2110_OBS_PLUGIN_AUDIO_RECEIVER_BOOTSTRAP_HPP
#define ST2110_OBS_PLUGIN_AUDIO_RECEIVER_BOOTSTRAP_HPP

#include "audio_signaling.hpp"
#include "audio_signaling_rx_config.hpp"
#include "audio_channel_order.hpp"
#include "rx_config.hpp"
#include "error.hpp"

#include <expected>
#include <string>

namespace st2110 {
struct AudioReceiverBootstrapConfig {
    RxAudioConfig rx_config;
    ParsedAudioChannelOrder channel_order;
};

[[nodiscard]] inline std::expected<AudioReceiverBootstrapConfig, Error>
audio_receiver_bootstrap_config_from_audio_stream_signaling(const AudioStreamSignaling &signaling, uint16_t udp_port,
                                                            uint8_t payload_type, std::string local_ip,
                                                            std::string dest_ip,
                                                            AudioSampleFormat format = AudioSampleFormat::LinearPcm) {
    auto rx_config =
        rx_audio_config_from_audio_stream_signaling(signaling, udp_port, payload_type, local_ip, dest_ip, format);
    if (!rx_config) {
        return std::unexpected(rx_config.error());
    }

    auto effective_audio_channel_order = effective_audio_channel_order_from_audio_stream_signaling(signaling);
    if (!effective_audio_channel_order) {
        return std::unexpected(effective_audio_channel_order.error());
    }

    return AudioReceiverBootstrapConfig{*rx_config, *effective_audio_channel_order};
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_AUDIO_RECEIVER_BOOTSTRAP_HPP

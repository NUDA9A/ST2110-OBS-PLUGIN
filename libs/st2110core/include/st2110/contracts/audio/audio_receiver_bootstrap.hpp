#ifndef ST2110_OBS_PLUGIN_AUDIO_RECEIVER_BOOTSTRAP_HPP
#define ST2110_OBS_PLUGIN_AUDIO_RECEIVER_BOOTSTRAP_HPP

#include "st2110/receive/audio/audio_reorder_buffer.hpp"
#include "st2110/receive/audio/audio_timestamp_mapping.hpp"
#include "st2110/foundation/error.hpp"
#include "st2110/ingress/shared/packet_parse.hpp"
#include "st2110/model/audio/audio_channel_order.hpp"
#include "st2110/receive/audio/audio_frame_assembler.hpp"
#include "st2110/receive/audio/audio_packet.hpp"
#include "st2110/rx_config.hpp"
#include "st2110/model/audio/audio_signaling.hpp"
#include "audio_signaling_rx_config.hpp"

#include <expected>
#include <string>

namespace st2110 {
struct AudioReceiverBootstrapConfig {
    PacketParsePolicy packet_parse_policy{};
    RxAudioConfig rx_config{};
    AudioRtpPacketPolicy audio_packet_policy{};
    AudioFrameAssemblerConfig frame_assembler_config{};
    AudioReorderBufferConfig reorder_buffer_config{};
    AudioRtpTimestampMapperConfig timestamp_mapper_config{};
    ParsedAudioChannelOrder channel_order{};
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

    auto audio_packet_policy = audio_rtp_packet_policy_from_rx_audio_config(*rx_config);
    if (!audio_packet_policy) {
        return std::unexpected(audio_packet_policy.error());
    }

    const AudioFrameAssemblerConfig frame_assembler_config{
        .storage_format = AudioSampleStorageFormat::InterleavedS32,
    };
    if (const Error err = validate_audio_frame_assembler_config(frame_assembler_config); err != Error::Ok) {
        return std::unexpected(err);
    }

    const AudioReorderBufferConfig reorder_buffer_config{};
    if (const Error err = validate_audio_reorder_buffer_config(reorder_buffer_config); err != Error::Ok) {
        return std::unexpected(err);
    }

    const AudioRtpTimestampMapperConfig timestamp_mapper_config =
        audio_rtp_timestamp_mapper_config_first_observed_local_zero(rx_config->sampling_rate_hz);
    if (const Error err = validate_audio_rtp_timestamp_mapper_config(timestamp_mapper_config); err != Error::Ok) {
        return std::unexpected(err);
    }

    return AudioReceiverBootstrapConfig{
        .packet_parse_policy = PacketParsePolicy{},
        .rx_config = *rx_config,
        .audio_packet_policy = *audio_packet_policy,
        .frame_assembler_config = frame_assembler_config,
        .reorder_buffer_config = reorder_buffer_config,
        .timestamp_mapper_config = timestamp_mapper_config,
        .channel_order = *effective_audio_channel_order,
    };
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_AUDIO_RECEIVER_BOOTSTRAP_HPP

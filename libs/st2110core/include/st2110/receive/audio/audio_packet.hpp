#ifndef ST2110_OBS_PLUGIN_AUDIO_PACKET_HPP
#define ST2110_OBS_PLUGIN_AUDIO_PACKET_HPP

#include "st2110/foundation/bytes.hpp"
#include "st2110/foundation/error.hpp"
#include "st2110/ingress/shared/packet_view.hpp"
#include "st2110/model/audio/audio_signaling.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <utility>

namespace st2110 {
struct AudioPacketView final : PacketView {
    std::uint32_t sampling_rate_hz = 0;
    std::uint16_t channel_count = 0;
    std::uint32_t samples_per_channel = 0;
    AudioPcmBitDepth pcm_bit_depth = AudioPcmBitDepth::Bits24;

    [[nodiscard]] std::unique_ptr<StoredPacket> store() const override;
    [[nodiscard]] std::uint32_t reorder_sequence() const override { return rtp.seq_number; }
};

[[nodiscard]] inline std::size_t audio_pcm_wire_sample_bytes(const AudioPcmBitDepth bit_depth) {
    switch (bit_depth) {
    case AudioPcmBitDepth::Bits16:
        return 2;
    case AudioPcmBitDepth::Bits24:
        return 3;
    default:
        std::unreachable();
    }
}

[[nodiscard]] inline std::size_t audio_rtp_packet_payload_size_bytes(const AudioPcmBitDepth bit_depth,
                                                                     const std::uint32_t samples_per_packet,
                                                                     const std::uint16_t channel_count) {
    const auto bytes_per_sample = audio_pcm_wire_sample_bytes(bit_depth);
    return static_cast<std::size_t>(samples_per_packet) * static_cast<std::size_t>(channel_count) * bytes_per_sample;
}

[[nodiscard]] inline AudioPacketView make_audio_rtp_packet_view(const RtpHeaderView &rtp, const ByteSpan payload,
                                                                const AudioMediaDescription &media,
                                                                const std::uint32_t samples_per_packet) {
    AudioPacketView res{};
    res.rtp = rtp;
    res.payload_data = payload;
    res.sampling_rate_hz = media.sampling_rate_hz;
    res.channel_count = media.channel_count;
    res.samples_per_channel = samples_per_packet;
    res.pcm_bit_depth = media.pcm_bit_depth;

    return res;
}

[[nodiscard]] inline std::expected<AudioPacketView, Error>
parse_audio_rtp_packet_view(const ByteSpan udp_payload, const AudioMediaDescription &media,
                            const std::uint32_t samples_per_packet) {
    auto rtp_header = parse_rtp_header(udp_payload);
    if (!rtp_header) {
        return std::unexpected(rtp_header.error());
    }

    const auto payload = rtp_payload_span(udp_payload, *rtp_header);

    if (payload.size() !=
        audio_rtp_packet_payload_size_bytes(media.pcm_bit_depth, samples_per_packet, media.channel_count)) {
        return std::unexpected(Error::InvalidValue);
    }

    return make_audio_rtp_packet_view(*rtp_header, payload, media, samples_per_packet);
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_AUDIO_PACKET_HPP

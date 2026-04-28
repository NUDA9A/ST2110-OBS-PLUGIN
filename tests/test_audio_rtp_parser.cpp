#include <st2110/audio_packet.hpp>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>

namespace {
    using namespace st2110;

    void write_be16(std::vector<uint8_t>& bytes, std::size_t offset, uint16_t value) {
        bytes[offset + 0] = static_cast<uint8_t>((value >> 8) & 0xFF);
        bytes[offset + 1] = static_cast<uint8_t>(value & 0xFF);
    }

    void write_be32(std::vector<uint8_t>& bytes, std::size_t offset, uint32_t value) {
        bytes[offset + 0] = static_cast<uint8_t>((value >> 24) & 0xFF);
        bytes[offset + 1] = static_cast<uint8_t>((value >> 16) & 0xFF);
        bytes[offset + 2] = static_cast<uint8_t>((value >> 8) & 0xFF);
        bytes[offset + 3] = static_cast<uint8_t>(value & 0xFF);
    }

    RxAudioConfig make_level_a_rx_audio_config(uint8_t payload_type = 96, uint16_t channel_count = 2) {
        RxAudioConfig cfg{};
        cfg.sampling_rate_hz = 48000;
        cfg.packet_time_us = 1000;
        cfg.samples_per_packet = 48;
        cfg.channel_count = channel_count;
        cfg.udp_port = 5004;
        cfg.payload_type = payload_type;
        cfg.local_ip = "";
        cfg.dest_ip = "239.1.1.1";
        cfg.format = AudioSampleFormat::LinearPcm;
        return cfg;
    }

    AudioRtpPacketPolicy make_policy(
            AudioPcmWireFormat wire_format = AudioPcmWireFormat::L24,
            uint8_t payload_type = 96,
            uint16_t channel_count = 2) {
        auto policy = audio_rtp_packet_policy_from_rx_audio_config(
                make_level_a_rx_audio_config(payload_type, channel_count),
                wire_format);
        assert(policy.has_value());
        return *policy;
    }

    std::vector<uint8_t> make_payload(const AudioRtpPacketPolicy& policy, uint8_t start = 0x10) {
        auto size = audio_rtp_packet_payload_size_bytes(policy);
        assert(size.has_value());

        std::vector<uint8_t> payload(*size);
        for (std::size_t i = 0; i < payload.size(); ++i) {
            payload[i] = static_cast<uint8_t>(start + (i & 0x3F));
        }
        return payload;
    }

    std::vector<uint8_t> make_rtp_packet(
            uint8_t payload_type,
            bool marker,
            uint16_t sequence_number,
            uint32_t timestamp,
            uint32_t ssrc,
            const std::vector<uint8_t>& payload) {
        std::vector<uint8_t> packet(12 + payload.size());

        packet[0] = 0x80; // V=2, P=0, X=0, CC=0
        packet[1] = static_cast<uint8_t>((marker ? 0x80 : 0x00) | (payload_type & 0x7F));
        write_be16(packet, 2, sequence_number);
        write_be32(packet, 4, timestamp);
        write_be32(packet, 8, ssrc);

        std::copy(payload.begin(), payload.end(), packet.begin() + 12);
        return packet;
    }

    std::vector<uint8_t> make_rtp_packet_with_csrc_and_extension(
            uint8_t payload_type,
            bool marker,
            uint16_t sequence_number,
            uint32_t timestamp,
            uint32_t ssrc,
            uint32_t csrc,
            const std::vector<uint8_t>& payload) {
        constexpr std::size_t fixed_header_bytes = 12;
        constexpr std::size_t csrc_bytes = 4;
        constexpr std::size_t extension_header_bytes = 4;
        constexpr std::size_t extension_payload_bytes = 4;

        const std::size_t payload_offset =
                fixed_header_bytes + csrc_bytes + extension_header_bytes + extension_payload_bytes;

        std::vector<uint8_t> packet(payload_offset + payload.size());

        packet[0] = 0x91; // V=2, P=0, X=1, CC=1
        packet[1] = static_cast<uint8_t>((marker ? 0x80 : 0x00) | (payload_type & 0x7F));
        write_be16(packet, 2, sequence_number);
        write_be32(packet, 4, timestamp);
        write_be32(packet, 8, ssrc);

        write_be32(packet, 12, csrc);

        // RFC 3550 header extension:
        // 16-bit profile-specific id + 16-bit extension length in 32-bit words.
        write_be16(packet, 16, 0xBEDE);
        write_be16(packet, 18, 1);
        packet[20] = 0xA1;
        packet[21] = 0xA2;
        packet[22] = 0xA3;
        packet[23] = 0xA4;

        std::copy(payload.begin(), payload.end(), packet.begin() + static_cast<std::ptrdiff_t>(payload_offset));
        return packet;
    }

    void parses_valid_l24_audio_rtp_packet() {
        const auto policy = make_policy(AudioPcmWireFormat::L24, 96, 2);
        const auto payload = make_payload(policy, 0x20);
        const auto packet = make_rtp_packet(
                policy.payload_type,
                true,
                0x1234,
                0x01020304,
                0xAABBCCDD,
                payload);

        const auto parsed = parse_audio_rtp_packet_view(ByteSpan{packet.data(), packet.size()}, policy);
        assert(parsed.has_value());

        assert(parsed->rtp.version == 2);
        assert(parsed->rtp.marker);
        assert(parsed->rtp.payload_type == policy.payload_type);
        assert(parsed->rtp.seq_number == 0x1234);
        assert(parsed->rtp.timestamp == 0x01020304);
        assert(parsed->rtp.ssrc == 0xAABBCCDD);
        assert(parsed->rtp.payload_offset == 12);

        assert(parsed->payload.size() == payload.size());
        assert(parsed->payload.data() == packet.data() + 12);
        assert(parsed->payload[0] == payload[0]);

        assert(parsed->sampling_rate_hz == 48000);
        assert(parsed->channel_count == 2);
        assert(parsed->samples_per_channel == 48);
        assert(parsed->wire_format == AudioPcmWireFormat::L24);
    }

    void parses_valid_l16_audio_rtp_packet() {
        const auto policy = make_policy(AudioPcmWireFormat::L16, 97, 1);
        const auto payload = make_payload(policy, 0x30);
        const auto packet = make_rtp_packet(
                policy.payload_type,
                false,
                7,
                48000,
                0x01010101,
                payload);

        const auto parsed = parse_audio_rtp_packet_view(ByteSpan{packet.data(), packet.size()}, policy);
        assert(parsed.has_value());

        assert(!parsed->rtp.marker);
        assert(parsed->rtp.payload_type == 97);
        assert(parsed->payload.size() == 48 * 1 * 2);
        assert(parsed->channel_count == 1);
        assert(parsed->samples_per_channel == 48);
        assert(parsed->wire_format == AudioPcmWireFormat::L16);
    }

    void tolerates_csrc_and_rtp_header_extension() {
        const auto policy = make_policy(AudioPcmWireFormat::L24, 98, 2);
        const auto payload = make_payload(policy, 0x40);
        const auto packet = make_rtp_packet_with_csrc_and_extension(
                policy.payload_type,
                true,
                100,
                123456,
                0x11111111,
                0x22222222,
                payload);

        const auto parsed = parse_audio_rtp_packet_view(ByteSpan{packet.data(), packet.size()}, policy);
        assert(parsed.has_value());

        assert(parsed->rtp.extension_flag);
        assert(parsed->rtp.csrc_count == 1);
        assert(parsed->rtp.payload_offset == 24);
        assert(parsed->payload.size() == payload.size());
        assert(parsed->payload.data() == packet.data() + 24);
        assert(parsed->payload[0] == payload[0]);
    }

    void rejects_payload_type_mismatch() {
        const auto policy = make_policy(AudioPcmWireFormat::L24, 96, 2);
        const auto payload = make_payload(policy);
        const auto packet = make_rtp_packet(
                97,
                false,
                1,
                2,
                3,
                payload);

        const auto parsed = parse_audio_rtp_packet_view(ByteSpan{packet.data(), packet.size()}, policy);
        assert(!parsed.has_value());
        assert(parsed.error() == Error::InvalidValue);
    }

    void rejects_payload_size_mismatch() {
        const auto policy = make_policy(AudioPcmWireFormat::L24, 96, 2);
        auto payload = make_payload(policy);
        assert(!payload.empty());
        payload.pop_back();

        const auto packet = make_rtp_packet(
                policy.payload_type,
                false,
                1,
                2,
                3,
                payload);

        const auto parsed = parse_audio_rtp_packet_view(ByteSpan{packet.data(), packet.size()}, policy);
        assert(!parsed.has_value());
        assert(parsed.error() == Error::InvalidValue);
    }

    void rejects_short_rtp_packet() {
        const auto policy = make_policy(AudioPcmWireFormat::L24, 96, 2);
        const std::vector<uint8_t> packet(11, 0);

        const auto parsed = parse_audio_rtp_packet_view(ByteSpan{packet.data(), packet.size()}, policy);
        assert(!parsed.has_value());
    }

    void rejects_bad_rtp_version_before_audio_payload_policy() {
        const auto policy = make_policy(AudioPcmWireFormat::L24, 96, 2);
        const auto payload = make_payload(policy);
        auto packet = make_rtp_packet(
                policy.payload_type,
                false,
                1,
                2,
                3,
                payload);

        packet[0] = 0x40; // V=1

        const auto parsed = parse_audio_rtp_packet_view(ByteSpan{packet.data(), packet.size()}, policy);
        assert(!parsed.has_value());
        assert(parsed.error() == Error::BadRTPVersion);
    }
}

int main() {
    parses_valid_l24_audio_rtp_packet();
    parses_valid_l16_audio_rtp_packet();
    tolerates_csrc_and_rtp_header_extension();
    rejects_payload_type_mismatch();
    rejects_payload_size_mismatch();
    rejects_short_rtp_packet();
    rejects_bad_rtp_version_before_audio_payload_policy();
    return 0;
}
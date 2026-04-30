#include "st2110/audio_frame_assembler.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

using namespace st2110;

namespace {
RtpHeaderView make_rtp(uint16_t seq, uint32_t timestamp, bool marker = false, uint8_t payload_type = 96) {
    RtpHeaderView rtp{};
    rtp.version = 2;
    rtp.padding_flag = false;
    rtp.extension_flag = false;
    rtp.csrc_count = 0;
    rtp.marker = marker;
    rtp.payload_type = payload_type;
    rtp.seq_number = seq;
    rtp.timestamp = timestamp;
    rtp.ssrc = 0x11223344;
    rtp.payload_offset = 12;
    rtp.payload_len = 0;
    return rtp;
}

AudioRtpPacketView make_packet(uint16_t seq, uint32_t timestamp, bool marker, std::vector<uint8_t> &payload,
                               uint32_t sampling_rate_hz, uint16_t channel_count, uint32_t samples_per_channel,
                               AudioPcmWireFormat wire_format) {
    RtpHeaderView rtp = make_rtp(seq, timestamp, marker);
    rtp.payload_len = payload.size();

    return AudioRtpPacketView{
        rtp,        ByteSpan{payload.data(), payload.size()}, sampling_rate_hz, channel_count, samples_per_channel,
        wire_format};
}

void assembles_l24_packet_into_interleaved_s32_block() {
    std::vector<uint8_t> payload{
        0x00, 0x00, 0x01, // sample 0, channel 0 => 1
        0xFF, 0xFF, 0xFE, // sample 0, channel 1 => -2
        0x7F, 0xFF, 0xFF, // sample 1, channel 0 => 8388607
        0x80, 0x00, 0x00  // sample 1, channel 1 => -8388608
    };

    AudioFrameAssembler assembler{};

    const auto packet = make_packet(100, 48000, false, payload, 48000, 2, 2, AudioPcmWireFormat::L24);

    auto block = assembler.push(packet);
    assert(block.has_value());

    assert(block->rtp_timestamp == 48000);
    assert(block->rtp_sequence_number == 100);
    assert(block->rtp_marker == false);
    assert(block->complete == true);

    assert(block->buffer.storage_format() == AudioSampleStorageFormat::InterleavedS32);
    assert(block->buffer.sampling_rate_hz() == 48000);
    assert(block->buffer.channel_count() == 2);
    assert(block->buffer.samples_per_channel() == 2);
    assert(block->buffer.total_sample_count() == 4);
    assert(block->buffer.sample_frame_stride() == 2);

    assert(block->buffer.sample(0, 0) == 1);
    assert(block->buffer.sample(0, 1) == -2);
    assert(block->buffer.sample(1, 0) == 8388607);
    assert(block->buffer.sample(1, 1) == -8388608);

    assert(assembler.stats().packets_used == 1);
    assert(assembler.stats().blocks_emitted == 1);
    assert(assembler.stats().packets_rejected == 0);
}

void assembles_l16_packet_into_interleaved_s32_block() {
    std::vector<uint8_t> payload{
        0x00, 0x01, // sample 0, channel 0 => 1
        0xFF, 0xFE, // sample 0, channel 1 => -2
        0x7F, 0xFF, // sample 1, channel 0 => 32767
        0x80, 0x00  // sample 1, channel 1 => -32768
    };

    AudioFrameAssembler assembler{};

    const auto packet = make_packet(101, 96000, true, payload, 48000, 2, 2, AudioPcmWireFormat::L16);

    auto block = assembler.push(packet);
    assert(block.has_value());

    assert(block->rtp_timestamp == 96000);
    assert(block->rtp_sequence_number == 101);
    assert(block->rtp_marker == true);

    assert(block->buffer.sample(0, 0) == 1);
    assert(block->buffer.sample(0, 1) == -2);
    assert(block->buffer.sample(1, 0) == 32767);
    assert(block->buffer.sample(1, 1) == -32768);
}

void does_not_assume_stereo_or_48_samples_per_packet() {
    std::vector<uint8_t> payload{
        0x00, 0x00, 0x0A, // sample 0, channel 0 => 10
        0x00, 0x00, 0x14, // sample 0, channel 1 => 20
        0x00, 0x00, 0x1E, // sample 0, channel 2 => 30
        0x00, 0x00, 0x28, // sample 1, channel 0 => 40
        0x00, 0x00, 0x32, // sample 1, channel 1 => 50
        0x00, 0x00, 0x3C, // sample 1, channel 2 => 60
        0x00, 0x00, 0x46, // sample 2, channel 0 => 70
        0x00, 0x00, 0x50, // sample 2, channel 1 => 80
        0x00, 0x00, 0x5A  // sample 2, channel 2 => 90
    };

    AudioFrameAssembler assembler{};

    const auto packet = make_packet(102, 123456, false, payload, 48000, 3, 3, AudioPcmWireFormat::L24);

    auto block = assembler.push(packet);
    assert(block.has_value());

    assert(block->buffer.channel_count() == 3);
    assert(block->buffer.samples_per_channel() == 3);
    assert(block->buffer.total_sample_count() == 9);

    assert(block->buffer.sample(0, 0) == 10);
    assert(block->buffer.sample(0, 1) == 20);
    assert(block->buffer.sample(0, 2) == 30);

    assert(block->buffer.sample(1, 0) == 40);
    assert(block->buffer.sample(1, 1) == 50);
    assert(block->buffer.sample(1, 2) == 60);

    assert(block->buffer.sample(2, 0) == 70);
    assert(block->buffer.sample(2, 1) == 80);
    assert(block->buffer.sample(2, 2) == 90);
}

void rejects_payload_size_mismatch_without_emitting_block() {
    std::vector<uint8_t> payload{
        0x00, 0x00, 0x01, 0x00, 0x00, 0x02,
        0x00, 0x00, 0x03, 0x00, 0x00 // one byte short for 2 channels * 2 samples * L24
    };

    AudioFrameAssembler assembler{};

    const auto packet = make_packet(103, 123, false, payload, 48000, 2, 2, AudioPcmWireFormat::L24);

    auto block = assembler.push(packet);
    assert(!block.has_value());
    assert(block.error() == Error::InvalidValue);

    assert(assembler.stats().packets_used == 0);
    assert(assembler.stats().blocks_emitted == 0);
    assert(assembler.stats().packets_rejected == 1);
}

void rejects_invalid_packet_shape() {
    std::vector<uint8_t> payload{0x00, 0x01};

    {
        AudioFrameAssembler assembler{};
        auto packet = make_packet(104, 123, false, payload, 48000, 0, 1, AudioPcmWireFormat::L16);

        auto block = assembler.push(packet);
        assert(!block.has_value());
        assert(block.error() == Error::InvalidValue);
    }

    {
        AudioFrameAssembler assembler{};
        auto packet = make_packet(105, 123, false, payload, 48000, 1, 0, AudioPcmWireFormat::L16);

        auto block = assembler.push(packet);
        assert(!block.has_value());
        assert(block.error() == Error::InvalidValue);
    }

    {
        AudioFrameAssembler assembler{};
        auto packet = make_packet(106, 123, false, payload, 48000, 1, 1, static_cast<AudioPcmWireFormat>(99));

        auto block = assembler.push(packet);
        assert(!block.has_value());
        assert(block.error() == Error::InvalidValue);
    }
}

void reset_clears_stats_only_and_keeps_assembler_reusable() {
    std::vector<uint8_t> bad_payload{0x00};

    AudioFrameAssembler assembler{};

    const auto bad_packet = make_packet(107, 123, false, bad_payload, 48000, 1, 1, AudioPcmWireFormat::L16);

    auto bad_block = assembler.push(bad_packet);
    assert(!bad_block.has_value());
    assert(assembler.stats().packets_rejected == 1);

    assembler.reset();
    assert(assembler.stats().packets_used == 0);
    assert(assembler.stats().packets_rejected == 0);
    assert(assembler.stats().blocks_emitted == 0);

    std::vector<uint8_t> good_payload{0x00, 0x2A};

    const auto good_packet = make_packet(108, 456, false, good_payload, 48000, 1, 1, AudioPcmWireFormat::L16);

    auto good_block = assembler.push(good_packet);
    assert(good_block.has_value());
    assert(good_block->buffer.sample(0, 0) == 42);
    assert(assembler.stats().packets_used == 1);
    assert(assembler.stats().blocks_emitted == 1);
    assert(assembler.stats().packets_rejected == 0);
}
} // namespace

int main() {
    assembles_l24_packet_into_interleaved_s32_block();
    assembles_l16_packet_into_interleaved_s32_block();
    does_not_assume_stereo_or_48_samples_per_packet();
    rejects_payload_size_mismatch_without_emitting_block();
    rejects_invalid_packet_shape();
    reset_clears_stats_only_and_keeps_assembler_reusable();
    return 0;
}
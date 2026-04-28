#include "st2110/audio_reorder_buffer.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

using namespace st2110;

namespace {
    constexpr uint8_t kPayloadType = 111;
    constexpr uint32_t kSamplingRate = 48'000;
    constexpr uint16_t kChannelCount = 2;
    constexpr uint32_t kSamplesPerPacket = 48;
    constexpr std::size_t kPayloadBytes = kSamplesPerPacket * kChannelCount * 3;

    struct PacketFixture {
        RtpHeaderView rtp{};
        std::vector<uint8_t> payload{};
        uint32_t sampling_rate_hz = kSamplingRate;
        uint16_t channel_count = kChannelCount;
        uint32_t samples_per_channel = kSamplesPerPacket;
        AudioPcmWireFormat wire_format = AudioPcmWireFormat::L24;

        [[nodiscard]] AudioRtpPacketView view() const {
            return AudioRtpPacketView{
                    rtp,
                    ByteSpan{payload.data(), payload.size()},
                    sampling_rate_hz,
                    channel_count,
                    samples_per_channel,
                    wire_format,
            };
        }
    };

    PacketFixture make_packet(
            uint16_t seq,
    uint32_t timestamp = 0,
            uint8_t fill = 0x55) {
    PacketFixture f{};
    f.payload.assign(kPayloadBytes, fill);

    f.rtp.version = 2;
    f.rtp.padding_flag = false;
    f.rtp.extension_flag = false;
    f.rtp.csrc_count = 0;
    f.rtp.marker = false;
    f.rtp.payload_type = kPayloadType;
    f.rtp.seq_number = seq;
    f.rtp.timestamp = timestamp;
    f.rtp.ssrc = 0x11223344;
    f.rtp.payload_offset = 12;
    f.rtp.payload_len = f.payload.size();

    return f;
}

void in_order_packets_pop_in_order() {
    AudioFixedWindowReorderBuffer buffer{AudioReorderBufferConfig{.window_size_packets = 8}};

    auto p10 = make_packet(10, 480, 0x10);
    auto p11 = make_packet(11, 528, 0x11);

    assert(buffer.push(p10.view()) == Error::Ok);

    auto out10 = buffer.pop_next();
    assert(out10.has_value());
    assert(out10->view().rtp.seq_number == 10);
    assert(out10->view().rtp.timestamp == 480);
    assert(out10->view().payload.size() == kPayloadBytes);
    assert(out10->view().payload[0] == 0x10);

    assert(!buffer.pop_next().has_value());

    assert(buffer.push(p11.view()) == Error::Ok);

    auto out11 = buffer.pop_next();
    assert(out11.has_value());
    assert(out11->view().rtp.seq_number == 11);
    assert(out11->view().rtp.timestamp == 528);
    assert(out11->view().payload[0] == 0x11);

    assert(!buffer.pop_next().has_value());
    assert(buffer.stats().packets_pushed == 2);
    assert(buffer.stats().packets_popped == 2);
}

void out_of_order_packets_are_reordered_within_window() {
    AudioFixedWindowReorderBuffer buffer{AudioReorderBufferConfig{.window_size_packets = 8}};

    auto p10 = make_packet(10, 480, 0x10);
    auto p12 = make_packet(12, 576, 0x12);
    auto p11 = make_packet(11, 528, 0x11);

    assert(buffer.push(p10.view()) == Error::Ok);
    assert(buffer.push(p12.view()) == Error::Ok);

    auto out10 = buffer.pop_next();
    assert(out10.has_value());
    assert(out10->view().rtp.seq_number == 10);

    assert(!buffer.pop_next().has_value());

    assert(buffer.push(p11.view()) == Error::Ok);

    auto out11 = buffer.pop_next();
    assert(out11.has_value());
    assert(out11->view().rtp.seq_number == 11);
    assert(out11->view().payload[0] == 0x11);

    auto out12 = buffer.pop_next();
    assert(out12.has_value());
    assert(out12->view().rtp.seq_number == 12);
    assert(out12->view().payload[0] == 0x12);

    assert(!buffer.pop_next().has_value());
    assert(buffer.stats().packets_pushed == 3);
    assert(buffer.stats().packets_popped == 3);
}

void duplicate_packet_is_rejected_without_overwriting_original() {
    AudioFixedWindowReorderBuffer buffer{AudioReorderBufferConfig{.window_size_packets = 8}};

    auto original = make_packet(10, 480, 0x10);
    auto duplicate = make_packet(10, 480, 0x99);

    assert(buffer.push(original.view()) == Error::Ok);
    assert(buffer.push(duplicate.view()) == Error::InvalidValue);

    auto out = buffer.pop_next();
    assert(out.has_value());
    assert(out->view().rtp.seq_number == 10);
    assert(out->view().payload[0] == 0x10);

    assert(!buffer.pop_next().has_value());
    assert(buffer.stats().duplicates == 1);
    assert(buffer.stats().packets_pushed == 1);
    assert(buffer.stats().packets_popped == 1);
}

void missing_packet_can_be_flushed_once() {
    AudioFixedWindowReorderBuffer buffer{AudioReorderBufferConfig{.window_size_packets = 8}};

    auto p10 = make_packet(10, 480, 0x10);
    auto p12 = make_packet(12, 576, 0x12);

    assert(buffer.push(p10.view()) == Error::Ok);
    assert(buffer.push(p12.view()) == Error::Ok);

    auto out10 = buffer.pop_next();
    assert(out10.has_value());
    assert(out10->view().rtp.seq_number == 10);

    assert(!buffer.pop_next().has_value());

    buffer.flush_missing_once();

    auto out12 = buffer.pop_next();
    assert(out12.has_value());
    assert(out12->view().rtp.seq_number == 12);

    assert(!buffer.pop_next().has_value());
    assert(buffer.stats().missing_packets_flushed == 1);
}

void late_packet_after_progress_is_rejected() {
    AudioFixedWindowReorderBuffer buffer{AudioReorderBufferConfig{.window_size_packets = 8}};

    auto p10 = make_packet(10, 480, 0x10);
    auto p09 = make_packet(9, 432, 0x09);

    assert(buffer.push(p10.view()) == Error::Ok);

    auto out10 = buffer.pop_next();
    assert(out10.has_value());
    assert(out10->view().rtp.seq_number == 10);

    assert(buffer.push(p09.view()) == Error::InvalidValue);
    assert(buffer.stats().late_packets == 1);
    assert(!buffer.pop_next().has_value());
}

void packet_beyond_fixed_window_is_rejected() {
    AudioFixedWindowReorderBuffer buffer{AudioReorderBufferConfig{.window_size_packets = 3}};

    auto p10 = make_packet(10, 480, 0x10);
    auto p12 = make_packet(12, 576, 0x12);
    auto p13 = make_packet(13, 624, 0x13);

    assert(buffer.push(p10.view()) == Error::Ok);
    assert(buffer.push(p13.view()) == Error::InvalidValue);
    assert(buffer.push(p12.view()) == Error::Ok);

    auto out10 = buffer.pop_next();
    assert(out10.has_value());
    assert(out10->view().rtp.seq_number == 10);

    assert(!buffer.pop_next().has_value());
    assert(buffer.stats().out_of_window == 1);
}

void reset_clears_pending_packets_and_sequence_state() {
    AudioFixedWindowReorderBuffer buffer{AudioReorderBufferConfig{.window_size_packets = 8}};

    auto p10 = make_packet(10, 480, 0x10);
    auto p12 = make_packet(12, 576, 0x12);

    assert(buffer.push(p10.view()) == Error::Ok);
    assert(buffer.push(p12.view()) == Error::Ok);
    assert(buffer.has_pending());

    buffer.reset();

    assert(!buffer.has_pending());
    assert(!buffer.pop_next().has_value());

    auto p100 = make_packet(100, 4'800, 0x64);
    assert(buffer.push(p100.view()) == Error::Ok);

    auto out100 = buffer.pop_next();
    assert(out100.has_value());
    assert(out100->view().rtp.seq_number == 100);
    assert(out100->view().payload[0] == 0x64);
}

void sequence_number_wraparound_is_supported() {
    AudioFixedWindowReorderBuffer buffer{AudioReorderBufferConfig{.window_size_packets = 8}};

    auto p65534 = make_packet(65'534, 100, 0xfe);
    auto p0 = make_packet(0, 196, 0x00);
    auto p65535 = make_packet(65'535, 148, 0xff);

    assert(buffer.push(p65534.view()) == Error::Ok);
    assert(buffer.push(p0.view()) == Error::Ok);

    auto out65534 = buffer.pop_next();
    assert(out65534.has_value());
    assert(out65534->view().rtp.seq_number == 65'534);

    assert(!buffer.pop_next().has_value());

    assert(buffer.push(p65535.view()) == Error::Ok);

    auto out65535 = buffer.pop_next();
    assert(out65535.has_value());
    assert(out65535->view().rtp.seq_number == 65'535);
    assert(out65535->view().payload[0] == 0xff);

    auto out0 = buffer.pop_next();
    assert(out0.has_value());
    assert(out0->view().rtp.seq_number == 0);
    assert(out0->view().payload[0] == 0x00);

    assert(!buffer.pop_next().has_value());
}
}

int main() {
    in_order_packets_pop_in_order();
    out_of_order_packets_are_reordered_within_window();
    duplicate_packet_is_rejected_without_overwriting_original();
    missing_packet_can_be_flushed_once();
    late_packet_after_progress_is_rejected();
    packet_beyond_fixed_window_is_rejected();
    reset_clears_pending_packets_and_sequence_state();
    sequence_number_wraparound_is_supported();

    return 0;
}
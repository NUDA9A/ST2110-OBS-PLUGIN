#include "st2110/audio_frame.hpp"
#include "st2110/rx_config.hpp"

#include <cassert>
#include <cstdint>

namespace {
void constructs_level_a_interleaved_s32_buffer_shape() {
    st2110::AudioBuffer buffer{48000, 2, 48};

    assert(buffer.storage_format() == st2110::AudioSampleStorageFormat::InterleavedS32);
    assert(buffer.sampling_rate_hz() == 48000);
    assert(buffer.channel_count() == 2);
    assert(buffer.samples_per_channel() == 48);

    assert(buffer.sample_frame_stride() == 2);
    assert(buffer.total_sample_count() == 96);
    assert(buffer.size_bytes() == 96 * sizeof(std::int32_t));

    assert(buffer.samples() != nullptr);

    const auto view = buffer.view();

    assert(view.storage_format == st2110::AudioSampleStorageFormat::InterleavedS32);
    assert(view.sampling_rate_hz == 48000);
    assert(view.channel_count == 2);
    assert(view.samples_per_channel == 48);
    assert(view.samples == buffer.samples());
    assert(view.total_sample_count == buffer.total_sample_count());
    assert(view.sample_frame_stride == buffer.sample_frame_stride());
    assert(view.size_bytes == buffer.size_bytes());
    assert(view.timestamp_ns == 0);
}

void provides_interleaved_mutable_sample_access() {
    st2110::AudioBuffer buffer{48000, 3, 4};

    buffer.sample(0, 0) = 10;
    buffer.sample(0, 1) = 20;
    buffer.sample(0, 2) = 30;

    buffer.sample(1, 0) = 40;
    buffer.sample(3, 2) = 99;

    assert(buffer.samples()[0] == 10);
    assert(buffer.samples()[1] == 20);
    assert(buffer.samples()[2] == 30);

    assert(buffer.samples()[3] == 40);
    assert(buffer.samples()[11] == 99);

    const st2110::AudioBuffer &const_buffer = buffer;
    assert(const_buffer.sample(0, 0) == 10);
    assert(const_buffer.sample(3, 2) == 99);
}

void view_carries_timestamp_without_copying_storage() {
    st2110::AudioBuffer buffer{48000, 2, 48};

    buffer.sample(0, 0) = 123;
    buffer.sample(47, 1) = 456;

    const auto view = buffer.view(123456789ULL);

    assert(view.timestamp_ns == 123456789ULL);
    assert(view.samples == buffer.samples());
    assert(view.samples[0] == 123);
    assert(view.samples[95] == 456);
}

void rx_audio_config_constructor_uses_samples_per_packet_as_block_length() {
    st2110::RxAudioConfig cfg{};
    cfg.sampling_rate_hz = 48000;
    cfg.packet_time_us = 1000;
    cfg.samples_per_packet = 48;
    cfg.channel_count = 8;
    cfg.udp_port = 5004;
    cfg.payload_type = 101;
    cfg.dest_ip = "239.10.10.10";

    st2110::AudioBuffer buffer{cfg};

    assert(buffer.sampling_rate_hz() == 48000);
    assert(buffer.channel_count() == 8);
    assert(buffer.samples_per_channel() == 48);
    assert(buffer.sample_frame_stride() == 8);
    assert(buffer.total_sample_count() == 384);
    assert(buffer.size_bytes() == 384 * sizeof(std::int32_t));
}

void storage_contract_is_not_hardcoded_to_level_a_packet_cadence() {
    st2110::AudioBuffer buffer{96000, 4, 96};

    assert(buffer.sampling_rate_hz() == 96000);
    assert(buffer.channel_count() == 4);
    assert(buffer.samples_per_channel() == 96);
    assert(buffer.sample_frame_stride() == 4);
    assert(buffer.total_sample_count() == 384);
}
} // namespace

int main() {
    constructs_level_a_interleaved_s32_buffer_shape();
    provides_interleaved_mutable_sample_access();
    view_carries_timestamp_without_copying_storage();
    rx_audio_config_constructor_uses_samples_per_packet_as_block_length();
    storage_contract_is_not_hardcoded_to_level_a_packet_cadence();

    return 0;
}
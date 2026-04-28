#include "st2110/audio_stats.hpp"

#include <cassert>
#include <cstdint>

using namespace st2110;

static void default_stats_are_zero() {
    const AudioReceiveStats stats{};

    assert(stats.packets_ok == 0);
    assert(stats.packets_lost == 0);
    assert(stats.packets_rejected == 0);

    assert(stats.blocks_ok == 0);
    assert(stats.blocks_partial == 0);
    assert(stats.blocks_dropped == 0);
}

static void packet_counters_are_recorded_independently_from_block_counters() {
    AudioReceiveStats stats{};

    record_audio_packet_ok(stats);
    record_audio_packet_lost(stats);
    record_audio_packet_lost(stats);
    record_audio_packet_rejected(stats);

    assert(stats.packets_ok == 1);
    assert(stats.packets_lost == 2);
    assert(stats.packets_rejected == 1);

    assert(stats.blocks_ok == 0);
    assert(stats.blocks_partial == 0);
    assert(stats.blocks_dropped == 0);
}

static void block_status_routes_to_the_matching_counter() {
    AudioReceiveStats stats{};

    assert(record_audio_block_result(
            stats,
            AudioBlockCompletionStatus::Complete) == Error::Ok);
    assert(record_audio_block_result(
            stats,
            AudioBlockCompletionStatus::Partial) == Error::Ok);
    assert(record_audio_block_result(
            stats,
            AudioBlockCompletionStatus::Partial) == Error::Ok);
    assert(record_audio_block_result(
            stats,
            AudioBlockCompletionStatus::Dropped) == Error::Ok);

    assert(stats.blocks_ok == 1);
    assert(stats.blocks_partial == 2);
    assert(stats.blocks_dropped == 1);

    assert(stats.packets_ok == 0);
    assert(stats.packets_lost == 0);
    assert(stats.packets_rejected == 0);
}

static void invalid_block_status_is_rejected_without_mutation() {
    AudioReceiveStats stats{};

    record_audio_packet_ok(stats);
    assert(record_audio_block_result(
            stats,
            AudioBlockCompletionStatus::Complete) == Error::Ok);

    const AudioReceiveStats before = stats;

    const auto invalid_status =
            static_cast<AudioBlockCompletionStatus>(255);

    assert(validate_audio_block_completion_status(invalid_status) ==
           Error::InvalidValue);

    assert(record_audio_block_result(stats, invalid_status) ==
           Error::InvalidValue);

    assert(stats.packets_ok == before.packets_ok);
    assert(stats.packets_lost == before.packets_lost);
    assert(stats.packets_rejected == before.packets_rejected);

    assert(stats.blocks_ok == before.blocks_ok);
    assert(stats.blocks_partial == before.blocks_partial);
    assert(stats.blocks_dropped == before.blocks_dropped);
}

static void reset_clears_all_audio_receive_stats() {
    AudioReceiveStats stats{};

    record_audio_packet_ok(stats);
    record_audio_packet_lost(stats);
    record_audio_packet_rejected(stats);

    assert(record_audio_block_result(
            stats,
            AudioBlockCompletionStatus::Complete) == Error::Ok);
    assert(record_audio_block_result(
            stats,
            AudioBlockCompletionStatus::Partial) == Error::Ok);
    assert(record_audio_block_result(
            stats,
            AudioBlockCompletionStatus::Dropped) == Error::Ok);

    reset_audio_receive_stats(stats);

    assert(stats.packets_ok == 0);
    assert(stats.packets_lost == 0);
    assert(stats.packets_rejected == 0);

    assert(stats.blocks_ok == 0);
    assert(stats.blocks_partial == 0);
    assert(stats.blocks_dropped == 0);
}

int main() {
    default_stats_are_zero();
    packet_counters_are_recorded_independently_from_block_counters();
    block_status_routes_to_the_matching_counter();
    invalid_block_status_is_rejected_without_mutation();
    reset_clears_all_audio_receive_stats();

    return 0;
}
#include "st2110/audio_timestamp_mapping.hpp"

#include <cassert>
#include <cstdint>
#include <limits>

namespace {
    using namespace st2110;

    void validates_mapper_config() {
        {
            const AudioRtpTimestampMapperConfig cfg{
                    .rtp_clock_rate = 48'000,
                    .anchor_rtp_timestamp = 1000,
                    .anchor_timestamp_ns = 1234
            };

            assert(validate_audio_rtp_timestamp_mapper_config(cfg) == Error::Ok);
        }

        {
            const AudioRtpTimestampMapperConfig cfg{
                    .rtp_clock_rate = 0,
                    .anchor_rtp_timestamp = 1000,
                    .anchor_timestamp_ns = 1234
            };

            assert(validate_audio_rtp_timestamp_mapper_config(cfg) == Error::InvalidValue);
        }
    }

    void converts_audio_rtp_ticks_using_configured_clock_rate() {
        {
            const auto one_ms_48k = audio_rtp_ticks_to_timestamp_ns(48, 48'000);
            assert(one_ms_48k.has_value());
            assert(*one_ms_48k == 1'000'000);
        }

        {
            const auto one_ms_96k = audio_rtp_ticks_to_timestamp_ns(96, 96'000);
            assert(one_ms_96k.has_value());
            assert(*one_ms_96k == 1'000'000);
        }

        {
            const auto invalid_rate = audio_rtp_ticks_to_timestamp_ns(48, 0);
            assert(!invalid_rate.has_value());
            assert(invalid_rate.error() == Error::InvalidValue);
        }
    }

    void rejects_tick_to_ns_overflow() {
        const auto too_large = audio_rtp_ticks_to_timestamp_ns(
                std::numeric_limits<uint64_t>::max(),
                48'000);

        assert(!too_large.has_value());
        assert(too_large.error() == Error::InvalidValue);
    }

    void computes_forward_rtp_timestamp_delta_with_wraparound() {
        {
            const auto delta = forward_audio_rtp_timestamp_delta(1000, 1048);
            assert(delta.has_value());
            assert(*delta == 48);
        }

        {
            const auto delta = forward_audio_rtp_timestamp_delta(0xfffffff0u, 0x00000020u);
            assert(delta.has_value());
            assert(*delta == 48);
        }

        {
            const auto ambiguous = forward_audio_rtp_timestamp_delta(0, 0x80000000u);
            assert(!ambiguous.has_value());
            assert(ambiguous.error() == Error::InvalidValue);
        }

        {
            const auto backward = forward_audio_rtp_timestamp_delta(1000, 999);
            assert(!backward.has_value());
            assert(backward.error() == Error::InvalidValue);
        }
    }

    void maps_audio_rtp_timestamps_to_internal_ns() {
        AudioRtpTimestampMapper mapper({
                                               .rtp_clock_rate = 48'000,
                                               .anchor_rtp_timestamp = 1000,
                                               .anchor_timestamp_ns = 10'000'000
                                       });

        {
            const auto ts = mapper.map(1000);
            assert(ts.has_value());
            assert(*ts == 10'000'000);
        }

        {
            const auto ts = mapper.map(1048);
            assert(ts.has_value());
            assert(*ts == 11'000'000);
        }

        {
            const auto ts = mapper.map(1096);
            assert(ts.has_value());
            assert(*ts == 12'000'000);
        }
    }

    void maps_audio_rtp_timestamps_across_wraparound() {
        AudioRtpTimestampMapper mapper({
                                               .rtp_clock_rate = 48'000,
                                               .anchor_rtp_timestamp = 0xfffffff0u,
                                               .anchor_timestamp_ns = 5'000
                                       });

        {
            const auto ts = mapper.map(0xfffffff0u);
            assert(ts.has_value());
            assert(*ts == 5'000);
        }

        {
            const auto ts = mapper.map(0x00000020u);
            assert(ts.has_value());
            assert(*ts == 1'005'000);
        }
    }

    void rejects_backward_or_invalid_mapping() {
        {
            AudioRtpTimestampMapper mapper({
                                                   .rtp_clock_rate = 48'000,
                                                   .anchor_rtp_timestamp = 1000,
                                                   .anchor_timestamp_ns = 0
                                           });

            const auto first = mapper.map(1000);
            assert(first.has_value());

            const auto second = mapper.map(1048);
            assert(second.has_value());

            const auto backward = mapper.map(1047);
            assert(!backward.has_value());
            assert(backward.error() == Error::InvalidValue);
        }

        {
            AudioRtpTimestampMapper mapper({
                                                   .rtp_clock_rate = 0,
                                                   .anchor_rtp_timestamp = 1000,
                                                   .anchor_timestamp_ns = 0
                                           });

            const auto invalid = mapper.map(1000);
            assert(!invalid.has_value());
            assert(invalid.error() == Error::InvalidValue);
        }
    }

    void rejects_mapping_timestamp_overflow() {
        AudioRtpTimestampMapper mapper({
                                               .rtp_clock_rate = 48'000,
                                               .anchor_rtp_timestamp = 0,
                                               .anchor_timestamp_ns = std::numeric_limits<TimestampNs>::max() - 10
                                       });

        const auto overflow = mapper.map(1);
        assert(!overflow.has_value());
        assert(overflow.error() == Error::InvalidValue);
    }

    void reset_reanchors_mapper() {
        AudioRtpTimestampMapper mapper({
                                               .rtp_clock_rate = 48'000,
                                               .anchor_rtp_timestamp = 1000,
                                               .anchor_timestamp_ns = 10'000'000
                                       });

        const auto first = mapper.map(1048);
        assert(first.has_value());
        assert(*first == 11'000'000);

        const Error reset_error = mapper.reset({
                                                       .rtp_clock_rate = 96'000,
                                                       .anchor_rtp_timestamp = 10,
                                                       .anchor_timestamp_ns = 20'000'000
                                               });
        assert(reset_error == Error::Ok);

        const auto after_reset = mapper.map(106);
        assert(after_reset.has_value());
        assert(*after_reset == 21'000'000);
    }

    void computes_receiver_playout_timing_decision() {
        const AudioReceiverPlayoutTimingConfig cfg{
                .playout_delay_ns = 250'000
        };

        assert(validate_audio_receiver_playout_timing_config(cfg) == Error::Ok);

        const auto decision = audio_receiver_playout_timing_decision(1'000'000, cfg);
        assert(decision.has_value());
        assert(decision->media_timestamp_ns == 1'000'000);
        assert(decision->playout_timestamp_ns == 1'250'000);
    }

    void rejects_receiver_playout_timing_overflow() {
        const AudioReceiverPlayoutTimingConfig cfg{
                .playout_delay_ns = 100
        };

        const auto decision = audio_receiver_playout_timing_decision(
                std::numeric_limits<TimestampNs>::max() - 50,
                cfg);

        assert(!decision.has_value());
        assert(decision.error() == Error::InvalidValue);
    }

    void computes_audio_block_timing_without_coupling_to_assembler() {
        const AudioReceiverPlayoutTimingConfig cfg{
                .playout_delay_ns = 500'000
        };

        const auto timing = audio_block_timing(
                12345,
                2'000'000,
                cfg);

        assert(timing.has_value());
        assert(timing->rtp_timestamp == 12345);
        assert(timing->media_timestamp_ns == 2'000'000);
        assert(timing->playout_timestamp_ns == 2'500'000);
    }
}

int main() {
    validates_mapper_config();
    converts_audio_rtp_ticks_using_configured_clock_rate();
    rejects_tick_to_ns_overflow();
    computes_forward_rtp_timestamp_delta_with_wraparound();
    maps_audio_rtp_timestamps_to_internal_ns();
    maps_audio_rtp_timestamps_across_wraparound();
    rejects_backward_or_invalid_mapping();
    rejects_mapping_timestamp_overflow();
    reset_reanchors_mapper();
    computes_receiver_playout_timing_decision();
    rejects_receiver_playout_timing_overflow();
    computes_audio_block_timing_without_coupling_to_assembler();

    return 0;
}
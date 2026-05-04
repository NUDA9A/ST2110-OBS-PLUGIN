#include <cassert>
#include <type_traits>
#include <utility>

#include <st2110/audio_reorder_buffer.hpp>
#include <st2110/receive_reorder_tolerance_policy.hpp>
#include <st2110/video_reorder_policy.hpp>

static_assert(std::is_enum_v<st2110::ReceiveReorderGapPolicy>);
static_assert(std::is_same_v<decltype(std::declval<st2110::ReceiveReorderTolerancePolicy>().gap_policy),
                             st2110::ReceiveReorderGapPolicy>);
static_assert(std::is_same_v<decltype(std::declval<st2110::VideoReorderBufferConfig>().reorder_tolerance_policy),
                             st2110::ReceiveReorderTolerancePolicy>);
static_assert(std::is_same_v<decltype(std::declval<st2110::AudioReorderBufferConfig>().reorder_tolerance_policy),
                             st2110::ReceiveReorderTolerancePolicy>);

namespace {

void receive_reorder_gap_policy_validation_accepts_known_values() {
    assert(st2110::validate_receive_reorder_gap_policy(st2110::ReceiveReorderGapPolicy::WaitForMissing) ==
           st2110::Error::Ok);
    assert(st2110::validate_receive_reorder_gap_policy(st2110::ReceiveReorderGapPolicy::FlushGapOnce) ==
           st2110::Error::Ok);
}

void receive_reorder_gap_policy_validation_rejects_unknown_value() {
    const auto invalid_policy = static_cast<st2110::ReceiveReorderGapPolicy>(255);
    assert(st2110::validate_receive_reorder_gap_policy(invalid_policy) == st2110::Error::InvalidValue);
}

void receive_reorder_tolerance_policy_default_is_explicit_wait_for_missing() {
    const st2110::ReceiveReorderTolerancePolicy policy{};

    assert(policy.gap_policy == st2110::ReceiveReorderGapPolicy::WaitForMissing);
    assert(st2110::validate_receive_reorder_tolerance_policy(policy) == st2110::Error::Ok);
    assert(!st2110::receive_reorder_policy_allows_gap_flush_once(policy));
}

void receive_reorder_tolerance_policy_helper_distinguishes_named_policies() {
    const st2110::ReceiveReorderTolerancePolicy wait_policy{
        .gap_policy = st2110::ReceiveReorderGapPolicy::WaitForMissing,
    };
    const st2110::ReceiveReorderTolerancePolicy flush_policy{
        .gap_policy = st2110::ReceiveReorderGapPolicy::FlushGapOnce,
    };

    assert(st2110::validate_receive_reorder_tolerance_policy(wait_policy) == st2110::Error::Ok);
    assert(st2110::validate_receive_reorder_tolerance_policy(flush_policy) == st2110::Error::Ok);

    assert(!st2110::receive_reorder_policy_allows_gap_flush_once(wait_policy));
    assert(st2110::receive_reorder_policy_allows_gap_flush_once(flush_policy));
}

void receive_reorder_tolerance_policy_validation_rejects_unknown_gap_policy() {
    const st2110::ReceiveReorderTolerancePolicy invalid_policy{
        .gap_policy = static_cast<st2110::ReceiveReorderGapPolicy>(255),
    };

    assert(st2110::validate_receive_reorder_tolerance_policy(invalid_policy) == st2110::Error::InvalidValue);
}

void video_reorder_buffer_config_validation_accepts_known_policies() {
    const st2110::VideoReorderBufferConfig wait_cfg{
        .window_size_packets = 32,
        .reorder_tolerance_policy =
            st2110::ReceiveReorderTolerancePolicy{
                .gap_policy = st2110::ReceiveReorderGapPolicy::WaitForMissing,
            },
    };

    const st2110::VideoReorderBufferConfig flush_cfg{
        .window_size_packets = 32,
        .reorder_tolerance_policy =
            st2110::ReceiveReorderTolerancePolicy{
                .gap_policy = st2110::ReceiveReorderGapPolicy::FlushGapOnce,
            },
    };

    assert(st2110::validate_video_reorder_buffer_config(wait_cfg) == st2110::Error::Ok);
    assert(st2110::validate_video_reorder_buffer_config(flush_cfg) == st2110::Error::Ok);
}

void video_reorder_buffer_config_validation_rejects_zero_window_and_invalid_policy() {
    const st2110::VideoReorderBufferConfig zero_window_cfg{
        .window_size_packets = 0,
        .reorder_tolerance_policy =
            st2110::ReceiveReorderTolerancePolicy{
                .gap_policy = st2110::ReceiveReorderGapPolicy::WaitForMissing,
            },
    };

    const st2110::VideoReorderBufferConfig invalid_policy_cfg{
        .window_size_packets = 32,
        .reorder_tolerance_policy =
            st2110::ReceiveReorderTolerancePolicy{
                .gap_policy = static_cast<st2110::ReceiveReorderGapPolicy>(255),
            },
    };

    assert(st2110::validate_video_reorder_buffer_config(zero_window_cfg) == st2110::Error::InvalidValue);
    assert(st2110::validate_video_reorder_buffer_config(invalid_policy_cfg) == st2110::Error::InvalidValue);
}

void audio_reorder_buffer_config_validation_accepts_known_policies() {
    const st2110::AudioReorderBufferConfig wait_cfg{
        .window_size_packets = 64,
        .reorder_tolerance_policy =
            st2110::ReceiveReorderTolerancePolicy{
                .gap_policy = st2110::ReceiveReorderGapPolicy::WaitForMissing,
            },
    };

    const st2110::AudioReorderBufferConfig flush_cfg{
        .window_size_packets = 64,
        .reorder_tolerance_policy =
            st2110::ReceiveReorderTolerancePolicy{
                .gap_policy = st2110::ReceiveReorderGapPolicy::FlushGapOnce,
            },
    };

    assert(st2110::validate_audio_reorder_buffer_config(wait_cfg) == st2110::Error::Ok);
    assert(st2110::validate_audio_reorder_buffer_config(flush_cfg) == st2110::Error::Ok);
}

void audio_reorder_buffer_config_validation_rejects_zero_window_and_invalid_policy() {
    const st2110::AudioReorderBufferConfig zero_window_cfg{
        .window_size_packets = 0,
        .reorder_tolerance_policy =
            st2110::ReceiveReorderTolerancePolicy{
                .gap_policy = st2110::ReceiveReorderGapPolicy::WaitForMissing,
            },
    };

    const st2110::AudioReorderBufferConfig invalid_policy_cfg{
        .window_size_packets = 64,
        .reorder_tolerance_policy =
            st2110::ReceiveReorderTolerancePolicy{
                .gap_policy = static_cast<st2110::ReceiveReorderGapPolicy>(255),
            },
    };

    assert(st2110::validate_audio_reorder_buffer_config(zero_window_cfg) == st2110::Error::InvalidValue);
    assert(st2110::validate_audio_reorder_buffer_config(invalid_policy_cfg) == st2110::Error::InvalidValue);
}

} // namespace

int main() {
    receive_reorder_gap_policy_validation_accepts_known_values();
    receive_reorder_gap_policy_validation_rejects_unknown_value();
    receive_reorder_tolerance_policy_default_is_explicit_wait_for_missing();
    receive_reorder_tolerance_policy_helper_distinguishes_named_policies();
    receive_reorder_tolerance_policy_validation_rejects_unknown_gap_policy();
    video_reorder_buffer_config_validation_accepts_known_policies();
    video_reorder_buffer_config_validation_rejects_zero_window_and_invalid_policy();
    audio_reorder_buffer_config_validation_accepts_known_policies();
    audio_reorder_buffer_config_validation_rejects_zero_window_and_invalid_policy();
    return 0;
}
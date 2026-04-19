#include <cassert>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <vector>

#include <st2110/depacketizer.hpp>

static_assert(std::is_same_v<
        decltype(std::declval<st2110::Depacketizer&>().push(std::declval<const st2110::PacketView&>())),
        std::vector<st2110::AssembledVideoFrame>>);

static_assert(std::is_same_v<
        decltype(std::declval<const st2110::Depacketizer&>().current_rtp_timestamp()),
        std::optional<uint32_t>>);

static void test_config_default_partial_policy_is_emit_with_flag() {
    st2110::DepacketizerConfig cfg{};
    assert(cfg.partial_frame_policy == st2110::PartialFramePolicy::EmitWithFlag);
}

static void test_construct_depacketizer_and_initial_state() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 1920;
    cfg.height = 1080;
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.partial_frame_policy = st2110::PartialFramePolicy::Drop;

    st2110::Depacketizer dep(cfg);

    assert(!dep.has_frame_in_progress());
    assert(!dep.current_rtp_timestamp().has_value());
}

static void test_reset_preserves_empty_state() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 1280;
    cfg.height = 720;
    cfg.format = st2110::PixelFormat::UYVY;

    st2110::Depacketizer dep(cfg);
    dep.reset();

    assert(!dep.has_frame_in_progress());
    assert(!dep.current_rtp_timestamp().has_value());
}

int main() {
    test_config_default_partial_policy_is_emit_with_flag();
    test_construct_depacketizer_and_initial_state();
    test_reset_preserves_empty_state();
    return 0;
}
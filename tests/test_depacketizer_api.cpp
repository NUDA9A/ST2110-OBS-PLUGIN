#include <cassert>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <vector>

#include <st2110/depacketizer.hpp>

static_assert(
    std::is_same_v<decltype(std::declval<st2110::Depacketizer &>().push(std::declval<const st2110::PacketView &>())),
                   std::vector<st2110::AssembledVideoUnit>>);

static_assert(std::is_same_v<decltype(std::declval<const st2110::Depacketizer &>().current_unit_rtp_timestamp()),
                             std::optional<uint32_t>>);

static_assert(std::is_same_v<decltype(std::declval<const st2110::Depacketizer &>().assembly_unit_kind()),
                             st2110::VideoAssemblyUnitKind>);

static void test_config_default_partial_policy_is_emit_with_flag() {
    st2110::DepacketizerConfig cfg{};
    assert(cfg.partial_frame_policy == st2110::PartialFramePolicy::EmitWithFlag);
}

static void test_config_default_scan_mode_is_progressive() {
    st2110::DepacketizerConfig cfg{};
    assert(cfg.scan_mode == st2110::VideoScanMode::Progressive);
}

static void test_construct_depacketizer_and_initial_state() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 1920;
    cfg.height = 1080;
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.partial_frame_policy = st2110::PartialFramePolicy::Drop;

    st2110::Depacketizer dep(cfg);

    assert(!dep.has_unit_in_progress());
    assert(!dep.current_unit_rtp_timestamp().has_value());
    assert(dep.assembly_unit_kind() == st2110::VideoAssemblyUnitKind::Frame);
}

static void test_reset_preserves_empty_state() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 1280;
    cfg.height = 720;
    cfg.format = st2110::PixelFormat::UYVY;

    st2110::Depacketizer dep(cfg);
    dep.reset();

    assert(!dep.has_unit_in_progress());
    assert(!dep.current_unit_rtp_timestamp().has_value());
}

int main() {
    test_config_default_partial_policy_is_emit_with_flag();
    test_config_default_scan_mode_is_progressive();
    test_construct_depacketizer_and_initial_state();
    test_reset_preserves_empty_state();
    return 0;
}
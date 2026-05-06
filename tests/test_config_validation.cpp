#include <cassert>
#include <cstdint>
#include <type_traits>
#include <utility>

#include <st2110/config_validation.hpp>
#include <st2110/error.hpp>
#include <st2110/pixel_format.hpp>
#include <st2110/rx_config.hpp>

static_assert(std::is_same_v<decltype(st2110::config_validation::is_non_empty("x")), bool>);

static_assert(
    std::is_same_v<decltype(st2110::config_validation::is_dynamic_rtp_payload_type(static_cast<uint8_t>(96))), bool>);

static_assert(std::is_same_v<decltype(st2110::validate_rx_video_config(std::declval<const st2110::RxVideoConfig &>())),
                             st2110::Error>);

static_assert(std::is_same_v<decltype(st2110::validate_rx_video_config_against_runtime_support(
                                 std::declval<const st2110::RxVideoConfig &>(),
                                 std::declval<const st2110::VideoRuntimeSupportPolicy &>())),
                             st2110::Error>);

static void test_non_empty_helper() {
    assert(!st2110::config_validation::is_non_empty(""));
    assert(st2110::config_validation::is_non_empty("239.0.0.1"));
}

static void test_dynamic_payload_type_helper() {
    assert(!st2110::config_validation::is_dynamic_rtp_payload_type(0));
    assert(!st2110::config_validation::is_dynamic_rtp_payload_type(95));
    assert(st2110::config_validation::is_dynamic_rtp_payload_type(96));
    assert(st2110::config_validation::is_dynamic_rtp_payload_type(127));
}

static void test_frame_rate_validation() {
    assert(st2110::config_validation::validate_frame_rate(25, 1) == st2110::Error::Ok);
    assert(st2110::config_validation::validate_frame_rate(30000, 1001) == st2110::Error::Ok);
    assert(st2110::config_validation::validate_frame_rate(0, 1) == st2110::Error::InvalidValue);
    assert(st2110::config_validation::validate_frame_rate(25, 0) == st2110::Error::InvalidValue);
}

static void test_video_format_constraints_for_uyvy() {
    assert(st2110::config_validation::validate_video_format_constraints(st2110::PixelFormat::UYVY, 1920, 1080) ==
           st2110::Error::Ok);

    assert(st2110::config_validation::validate_video_format_constraints(st2110::PixelFormat::UYVY, 1919, 1080) ==
           st2110::Error::InvalidValue);

    assert(st2110::config_validation::validate_video_format_constraints(st2110::PixelFormat::UYVY, 1920, 0) ==
           st2110::Error::InvalidValue);
}

static void test_unknown_format_is_unsupported() {
    const auto unknown = static_cast<st2110::PixelFormat>(999);
    assert(st2110::config_validation::validate_video_format_constraints(unknown, 1920, 1080) ==
           st2110::Error::Unsupported);
}

static st2110::RxVideoConfig make_valid_cfg() {
    st2110::RxVideoConfig cfg{};
    cfg.width = 1920;
    cfg.height = 1080;
    cfg.fps_num = 30000;
    cfg.fps_den = 1001;
    cfg.udp_port = 5004;
    cfg.payload_type = 96;
    cfg.local_ip = "0.0.0.0";
    cfg.dest_ip = "239.0.0.1";
    cfg.format = st2110::PixelFormat::UYVY;
    return cfg;
}

static void test_validate_rx_video_config_ok() {
    const st2110::RxVideoConfig cfg = make_valid_cfg();
    assert(st2110::validate_rx_video_config(cfg) == st2110::Error::Ok);
    assert(cfg.is_valid());

    assert(st2110::validate_rx_video_config_against_runtime_support(
               cfg, st2110::default_video_rx_runtime_support_policy()) == st2110::Error::Ok);
}

static void test_validate_rx_video_config_allows_odd_uyvy_width_structurally() {
    st2110::RxVideoConfig cfg = make_valid_cfg();
    cfg.width = 1919;

    /*
     * validate_rx_video_config() is now structural/common-model validation.
     * Project storage constraints are checked at the explicit runtime-support boundary.
     */
    assert(st2110::validate_rx_video_config(cfg) == st2110::Error::Ok);
    assert(cfg.is_valid());

    assert(st2110::validate_rx_video_config_against_runtime_support(
               cfg, st2110::default_video_rx_runtime_support_policy()) == st2110::Error::InvalidValue);
}

static void test_validate_rx_video_config_rejects_non_dynamic_payload_type() {
    st2110::RxVideoConfig cfg = make_valid_cfg();
    cfg.payload_type = 95;

    assert(st2110::validate_rx_video_config(cfg) == st2110::Error::InvalidValue);
    assert(!cfg.is_valid());
}

static void test_validate_rx_video_config_rejects_zero_port() {
    st2110::RxVideoConfig cfg = make_valid_cfg();
    cfg.udp_port = 0;

    assert(st2110::validate_rx_video_config(cfg) == st2110::Error::InvalidValue);
    assert(!cfg.is_valid());
}

static void test_validate_rx_video_config_rejects_empty_dest_ip() {
    st2110::RxVideoConfig cfg = make_valid_cfg();
    cfg.dest_ip.clear();

    assert(st2110::validate_rx_video_config(cfg) == st2110::Error::InvalidValue);
    assert(!cfg.is_valid());
}

int main() {
    test_non_empty_helper();
    test_dynamic_payload_type_helper();
    test_frame_rate_validation();
    test_video_format_constraints_for_uyvy();
    test_unknown_format_is_unsupported();
    test_validate_rx_video_config_ok();
    test_validate_rx_video_config_allows_odd_uyvy_width_structurally();
    test_validate_rx_video_config_rejects_non_dynamic_payload_type();
    test_validate_rx_video_config_rejects_zero_port();
    test_validate_rx_video_config_rejects_empty_dest_ip();
    return 0;
}
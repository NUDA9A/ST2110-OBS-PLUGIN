#ifndef ST2110_OBS_PLUGIN_RX_CONFIG_HPP
#define ST2110_OBS_PLUGIN_RX_CONFIG_HPP

#include <string>
#include <cstdint>
#include "pixel_format.hpp"
#include "config_validation.hpp"

namespace st2110 {
    struct RxVideoConfig;
    [[nodiscard]] Error validate_rx_video_config(const RxVideoConfig& cfg);

    struct RxVideoConfig {
        uint32_t width;
        uint32_t height;
        uint32_t fps_num;
        uint32_t fps_den;
        uint16_t udp_port;
        uint8_t payload_type;
        std::string local_ip;
        std::string dest_ip;
        PixelFormat format;

        [[nodiscard]] bool is_valid() const {
            return (validate_rx_video_config(*this) == Error::Ok);
        }
    };

    [[nodiscard]] inline Error validate_rx_video_config(const RxVideoConfig& cfg) {
        Error err = config_validation::validate_video_format_constraints(cfg.format, cfg.width, cfg.height);
        if (err != Error::Ok) {
            return err;
        }

        err = config_validation::validate_frame_rate(cfg.fps_num, cfg.fps_den);
        if (err != Error::Ok) {
            return err;
        }

        err = config_validation::validate_udp_port(cfg.udp_port);
        if (err != Error::Ok) {
            return err;
        }

        if (!config_validation::is_dynamic_rtp_payload_type(cfg.payload_type)) {
            return Error::InvalidValue;
        }

        if (!config_validation::is_non_empty(cfg.dest_ip)) {
            return Error::InvalidValue;
        }

        return Error::Ok;
    }
}

#endif //ST2110_OBS_PLUGIN_RX_CONFIG_HPP

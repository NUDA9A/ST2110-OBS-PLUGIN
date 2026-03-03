#ifndef ST2110_OBS_PLUGIN_RX_CONFIG_HPP
#define ST2110_OBS_PLUGIN_RX_CONFIG_HPP

#include <string>
#include <cstdint>

namespace st2110 {
    struct RxVideoConfig {
        uint32_t width;
        uint32_t height;
        uint32_t fps_num;
        uint32_t fps_den;
        uint16_t udp_port;
        uint8_t payload_type;
        std::string local_ip;
        std::string dest_ip;

        enum class PixelFormat {
            UYVY,
        };

        PixelFormat format;

        bool is_valid() const {
            if (width == 0 || height == 0) {
                return false;
            }
            if (fps_den == 0 || fps_num == 0) {
                return false;
            }
            if (udp_port == 0) {
                return false;
            }
            if (payload_type > 127) {
                return false;
            }
            if (dest_ip == "") {
                return false;
            }
            return true;
        }
    };
}

#endif //ST2110_OBS_PLUGIN_RX_CONFIG_HPP

#include <cassert>
#include <string>

#include <st2110/rx_config.hpp>

int main() {
    st2110::RxVideoConfig cfg{};
    assert(!cfg.is_valid());

    cfg.width = 1280;
    cfg.height = 720;
    cfg.fps_num = 30;
    cfg.fps_den = 1;
    cfg.udp_port = 20000;
    cfg.payload_type = 112;
    cfg.dest_ip = "239.1.1.1";
    cfg.local_ip = "0.0.0.0";
    cfg.format = st2110::RxVideoConfig::PixelFormat::UYVY;

    assert(cfg.is_valid());

    cfg.udp_port = 0;
    assert(!cfg.is_valid());

    return 0;
}
#include <cassert>
#include <cstdint>

#include <st2110/backend.hpp>
#include <st2110/video_frame.hpp>
#include <st2110/rx_config.hpp>

struct FakeSink : st2110::IFrameSink {
    int frames = 0;
    uint64_t last_ts = 0;

    void on_frame(const st2110::FrameView& f) override {
        frames++;
        last_ts = f.timestamp_ns;
        assert(f.width > 0);
        assert(f.height > 0);
        assert(f.data[0] != nullptr);
    }
};

struct FakeBackend : st2110::IRxVideoBackend {
    const char* backend_name() const override { return "fake"; }

    void start(const st2110::RxVideoConfig& cfg, st2110::IFrameSink& sink) override {
        st2110::VideoFrame frame(cfg.width, cfg.height, cfg.format);
        sink.on_frame(frame.view(42));
    }

    void stop() override {}
};

int main() {
    st2110::RxVideoConfig cfg{};
    cfg.width = 1280;
    cfg.height = 720;
    cfg.fps_num = 30;
    cfg.fps_den = 1;
    cfg.udp_port = 20000;
    cfg.payload_type = 112;
    cfg.dest_ip = "239.1.1.1";
    cfg.local_ip = "0.0.0.0";
    cfg.format = st2110::PixelFormat::UYVY;

    FakeSink sink;
    FakeBackend be;

    be.start(cfg, sink);
    assert(sink.frames == 1);
    assert(sink.last_ts == 42);

    be.stop();
    return 0;
}
#include <cassert>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <utility>

#include <st2110/backend.hpp>
#include <st2110/rx_config.hpp>
#include <st2110/video_frame.hpp>

static_assert(std::is_abstract_v<st2110::IRxBackend>);
static_assert(std::is_abstract_v<st2110::IRxVideoBackend>);
static_assert(std::is_base_of_v<st2110::IRxBackend, st2110::IRxVideoBackend>);
static_assert(std::is_abstract_v<st2110::IVideoFrameSink>);

static_assert(std::is_same_v<
        decltype(std::declval<const st2110::IRxBackend&>().backend_name()),
        const char*>);

static_assert(std::is_same_v<
        decltype(std::declval<const st2110::VideoFrame&>().view()),
        st2110::VideoFrameView>);

namespace {

    class FakeVideoSink final : public st2110::IVideoFrameSink {
    public:
        void on_video_frame(const st2110::VideoFrameView& frame) override {
            called = true;
            last_width = frame.width;
            last_height = frame.height;
            last_ts_ns = frame.timestamp_ns;
            last_data0 = frame.data[0];
            last_stride0 = frame.stride[0];
        }

        bool called = false;
        uint32_t last_width = 0;
        uint32_t last_height = 0;
        uint64_t last_ts_ns = 0;
        const uint8_t* last_data0 = nullptr;
        std::size_t last_stride0 = 0;
    };

    class FakeVideoBackend final : public st2110::IRxVideoBackend {
    public:
        const char* backend_name() const override {
            return "fake-video";
        }

        void start(const st2110::RxVideoConfig& cfg, st2110::IVideoFrameSink& sink) override {
            started = true;

            st2110::VideoFrame frame(cfg.width, cfg.height, cfg.format);
            sink.on_video_frame(frame.view(123456789));
        }

        void stop() override {
            stopped = true;
        }

        bool started = false;
        bool stopped = false;
    };

    void test_fake_backend_delivers_video_frame() {
        st2110::RxVideoConfig cfg{};
        cfg.width = 1920;
        cfg.height = 1080;
        cfg.fps_num = 30;
        cfg.fps_den = 1;
        cfg.udp_port = 5004;
        cfg.payload_type = 96;
        cfg.local_ip = "0.0.0.0";
        cfg.dest_ip = "239.0.0.1";
        cfg.format = st2110::PixelFormat::UYVY;

        FakeVideoBackend backend;
        FakeVideoSink sink;

        assert(!backend.started);
        assert(!backend.stopped);
        assert(!sink.called);

        assert(std::string_view(backend.backend_name()) == "fake-video");

        backend.start(cfg, sink);

        assert(backend.started);
        assert(sink.called);
        assert(sink.last_width == 1920);
        assert(sink.last_height == 1080);
        assert(sink.last_ts_ns == 123456789);
        assert(sink.last_data0 != nullptr);
        assert(sink.last_stride0 == 1920u * 2u);

        backend.stop();
        assert(backend.stopped);
    }

} // namespace

int main() {
    test_fake_backend_delivers_video_frame();
    return 0;
}
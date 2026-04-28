#include <cassert>
#include <memory>
#include <string_view>
#include <type_traits>

#include <st2110/backend.hpp>
#include <st2110/backend_factory.hpp>
#include <st2110/rx_config.hpp>
#include <st2110/socket_rx_video_backend.hpp>
#include <st2110/video_frame.hpp>

static_assert(std::is_final_v<st2110::SocketRxVideoBackend>);
static_assert(std::is_base_of_v<st2110::IRxVideoBackend, st2110::SocketRxVideoBackend>);
static_assert(std::is_convertible_v<st2110::SocketRxVideoBackend*, st2110::IRxBackend*>);

static_assert(std::is_final_v<st2110::SocketRxVideoBackendFactory>);
static_assert(std::is_base_of_v<st2110::IRxBackendFactory, st2110::SocketRxVideoBackendFactory>);

namespace
{
    class FakeVideoSink final : public st2110::IVideoFrameSink
    {
    public:
        void on_video_frame(const st2110::VideoFrameView& frame) override
        {
            called = true;
            last_width = frame.width;
            last_height = frame.height;
            last_timestamp_ns = frame.timestamp_ns;
        }

        bool called = false;
        uint32_t last_width = 0;
        uint32_t last_height = 0;
        st2110::TimestampNs last_timestamp_ns = 0;
    };

    st2110::RxVideoConfig make_valid_video_config()
    {
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
        cfg.scan_mode = st2110::VideoScanMode::Progressive;
        cfg.packing_mode = st2110::VideoPackingMode::Gpm;
        return cfg;
    }

    void test_socket_rx_video_backend_basic_contract()
    {
        st2110::SocketRxVideoBackend backend;

        const st2110::IRxBackend& backend_view = backend;
        st2110::IRxBackend& backend_base = backend;

        assert(std::string_view(backend_view.backend_name()) == "socket");

        const auto capabilities = backend_view.capabilities();
        assert(st2110::supports_media(capabilities, st2110::RxMediaKind::Video));
        assert(!st2110::supports_media(capabilities, st2110::RxMediaKind::Audio));

        auto* video_backend = dynamic_cast<st2110::IRxVideoBackend*>(&backend_base);
        assert(video_backend != nullptr);

        auto* audio_backend = dynamic_cast<st2110::IRxAudioBackend*>(&backend_base);
        assert(audio_backend == nullptr);

        FakeVideoSink sink;
        const auto cfg = make_valid_video_config();

        video_backend->start_video(cfg, sink);
        assert(!sink.called);

        backend_base.stop();
        backend_base.stop();
    }

    void test_socket_rx_video_backend_factory_descriptor()
    {
        st2110::SocketRxVideoBackendFactory factory;

        const auto descriptor = factory.descriptor();

        assert(descriptor.kind == st2110::RxBackendKind::Socket);
        assert(descriptor.name == std::string_view{"socket"});
        assert(descriptor.available);
        assert(st2110::supports_media(descriptor.capabilities, st2110::RxMediaKind::Video));
        assert(!st2110::supports_media(descriptor.capabilities, st2110::RxMediaKind::Audio));

        assert(st2110::validate_rx_backend_descriptor(descriptor) == st2110::Error::Ok);
    }

    void test_socket_rx_video_backend_factory_creates_video_backend()
    {
        st2110::SocketRxVideoBackendFactory factory;

        std::unique_ptr<st2110::IRxBackend> backend = factory.create_backend();
        assert(backend != nullptr);

        assert(std::string_view(backend->backend_name()) == "socket");

        const auto capabilities = backend->capabilities();
        assert(st2110::supports_media(capabilities, st2110::RxMediaKind::Video));
        assert(!st2110::supports_media(capabilities, st2110::RxMediaKind::Audio));

        auto* video_backend = dynamic_cast<st2110::IRxVideoBackend*>(backend.get());
        assert(video_backend != nullptr);

        auto* audio_backend = dynamic_cast<st2110::IRxAudioBackend*>(backend.get());
        assert(audio_backend == nullptr);

        FakeVideoSink sink;
        const auto cfg = make_valid_video_config();

        video_backend->start_video(cfg, sink);
        assert(!sink.called);

        backend->stop();
    }
} // namespace

int main()
{
    test_socket_rx_video_backend_basic_contract();
    test_socket_rx_video_backend_factory_descriptor();
    test_socket_rx_video_backend_factory_creates_video_backend();
    return 0;
}
#include <cassert>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <utility>

#include <st2110/audio_frame.hpp>
#include <st2110/backend.hpp>
#include <st2110/rx_config.hpp>
#include <st2110/video_frame.hpp>

static_assert(std::is_abstract_v<st2110::IRxBackend>);

static_assert(std::is_abstract_v<st2110::IRxVideoBackend>);
static_assert(std::is_base_of_v<st2110::IRxBackend, st2110::IRxVideoBackend>);
static_assert(std::is_abstract_v<st2110::IVideoFrameSink>);

static_assert(std::is_abstract_v<st2110::IRxAudioBackend>);
static_assert(std::is_base_of_v<st2110::IRxBackend, st2110::IRxAudioBackend>);
static_assert(std::is_abstract_v<st2110::IAudioFrameSink>);

static_assert(std::is_same_v<
        decltype(std::declval<const st2110::IRxBackend&>().backend_name()),
        const char*>);

static_assert(std::is_same_v<
        decltype(std::declval<st2110::IRxBackend&>().stop()),
        void>);

static_assert(std::is_same_v<
        decltype(std::declval<st2110::IRxVideoBackend&>().start(
                std::declval<const st2110::RxVideoConfig&>(),
                std::declval<st2110::IVideoFrameSink&>())),
        void>);

static_assert(std::is_same_v<
        decltype(std::declval<st2110::IRxAudioBackend&>().start(
                std::declval<const st2110::RxAudioConfig&>(),
                std::declval<st2110::IAudioFrameSink&>())),
        void>);

static_assert(std::is_same_v<
        decltype(std::declval<st2110::IVideoFrameSink&>().on_video_frame(
                std::declval<const st2110::VideoFrameView&>())),
        void>);

static_assert(std::is_same_v<
        decltype(std::declval<st2110::IAudioFrameSink&>().on_audio_frame(
                std::declval<const st2110::AudioFrameView&>())),
        void>);

static_assert(std::is_same_v<
        decltype(std::declval<const st2110::VideoFrame&>().view()),
        st2110::VideoFrameView>);

static_assert(std::is_same_v<
        decltype(std::declval<const st2110::AudioBuffer&>().view()),
        st2110::AudioFrameView>);

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

    class FakeAudioSink final : public st2110::IAudioFrameSink {
    public:
        void on_audio_frame(const st2110::AudioFrameView& frame) override {
            called = true;
            last_storage_format = frame.storage_format;
            last_sampling_rate_hz = frame.sampling_rate_hz;
            last_channel_count = frame.channel_count;
            last_samples_per_channel = frame.samples_per_channel;
            last_total_sample_count = frame.total_sample_count;
            last_sample_frame_stride = frame.sample_frame_stride;
            last_size_bytes = frame.size_bytes;
            last_ts_ns = frame.timestamp_ns;
            last_samples = frame.samples;

            if (frame.samples != nullptr && frame.total_sample_count >= 2) {
                first_sample = frame.samples[0];
                second_sample = frame.samples[1];
            }
        }

        bool called = false;
        st2110::AudioSampleStorageFormat last_storage_format =
                st2110::AudioSampleStorageFormat::InterleavedS32;
        uint32_t last_sampling_rate_hz = 0;
        uint16_t last_channel_count = 0;
        uint32_t last_samples_per_channel = 0;
        std::size_t last_total_sample_count = 0;
        std::size_t last_sample_frame_stride = 0;
        std::size_t last_size_bytes = 0;
        uint64_t last_ts_ns = 0;
        const std::int32_t* last_samples = nullptr;
        std::int32_t first_sample = 0;
        std::int32_t second_sample = 0;
    };

    class FakeAudioBackend final : public st2110::IRxAudioBackend {
    public:
        const char* backend_name() const override {
            return "fake-audio";
        }

        void start(const st2110::RxAudioConfig& cfg, st2110::IAudioFrameSink& sink) override {
            started = true;

            st2110::AudioBuffer buffer(cfg);
            buffer.sample(0, 0) = 11;
            buffer.sample(0, 1) = 22;

            sink.on_audio_frame(buffer.view(987654321));
        }

        void stop() override {
            stopped = true;
        }

        bool started = false;
        bool stopped = false;
    };

    st2110::RxVideoConfig make_valid_video_config() {
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
        return cfg;
    }

    st2110::RxAudioConfig make_valid_audio_config() {
        st2110::RxAudioConfig cfg{};
        cfg.sampling_rate_hz = 48000;
        cfg.packet_time_us = 1000;
        cfg.samples_per_packet = 48;
        cfg.channel_count = 2;
        cfg.udp_port = 5006;
        cfg.payload_type = 97;
        cfg.local_ip = "0.0.0.0";
        cfg.dest_ip = "239.0.0.2";
        cfg.format = st2110::AudioSampleFormat::LinearPcm;
        return cfg;
    }

    void test_fake_backend_delivers_video_frame() {
        const auto cfg = make_valid_video_config();

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

    void test_fake_backend_delivers_audio_frame() {
        const auto cfg = make_valid_audio_config();

        FakeAudioBackend backend;
        FakeAudioSink sink;

        assert(!backend.started);
        assert(!backend.stopped);
        assert(!sink.called);

        assert(std::string_view(backend.backend_name()) == "fake-audio");

        backend.start(cfg, sink);

        assert(backend.started);
        assert(sink.called);
        assert(sink.last_storage_format == st2110::AudioSampleStorageFormat::InterleavedS32);
        assert(sink.last_sampling_rate_hz == 48000);
        assert(sink.last_channel_count == 2);
        assert(sink.last_samples_per_channel == 48);
        assert(sink.last_total_sample_count == 96);
        assert(sink.last_sample_frame_stride == 2);
        assert(sink.last_size_bytes == 96u * sizeof(std::int32_t));
        assert(sink.last_ts_ns == 987654321);
        assert(sink.last_samples != nullptr);
        assert(sink.first_sample == 11);
        assert(sink.second_sample == 22);

        backend.stop();
        assert(backend.stopped);
    }

} // namespace

int main() {
    test_fake_backend_delivers_video_frame();
    test_fake_backend_delivers_audio_frame();
    return 0;
}
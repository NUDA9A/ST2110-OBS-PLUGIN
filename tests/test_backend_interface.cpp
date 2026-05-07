#include <cassert>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string_view>
#include <type_traits>
#include <utility>

#include <st2110/contracts/backend/backend.hpp>
#include <st2110/delivery/audio/audio_frame.hpp>
#include <st2110/delivery/video/video_frame.hpp>
#include <st2110/foundation/error.hpp>
#include <st2110/rx_config.hpp>

static_assert(std::is_abstract_v<st2110::IRxBackend>);

static_assert(std::is_abstract_v<st2110::IRxVideoBackend>);
static_assert(std::is_base_of_v<st2110::IRxBackend, st2110::IRxVideoBackend>);
static_assert(std::is_abstract_v<st2110::IVideoFrameSink>);

static_assert(std::is_abstract_v<st2110::IRxAudioBackend>);
static_assert(std::is_base_of_v<st2110::IRxBackend, st2110::IRxAudioBackend>);
static_assert(std::is_abstract_v<st2110::IAudioFrameSink>);

static_assert(std::is_same_v<decltype(st2110::RxBackendCapabilities{}.video_rx), bool>);
static_assert(std::is_same_v<decltype(st2110::RxBackendCapabilities{}.audio_rx), bool>);

static_assert(std::is_same_v<decltype(st2110::RxBackendState{}.video_active), bool>);
static_assert(std::is_same_v<decltype(st2110::RxBackendState{}.audio_active), bool>);

static_assert(std::is_same_v<decltype(st2110::supports_media(std::declval<const st2110::RxBackendCapabilities &>(),
                                                             st2110::RxMediaKind::Video)),
                             bool>);

static_assert(
    std::is_same_v<decltype(st2110::backend_is_stopped(std::declval<const st2110::RxBackendState &>())), bool>);

static_assert(std::is_same_v<decltype(st2110::backend_media_active(std::declval<const st2110::RxBackendState &>(),
                                                                   st2110::RxMediaKind::Video)),
                             bool>);

static_assert(std::is_same_v<decltype(std::declval<const st2110::IRxBackend &>().backend_name()), const char *>);

static_assert(
    std::is_same_v<decltype(std::declval<const st2110::IRxBackend &>().capabilities()), st2110::RxBackendCapabilities>);

static_assert(std::is_same_v<decltype(std::declval<const st2110::IRxBackend &>().state()), st2110::RxBackendState>);

static_assert(std::is_same_v<decltype(std::declval<const st2110::IRxBackend &>().stats()), st2110::BackendStats>);

static_assert(std::is_same_v<decltype(std::declval<st2110::IRxBackend &>().stop()), st2110::RxBackendLifecycleResult>);

static_assert(
    std::is_same_v<decltype(std::declval<st2110::IRxVideoBackend &>().start_video(
                       std::declval<const st2110::RxVideoConfig &>(), std::declval<st2110::IVideoFrameSink &>())),
                   st2110::RxBackendLifecycleResult>);

static_assert(
    std::is_same_v<decltype(std::declval<st2110::IRxAudioBackend &>().start_audio(
                       std::declval<const st2110::RxAudioConfig &>(), std::declval<st2110::IAudioFrameSink &>())),
                   st2110::RxBackendLifecycleResult>);

static_assert(std::is_same_v<decltype(std::declval<st2110::IVideoFrameSink &>().on_video_frame(
                                 std::declval<const st2110::VideoFrameView &>())),
                             void>);

static_assert(std::is_same_v<decltype(std::declval<st2110::IAudioFrameSink &>().on_audio_frame(
                                 std::declval<const st2110::AudioFrameView &>())),
                             void>);

static_assert(std::is_same_v<decltype(std::declval<const st2110::VideoFrame &>().view()), st2110::VideoFrameView>);

static_assert(std::is_same_v<decltype(std::declval<const st2110::AudioBuffer &>().view()), st2110::AudioFrameView>);

namespace {
class FakeVideoSink final : public st2110::IVideoFrameSink {
  public:
    void on_video_frame(const st2110::VideoFrameView &frame) override {
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
    const uint8_t *last_data0 = nullptr;
    std::size_t last_stride0 = 0;
};

class FakeAudioSink final : public st2110::IAudioFrameSink {
  public:
    void on_audio_frame(const st2110::AudioFrameView &frame) override {
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
    st2110::AudioSampleStorageFormat last_storage_format = st2110::AudioSampleStorageFormat::InterleavedS32;
    uint32_t last_sampling_rate_hz = 0;
    uint16_t last_channel_count = 0;
    uint32_t last_samples_per_channel = 0;
    std::size_t last_total_sample_count = 0;
    std::size_t last_sample_frame_stride = 0;
    std::size_t last_size_bytes = 0;
    uint64_t last_ts_ns = 0;
    const std::int32_t *last_samples = nullptr;
    std::int32_t first_sample = 0;
    std::int32_t second_sample = 0;
};

class FakeVideoBackend final : public st2110::IRxVideoBackend {
  public:
    const char *backend_name() const override { return "fake-video"; }

    st2110::RxBackendCapabilities capabilities() const override {
        st2110::RxBackendCapabilities result{};
        result.video_rx = true;
        return result;
    }

    st2110::RxBackendState state() const override { return state_; }

    st2110::BackendStats stats() const override { return stats_; }

    st2110::RxBackendLifecycleResult start_video(const st2110::RxVideoConfig &cfg,
                                                 st2110::IVideoFrameSink &sink) override {
        if (state_.video_active) {
            return std::unexpected(st2110::Error::InvalidBackendState);
        }

        state_.video_active = true;

        st2110::VideoFrame frame(cfg.width, cfg.height, cfg.format);
        sink.on_video_frame(frame.view(123456789));

        ++stats_.frames_delivered;
        ++stats_.media_units_delivered;

        return state_;
    }

    st2110::RxBackendLifecycleResult stop() override {
        state_ = {};
        return state_;
    }

  private:
    st2110::RxBackendState state_{};
    st2110::BackendStats stats_{};
};

class FakeAudioBackend final : public st2110::IRxAudioBackend {
  public:
    const char *backend_name() const override { return "fake-audio"; }

    st2110::RxBackendCapabilities capabilities() const override {
        st2110::RxBackendCapabilities result{};
        result.audio_rx = true;
        return result;
    }

    st2110::RxBackendState state() const override { return state_; }

    st2110::BackendStats stats() const override { return stats_; }

    st2110::RxBackendLifecycleResult start_audio(const st2110::RxAudioConfig &cfg,
                                                 st2110::IAudioFrameSink &sink) override {
        if (state_.audio_active) {
            return std::unexpected(st2110::Error::InvalidBackendState);
        }

        state_.audio_active = true;

        st2110::AudioBuffer buffer(cfg);
        buffer.sample(0, 0) = 11;
        buffer.sample(0, 1) = 22;

        sink.on_audio_frame(buffer.view(987654321));

        ++stats_.media_units_delivered;

        return state_;
    }

    st2110::RxBackendLifecycleResult stop() override {
        state_ = {};
        return state_;
    }

  private:
    st2110::RxBackendState state_{};
    st2110::BackendStats stats_{};
};

class FakeCombinedBackend final : public st2110::IRxVideoBackend, public st2110::IRxAudioBackend {
  public:
    const char *backend_name() const override { return "fake-combined"; }

    st2110::RxBackendCapabilities capabilities() const override {
        st2110::RxBackendCapabilities result{};
        result.video_rx = true;
        result.audio_rx = true;
        return result;
    }

    st2110::RxBackendState state() const override { return state_; }

    st2110::BackendStats stats() const override { return stats_; }

    st2110::RxBackendLifecycleResult start_video(const st2110::RxVideoConfig &cfg,
                                                 st2110::IVideoFrameSink &sink) override {
        if (state_.video_active) {
            return std::unexpected(st2110::Error::InvalidBackendState);
        }

        state_.video_active = true;

        st2110::VideoFrame frame(cfg.width, cfg.height, cfg.format);
        sink.on_video_frame(frame.view(222222222));

        ++stats_.frames_delivered;
        ++stats_.media_units_delivered;

        return state_;
    }

    st2110::RxBackendLifecycleResult start_audio(const st2110::RxAudioConfig &cfg,
                                                 st2110::IAudioFrameSink &sink) override {
        if (state_.audio_active) {
            return std::unexpected(st2110::Error::InvalidBackendState);
        }

        state_.audio_active = true;

        st2110::AudioBuffer buffer(cfg);
        buffer.sample(0, 0) = 33;
        buffer.sample(0, 1) = 44;

        sink.on_audio_frame(buffer.view(333333333));

        ++stats_.media_units_delivered;

        return state_;
    }

    st2110::RxBackendLifecycleResult stop() override {
        state_ = {};
        return state_;
    }

  private:
    st2110::RxBackendState state_{};
    st2110::BackendStats stats_{};
};

class FlakyVideoBackend final : public st2110::IRxVideoBackend {
  public:
    const char *backend_name() const override { return "flaky-video"; }

    st2110::RxBackendCapabilities capabilities() const override {
        st2110::RxBackendCapabilities result{};
        result.video_rx = true;
        return result;
    }

    st2110::RxBackendState state() const override { return state_; }

    st2110::BackendStats stats() const override { return stats_; }

    st2110::RxBackendLifecycleResult start_video(const st2110::RxVideoConfig &cfg,
                                                 st2110::IVideoFrameSink &sink) override {
        (void)cfg;
        (void)sink;

        if (state_.video_active) {
            return std::unexpected(st2110::Error::InvalidBackendState);
        }

        if (fail_next_start_) {
            fail_next_start_ = false;
            ++stats_.datagrams_dropped;
            return std::unexpected(st2110::Error::SystemFailure);
        }

        state_.video_active = true;
        return state_;
    }

    st2110::RxBackendLifecycleResult stop() override {
        state_ = {};
        return state_;
    }

  private:
    bool fail_next_start_ = true;
    st2110::RxBackendState state_{};
    st2110::BackendStats stats_{};
};

static_assert(std::is_convertible_v<FakeCombinedBackend *, st2110::IRxBackend *>);
static_assert(std::is_convertible_v<FakeCombinedBackend *, st2110::IRxVideoBackend *>);
static_assert(std::is_convertible_v<FakeCombinedBackend *, st2110::IRxAudioBackend *>);

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
    cfg.scan_mode = st2110::VideoScanMode::Progressive;
    cfg.packing_mode = st2110::VideoPackingMode::Gpm;
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

void test_backend_capability_and_state_helpers() {
    st2110::RxBackendCapabilities none{};
    assert(!none.video_rx);
    assert(!none.audio_rx);
    assert(!st2110::supports_media(none, st2110::RxMediaKind::Video));
    assert(!st2110::supports_media(none, st2110::RxMediaKind::Audio));

    st2110::RxBackendCapabilities video_only{};
    video_only.video_rx = true;
    assert(st2110::supports_media(video_only, st2110::RxMediaKind::Video));
    assert(!st2110::supports_media(video_only, st2110::RxMediaKind::Audio));

    st2110::RxBackendCapabilities audio_only{};
    audio_only.audio_rx = true;
    assert(!st2110::supports_media(audio_only, st2110::RxMediaKind::Video));
    assert(st2110::supports_media(audio_only, st2110::RxMediaKind::Audio));

    st2110::RxBackendCapabilities combined{};
    combined.video_rx = true;
    combined.audio_rx = true;
    assert(st2110::supports_media(combined, st2110::RxMediaKind::Video));
    assert(st2110::supports_media(combined, st2110::RxMediaKind::Audio));

    assert(!st2110::supports_media(combined, static_cast<st2110::RxMediaKind>(255)));

    st2110::RxBackendState stopped{};
    assert(st2110::backend_is_stopped(stopped));
    assert(!st2110::backend_media_active(stopped, st2110::RxMediaKind::Video));
    assert(!st2110::backend_media_active(stopped, st2110::RxMediaKind::Audio));

    st2110::RxBackendState video_active{};
    video_active.video_active = true;
    assert(!st2110::backend_is_stopped(video_active));
    assert(st2110::backend_media_active(video_active, st2110::RxMediaKind::Video));
    assert(!st2110::backend_media_active(video_active, st2110::RxMediaKind::Audio));

    st2110::RxBackendState audio_active{};
    audio_active.audio_active = true;
    assert(!st2110::backend_is_stopped(audio_active));
    assert(!st2110::backend_media_active(audio_active, st2110::RxMediaKind::Video));
    assert(st2110::backend_media_active(audio_active, st2110::RxMediaKind::Audio));

    st2110::RxBackendState both_active{};
    both_active.video_active = true;
    both_active.audio_active = true;
    assert(!st2110::backend_is_stopped(both_active));
    assert(st2110::backend_media_active(both_active, st2110::RxMediaKind::Video));
    assert(st2110::backend_media_active(both_active, st2110::RxMediaKind::Audio));

    assert(!st2110::backend_media_active(both_active, static_cast<st2110::RxMediaKind>(255)));
}

void test_fake_video_backend_lifecycle_and_delivery() {
    const auto cfg = make_valid_video_config();

    FakeVideoBackend backend;
    FakeVideoSink sink;

    const st2110::IRxBackend &backend_base_view = backend;
    assert(std::string_view(backend_base_view.backend_name()) == "fake-video");

    const auto capabilities = backend_base_view.capabilities();
    assert(st2110::supports_media(capabilities, st2110::RxMediaKind::Video));
    assert(!st2110::supports_media(capabilities, st2110::RxMediaKind::Audio));

    const auto stats_before_start = backend_base_view.stats();
    assert(stats_before_start.frames_delivered == 0);
    assert(stats_before_start.media_units_delivered == 0);
    assert(stats_before_start.datagrams_received == 0);

    assert(st2110::backend_is_stopped(backend.state()));
    assert(!sink.called);

    auto stop_before_start = backend.stop();
    assert(stop_before_start.has_value());
    assert(st2110::backend_is_stopped(*stop_before_start));
    assert(st2110::backend_is_stopped(backend.state()));

    auto started = backend.start_video(cfg, sink);
    assert(started.has_value());
    assert(st2110::backend_media_active(*started, st2110::RxMediaKind::Video));
    assert(!st2110::backend_media_active(*started, st2110::RxMediaKind::Audio));
    assert(st2110::backend_media_active(backend.state(), st2110::RxMediaKind::Video));

    assert(sink.called);
    assert(sink.last_width == 1920);
    assert(sink.last_height == 1080);
    assert(sink.last_ts_ns == 123456789);
    assert(sink.last_data0 != nullptr);
    assert(sink.last_stride0 == 1920u * 2u);

    const auto stats_after_start = backend_base_view.stats();
    assert(stats_after_start.frames_delivered == 1);
    assert(stats_after_start.media_units_delivered == 1);
    assert(stats_after_start.datagrams_received == 0);
    assert(stats_after_start.packets_parsed_ok == 0);

    auto started_again = backend.start_video(cfg, sink);
    assert(!started_again.has_value());
    assert(started_again.error() == st2110::Error::InvalidBackendState);
    assert(st2110::backend_media_active(backend.state(), st2110::RxMediaKind::Video));

    auto stopped = backend.stop();
    assert(stopped.has_value());
    assert(st2110::backend_is_stopped(*stopped));
    assert(st2110::backend_is_stopped(backend.state()));

    auto stopped_again = backend.stop();
    assert(stopped_again.has_value());
    assert(st2110::backend_is_stopped(*stopped_again));
    assert(st2110::backend_is_stopped(backend.state()));

    auto restarted = backend.start_video(cfg, sink);
    assert(restarted.has_value());
    assert(st2110::backend_media_active(*restarted, st2110::RxMediaKind::Video));

    const auto stats_after_restart = backend_base_view.stats();
    assert(stats_after_restart.frames_delivered == 2);
    assert(stats_after_restart.media_units_delivered == 2);
}

void test_fake_audio_backend_lifecycle_and_delivery() {
    const auto cfg = make_valid_audio_config();

    FakeAudioBackend backend;
    FakeAudioSink sink;

    const st2110::IRxBackend &backend_base_view = backend;
    assert(std::string_view(backend_base_view.backend_name()) == "fake-audio");

    const auto capabilities = backend_base_view.capabilities();
    assert(!st2110::supports_media(capabilities, st2110::RxMediaKind::Video));
    assert(st2110::supports_media(capabilities, st2110::RxMediaKind::Audio));

    const auto stats_before_start = backend_base_view.stats();
    assert(stats_before_start.media_units_delivered == 0);
    assert(stats_before_start.frames_delivered == 0);

    assert(st2110::backend_is_stopped(backend.state()));
    assert(!sink.called);

    auto started = backend.start_audio(cfg, sink);
    assert(started.has_value());
    assert(!st2110::backend_media_active(*started, st2110::RxMediaKind::Video));
    assert(st2110::backend_media_active(*started, st2110::RxMediaKind::Audio));
    assert(st2110::backend_media_active(backend.state(), st2110::RxMediaKind::Audio));

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

    const auto stats_after_start = backend_base_view.stats();
    assert(stats_after_start.media_units_delivered == 1);
    assert(stats_after_start.frames_delivered == 0);

    auto started_again = backend.start_audio(cfg, sink);
    assert(!started_again.has_value());
    assert(started_again.error() == st2110::Error::InvalidBackendState);
    assert(st2110::backend_media_active(backend.state(), st2110::RxMediaKind::Audio));

    auto stopped = backend.stop();
    assert(stopped.has_value());
    assert(st2110::backend_is_stopped(*stopped));
    assert(st2110::backend_is_stopped(backend.state()));
}

void test_combined_backend_has_single_common_backend_base_and_combined_state() {
    const auto video_cfg = make_valid_video_config();
    const auto audio_cfg = make_valid_audio_config();

    FakeCombinedBackend backend;
    FakeVideoSink video_sink;
    FakeAudioSink audio_sink;

    st2110::IRxBackend &backend_base = backend;
    st2110::IRxVideoBackend &video_backend = backend;
    st2110::IRxAudioBackend &audio_backend = backend;

    assert(std::string_view(backend_base.backend_name()) == "fake-combined");

    const auto capabilities = backend_base.capabilities();
    assert(st2110::supports_media(capabilities, st2110::RxMediaKind::Video));
    assert(st2110::supports_media(capabilities, st2110::RxMediaKind::Audio));
    assert(st2110::backend_is_stopped(backend_base.state()));

    const auto stats_before_start = backend_base.stats();
    assert(stats_before_start.frames_delivered == 0);
    assert(stats_before_start.media_units_delivered == 0);

    auto video_started = video_backend.start_video(video_cfg, video_sink);
    assert(video_started.has_value());
    assert(st2110::backend_media_active(*video_started, st2110::RxMediaKind::Video));
    assert(!st2110::backend_media_active(*video_started, st2110::RxMediaKind::Audio));

    auto audio_started = audio_backend.start_audio(audio_cfg, audio_sink);
    assert(audio_started.has_value());
    assert(st2110::backend_media_active(*audio_started, st2110::RxMediaKind::Video));
    assert(st2110::backend_media_active(*audio_started, st2110::RxMediaKind::Audio));
    assert(st2110::backend_media_active(backend_base.state(), st2110::RxMediaKind::Video));
    assert(st2110::backend_media_active(backend_base.state(), st2110::RxMediaKind::Audio));

    assert(video_sink.called);
    assert(video_sink.last_width == 1920);
    assert(video_sink.last_height == 1080);
    assert(video_sink.last_ts_ns == 222222222);

    assert(audio_sink.called);
    assert(audio_sink.last_sampling_rate_hz == 48000);
    assert(audio_sink.last_channel_count == 2);
    assert(audio_sink.last_samples_per_channel == 48);
    assert(audio_sink.last_ts_ns == 333333333);
    assert(audio_sink.first_sample == 33);
    assert(audio_sink.second_sample == 44);

    const auto stats_after_start = backend_base.stats();
    assert(stats_after_start.frames_delivered == 1);
    assert(stats_after_start.media_units_delivered == 2);

    auto video_started_again = video_backend.start_video(video_cfg, video_sink);
    assert(!video_started_again.has_value());
    assert(video_started_again.error() == st2110::Error::InvalidBackendState);

    auto audio_started_again = audio_backend.start_audio(audio_cfg, audio_sink);
    assert(!audio_started_again.has_value());
    assert(audio_started_again.error() == st2110::Error::InvalidBackendState);

    auto stopped = backend_base.stop();
    assert(stopped.has_value());
    assert(st2110::backend_is_stopped(*stopped));
    assert(st2110::backend_is_stopped(backend_base.state()));
}

void test_failed_start_leaves_backend_stopped_and_retryable() {
    const auto cfg = make_valid_video_config();

    FlakyVideoBackend backend;
    FakeVideoSink sink;

    assert(st2110::backend_is_stopped(backend.state()));

    auto first_start = backend.start_video(cfg, sink);
    assert(!first_start.has_value());
    assert(first_start.error() == st2110::Error::SystemFailure);
    assert(st2110::backend_is_stopped(backend.state()));

    const auto stats_after_failed_start = backend.stats();
    assert(stats_after_failed_start.datagrams_dropped == 1);
    assert(stats_after_failed_start.frames_delivered == 0);

    auto stop_after_failed_start = backend.stop();
    assert(stop_after_failed_start.has_value());
    assert(st2110::backend_is_stopped(*stop_after_failed_start));
    assert(st2110::backend_is_stopped(backend.state()));

    auto second_start = backend.start_video(cfg, sink);
    assert(second_start.has_value());
    assert(st2110::backend_media_active(*second_start, st2110::RxMediaKind::Video));
    assert(st2110::backend_media_active(backend.state(), st2110::RxMediaKind::Video));
}
} // namespace

int main() {
    test_backend_capability_and_state_helpers();
    test_fake_video_backend_lifecycle_and_delivery();
    test_fake_audio_backend_lifecycle_and_delivery();
    test_combined_backend_has_single_common_backend_base_and_combined_state();
    test_failed_start_leaves_backend_stopped_and_retryable();
    return 0;
}
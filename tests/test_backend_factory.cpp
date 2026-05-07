#include <array>
#include <cassert>
#include <cstddef>
#include <expected>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

#include <st2110/backend.hpp>
#include <st2110/backend_factory.hpp>
#include <st2110/error.hpp>

#ifndef ST2110_TEST_EXPECT_MTL_BUILT
#error "ST2110_TEST_EXPECT_MTL_BUILT must be provided by tests/CMakeLists.txt"
#endif

#if ST2110_TEST_EXPECT_MTL_BUILT
#include <st2110/mtl_rx_backend_factory.hpp>
#include <st2110/mtl_rx_video_backend.hpp>
#endif

static_assert(std::is_enum_v<st2110::RxBackendKind>);
static_assert(!std::is_convertible_v<st2110::RxBackendKind, int>);

static_assert(
    std::is_same_v<decltype(st2110::validate_rx_backend_kind(st2110::RxBackendKind::Socket)), st2110::Error>);

static_assert(
    std::is_same_v<decltype(st2110::rx_backend_kind_name(st2110::RxBackendKind::Socket)), std::string_view>);

static_assert(std::is_same_v<decltype(st2110::parse_rx_backend_kind(std::declval<std::string_view>())),
                             std::expected<st2110::RxBackendKind, st2110::Error>>);

static_assert(std::is_same_v<
              decltype(st2110::validate_rx_backend_descriptor(std::declval<const st2110::RxBackendDescriptor &>())),
              st2110::Error>);

static_assert(
    std::is_same_v<decltype(st2110::validate_rx_backend_selection(std::declval<const st2110::RxBackendSelection &>())),
                   st2110::Error>);

static_assert(std::is_same_v<decltype(st2110::rx_backend_kind_built(st2110::RxBackendKind::Socket)), bool>);

static_assert(std::is_same_v<decltype(st2110::default_rx_backend_factories()),
                             std::span<st2110::IRxBackendFactory *const>>);

static_assert(std::is_abstract_v<st2110::IRxBackend>);

static_assert(std::is_same_v<decltype(std::declval<const st2110::IRxBackendFactory &>().descriptor()),
                             st2110::RxBackendDescriptor>);

static_assert(std::is_same_v<decltype(std::declval<const st2110::IRxBackendFactory &>().create_backend()),
                             std::unique_ptr<st2110::IRxBackend>>);

static_assert(std::is_same_v<decltype(std::declval<const st2110::IRxBackend &>().stats()), st2110::BackendStats>);

static_assert(std::is_same_v<
              decltype(st2110::select_rx_backend_factory(std::declval<std::span<st2110::IRxBackendFactory *const>>(),
                                                         std::declval<const st2110::RxBackendSelection &>())),
              std::expected<st2110::IRxBackendFactory *, st2110::Error>>);

static_assert(
    std::is_same_v<decltype(st2110::create_rx_backend(std::declval<std::span<st2110::IRxBackendFactory *const>>(),
                                                      std::declval<const st2110::RxBackendSelection &>())),
                   std::expected<std::unique_ptr<st2110::IRxBackend>, st2110::Error>>);

static_assert(
    std::is_same_v<decltype(st2110::validate_rx_video_backend_selection_support(
                       std::declval<const st2110::RxBackendSelection &>(),
                       std::declval<const st2110::RxVideoConfig &>())),
                   st2110::Error>);

static_assert(
    std::is_same_v<decltype(st2110::select_rx_video_backend_factory(
                       std::declval<std::span<st2110::IRxBackendFactory *const>>(),
                       std::declval<const st2110::RxBackendSelection &>(),
                       std::declval<const st2110::RxVideoConfig &>())),
                   std::expected<st2110::IRxBackendFactory *, st2110::Error>>);

static_assert(
    std::is_same_v<decltype(st2110::create_rx_video_backend(
                       std::declval<std::span<st2110::IRxBackendFactory *const>>(),
                       std::declval<const st2110::RxBackendSelection &>(),
                       std::declval<const st2110::RxVideoConfig &>())),
                   std::expected<std::unique_ptr<st2110::IRxBackend>, st2110::Error>>);

namespace {
constexpr bool kExpectMtlBuilt = ST2110_TEST_EXPECT_MTL_BUILT != 0;

class FakeBackend final : public st2110::IRxBackend {
  public:
    FakeBackend(std::string_view backend_name, st2110::RxBackendCapabilities backend_capabilities)
        : name_(backend_name), capabilities_(backend_capabilities) {}

    const char *backend_name() const override { return name_.data(); }

    st2110::RxBackendCapabilities capabilities() const override { return capabilities_; }

    st2110::RxBackendState state() const override { return state_; }

    st2110::BackendStats stats() const override { return stats_; }

    st2110::RxBackendLifecycleResult stop() override {
        stopped = true;
        state_ = {};
        return state_;
    }

    bool stopped = false;

  private:
    std::string_view name_;
    st2110::RxBackendCapabilities capabilities_;
    st2110::RxBackendState state_{};
    st2110::BackendStats stats_{};
};

class FakeBackendFactory final : public st2110::IRxBackendFactory {
  public:
    explicit FakeBackendFactory(st2110::RxBackendDescriptor descriptor, bool return_null_backend = false)
        : descriptor_(descriptor), return_null_backend_(return_null_backend) {}

    st2110::RxBackendDescriptor descriptor() const override { return descriptor_; }

    std::unique_ptr<st2110::IRxBackend> create_backend() const override {
        ++create_count;

        if (return_null_backend_) {
            return nullptr;
        }

        return std::make_unique<FakeBackend>(descriptor_.name, descriptor_.capabilities);
    }

    mutable int create_count = 0;

  private:
    st2110::RxBackendDescriptor descriptor_;
    bool return_null_backend_ = false;
};

class FakeVideoSink final : public st2110::IVideoFrameSink {
  public:
    void on_video_frame(const st2110::VideoFrameView &) override { ++frames_received; }

    int frames_received = 0;
};

st2110::RxBackendCapabilities video_only_capabilities() {
    st2110::RxBackendCapabilities capabilities{};
    capabilities.video_rx = true;
    return capabilities;
}

st2110::RxBackendCapabilities audio_only_capabilities() {
    st2110::RxBackendCapabilities capabilities{};
    capabilities.audio_rx = true;
    return capabilities;
}

st2110::RxBackendCapabilities combined_capabilities() {
    st2110::RxBackendCapabilities capabilities{};
    capabilities.video_rx = true;
    capabilities.audio_rx = true;
    return capabilities;
}

st2110::RxVideoConfig make_valid_video_config() {
    st2110::RxVideoConfig cfg{};
    cfg.width = 1920;
    cfg.height = 1080;
    cfg.fps_num = 25;
    cfg.fps_den = 1;
    cfg.udp_port = 5004;
    cfg.payload_type = 96;
    cfg.local_ip = "";
    cfg.dest_ip = "239.1.1.1";
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.scan_mode = st2110::VideoScanMode::Progressive;
    cfg.packing_mode = st2110::VideoPackingMode::Gpm;
    return cfg;
}

void assert_zero_backend_stats(const st2110::BackendStats &stats) {
    assert(stats.datagrams_received == 0);
    assert(stats.bytes_received == 0);
    assert(stats.control_datagrams_ignored == 0);
    assert(stats.nonmedia_datagrams_ignored == 0);
    assert(stats.packets_parsed_ok == 0);
    assert(stats.packets_rejected == 0);
    assert(stats.datagrams_dropped == 0);
    assert(stats.frames_delivered == 0);
    assert(stats.media_units_delivered == 0);
}

void test_backend_kind_validation_and_string_mapping() {
    assert(st2110::validate_rx_backend_kind(st2110::RxBackendKind::Socket) == st2110::Error::Ok);
    assert(st2110::validate_rx_backend_kind(st2110::RxBackendKind::Mtl) == st2110::Error::Ok);
    assert(st2110::validate_rx_backend_kind(static_cast<st2110::RxBackendKind>(255)) == st2110::Error::InvalidValue);

    assert(st2110::rx_backend_kind_name(st2110::RxBackendKind::Socket) == std::string_view{"socket"});
    assert(st2110::rx_backend_kind_name(st2110::RxBackendKind::Mtl) == std::string_view{"mtl"});
    assert(st2110::rx_backend_kind_name(static_cast<st2110::RxBackendKind>(255)).empty());

    auto socket = st2110::parse_rx_backend_kind("socket");
    assert(socket.has_value());
    assert(*socket == st2110::RxBackendKind::Socket);

    auto mtl = st2110::parse_rx_backend_kind("mtl");
    assert(mtl.has_value());
    assert(*mtl == st2110::RxBackendKind::Mtl);

    auto empty = st2110::parse_rx_backend_kind("");
    assert(!empty.has_value());
    assert(empty.error() == st2110::Error::InvalidValue);

    auto uppercase = st2110::parse_rx_backend_kind("Socket");
    assert(!uppercase.has_value());
    assert(uppercase.error() == st2110::Error::InvalidValue);

    auto unknown = st2110::parse_rx_backend_kind("udp");
    assert(!unknown.has_value());
    assert(unknown.error() == st2110::Error::InvalidValue);
}

void test_backend_kind_built_helper() {
    assert(st2110::rx_backend_kind_built(st2110::RxBackendKind::Socket));

    const bool mtl_built = st2110::rx_backend_kind_built(st2110::RxBackendKind::Mtl);
    assert(mtl_built == kExpectMtlBuilt);

    assert(!st2110::rx_backend_kind_built(static_cast<st2110::RxBackendKind>(255)));
}

void test_descriptor_validation() {
    st2110::RxBackendDescriptor valid_socket{st2110::RxBackendKind::Socket, "socket", combined_capabilities(), true};
    assert(st2110::validate_rx_backend_descriptor(valid_socket) == st2110::Error::Ok);

    st2110::RxBackendDescriptor unavailable_mtl{st2110::RxBackendKind::Mtl, "mtl", video_only_capabilities(), false};
    assert(st2110::validate_rx_backend_descriptor(unavailable_mtl) == st2110::Error::Ok);

    st2110::RxBackendDescriptor invalid_kind = valid_socket;
    invalid_kind.kind = static_cast<st2110::RxBackendKind>(255);
    assert(st2110::validate_rx_backend_descriptor(invalid_kind) == st2110::Error::InvalidValue);

    st2110::RxBackendDescriptor empty_name = valid_socket;
    empty_name.name = {};
    assert(st2110::validate_rx_backend_descriptor(empty_name) == st2110::Error::InvalidValue);

    st2110::RxBackendDescriptor no_media = valid_socket;
    no_media.capabilities = {};
    assert(st2110::validate_rx_backend_descriptor(no_media) == st2110::Error::InvalidValue);
}

void test_selection_validation() {
    st2110::RxBackendSelection valid{st2110::RxBackendKind::Socket, st2110::RxMediaKind::Video};
    assert(st2110::validate_rx_backend_selection(valid) == st2110::Error::Ok);

    st2110::RxBackendSelection invalid_backend_kind = valid;
    invalid_backend_kind.backend_kind = static_cast<st2110::RxBackendKind>(255);
    assert(st2110::validate_rx_backend_selection(invalid_backend_kind) == st2110::Error::InvalidValue);

    st2110::RxBackendSelection invalid_media_kind = valid;
    invalid_media_kind.media_kind = static_cast<st2110::RxMediaKind>(255);
    assert(st2110::validate_rx_backend_selection(invalid_media_kind) == st2110::Error::InvalidValue);
}

void test_selector_picks_requested_backend_and_media() {
    FakeBackendFactory socket_factory{
        st2110::RxBackendDescriptor{st2110::RxBackendKind::Socket, "socket", combined_capabilities(), true}};

    FakeBackendFactory mtl_factory{
        st2110::RxBackendDescriptor{st2110::RxBackendKind::Mtl, "mtl", video_only_capabilities(), true}};

    std::array<st2110::IRxBackendFactory *, 2> factories{&socket_factory, &mtl_factory};
    const st2110::RxVideoConfig video_cfg = make_valid_video_config();

    auto socket_video = st2110::select_rx_video_backend_factory(
        factories, st2110::RxBackendSelection{st2110::RxBackendKind::Socket, st2110::RxMediaKind::Video}, video_cfg);
    assert(socket_video.has_value());
    assert(*socket_video == &socket_factory);

    auto socket_audio = st2110::select_rx_backend_factory(
        factories, st2110::RxBackendSelection{st2110::RxBackendKind::Socket, st2110::RxMediaKind::Audio});
    assert(socket_audio.has_value());
    assert(*socket_audio == &socket_factory);

    auto mtl_video = st2110::select_rx_video_backend_factory(
        factories, st2110::RxBackendSelection{st2110::RxBackendKind::Mtl, st2110::RxMediaKind::Video}, video_cfg);
    assert(mtl_video.has_value());
    assert(*mtl_video == &mtl_factory);

    auto mtl_audio = st2110::select_rx_backend_factory(
        factories, st2110::RxBackendSelection{st2110::RxBackendKind::Mtl, st2110::RxMediaKind::Audio});
    assert(!mtl_audio.has_value());
    assert(mtl_audio.error() == st2110::Error::Unsupported);
}

void test_selector_rejects_unavailable_backend() {
    FakeBackendFactory unavailable_mtl_factory{
        st2110::RxBackendDescriptor{st2110::RxBackendKind::Mtl, "mtl", video_only_capabilities(), false}};

    std::array<st2110::IRxBackendFactory *, 1> factories{&unavailable_mtl_factory};
    const st2110::RxVideoConfig video_cfg = make_valid_video_config();

    auto selected = st2110::select_rx_video_backend_factory(
        factories, st2110::RxBackendSelection{st2110::RxBackendKind::Mtl, st2110::RxMediaKind::Video}, video_cfg);

    assert(!selected.has_value());
    assert(selected.error() == st2110::Error::Unsupported);
    assert(unavailable_mtl_factory.create_count == 0);
}

void test_selector_rejects_missing_backend_kind() {
    FakeBackendFactory socket_factory{
        st2110::RxBackendDescriptor{st2110::RxBackendKind::Socket, "socket", combined_capabilities(), true}};

    std::array<st2110::IRxBackendFactory *, 1> factories{&socket_factory};
    const st2110::RxVideoConfig video_cfg = make_valid_video_config();

    auto selected = st2110::select_rx_video_backend_factory(
        factories, st2110::RxBackendSelection{st2110::RxBackendKind::Mtl, st2110::RxMediaKind::Video}, video_cfg);

    assert(!selected.has_value());
    assert(selected.error() == st2110::Error::Unsupported);
    assert(socket_factory.create_count == 0);
}

void test_selector_rejects_invalid_factory_entry() {
    FakeBackendFactory socket_factory{
        st2110::RxBackendDescriptor{st2110::RxBackendKind::Socket, "socket", combined_capabilities(), true}};

    std::array<st2110::IRxBackendFactory *, 2> factories{&socket_factory, nullptr};
    const st2110::RxVideoConfig video_cfg = make_valid_video_config();

    auto selected = st2110::select_rx_video_backend_factory(
        factories, st2110::RxBackendSelection{st2110::RxBackendKind::Mtl, st2110::RxMediaKind::Video}, video_cfg);

    assert(!selected.has_value());
    assert(selected.error() == st2110::Error::InvalidValue);
    assert(socket_factory.create_count == 0);
}

void test_video_selector_rejects_socket_unsupported_mode_before_factory_creation() {
    FakeBackendFactory socket_factory{
        st2110::RxBackendDescriptor{st2110::RxBackendKind::Socket, "socket", video_only_capabilities(), true}};

    std::array<st2110::IRxBackendFactory *, 1> factories{&socket_factory};

    st2110::RxVideoConfig cfg = make_valid_video_config();
    cfg.scan_mode = st2110::VideoScanMode::Interlaced;

    auto selected = st2110::select_rx_video_backend_factory(
        factories, st2110::RxBackendSelection{st2110::RxBackendKind::Socket, st2110::RxMediaKind::Video}, cfg);

    assert(!selected.has_value());
    assert(selected.error() == st2110::Error::Unsupported);
    assert(socket_factory.create_count == 0);
}

void test_default_factory_registry_shape() {
    const auto factories = st2110::default_rx_backend_factories();
    const auto factories_again = st2110::default_rx_backend_factories();

    assert(factories.size() == (kExpectMtlBuilt ? 4U : 2U));
    assert(factories.data() == factories_again.data());

    std::size_t socket_video_count = 0;
    std::size_t socket_audio_count = 0;
    std::size_t mtl_video_count = 0;
    std::size_t mtl_audio_count = 0;

    for (st2110::IRxBackendFactory *const factory : factories) {
        assert(factory != nullptr);

        const st2110::RxBackendDescriptor descriptor = factory->descriptor();
        assert(st2110::validate_rx_backend_descriptor(descriptor) == st2110::Error::Ok);
        assert(descriptor.name == st2110::rx_backend_kind_name(descriptor.kind));

        if (descriptor.kind == st2110::RxBackendKind::Socket && descriptor.capabilities.video_rx &&
            !descriptor.capabilities.audio_rx) {
            assert(descriptor.available);
            ++socket_video_count;
            continue;
        }

        if (descriptor.kind == st2110::RxBackendKind::Socket && !descriptor.capabilities.video_rx &&
            descriptor.capabilities.audio_rx) {
            assert(descriptor.available);
            ++socket_audio_count;
            continue;
        }

        if (descriptor.kind == st2110::RxBackendKind::Mtl && descriptor.capabilities.video_rx &&
            !descriptor.capabilities.audio_rx) {
            assert(kExpectMtlBuilt);
            assert(!descriptor.available);
            ++mtl_video_count;
            continue;
        }

        if (descriptor.kind == st2110::RxBackendKind::Mtl && !descriptor.capabilities.video_rx &&
            descriptor.capabilities.audio_rx) {
            assert(kExpectMtlBuilt);
            assert(!descriptor.available);
            ++mtl_audio_count;
            continue;
        }

        assert(false && "unexpected builtin backend factory descriptor");
    }

    assert(socket_video_count == 1);
    assert(socket_audio_count == 1);
    assert(mtl_video_count == (kExpectMtlBuilt ? 1U : 0U));
    assert(mtl_audio_count == (kExpectMtlBuilt ? 1U : 0U));
}

void test_default_factory_registry_selects_socket_backends() {
    const auto factories = st2110::default_rx_backend_factories();
    const st2110::RxVideoConfig video_cfg = make_valid_video_config();

    auto socket_video = st2110::select_rx_video_backend_factory(
        factories, st2110::RxBackendSelection{st2110::RxBackendKind::Socket, st2110::RxMediaKind::Video}, video_cfg);
    assert(socket_video.has_value());
    assert((*socket_video)->descriptor().kind == st2110::RxBackendKind::Socket);
    assert((*socket_video)->descriptor().available);
    assert((*socket_video)->descriptor().capabilities.video_rx);
    assert(!(*socket_video)->descriptor().capabilities.audio_rx);

    auto socket_audio = st2110::select_rx_backend_factory(
        factories, st2110::RxBackendSelection{st2110::RxBackendKind::Socket, st2110::RxMediaKind::Audio});
    assert(socket_audio.has_value());
    assert((*socket_audio)->descriptor().kind == st2110::RxBackendKind::Socket);
    assert((*socket_audio)->descriptor().available);
    assert(!(*socket_audio)->descriptor().capabilities.video_rx);
    assert((*socket_audio)->descriptor().capabilities.audio_rx);

    auto video_backend = st2110::create_rx_video_backend(
        factories, st2110::RxBackendSelection{st2110::RxBackendKind::Socket, st2110::RxMediaKind::Video}, video_cfg);
    assert(video_backend.has_value());
    assert(*video_backend != nullptr);
    assert(std::string_view((*video_backend)->backend_name()) == "socket");
    assert(st2110::supports_media((*video_backend)->capabilities(), st2110::RxMediaKind::Video));
    assert(!st2110::supports_media((*video_backend)->capabilities(), st2110::RxMediaKind::Audio));
    assert(st2110::backend_is_stopped((*video_backend)->state()));

    auto audio_backend = st2110::create_rx_backend(
        factories, st2110::RxBackendSelection{st2110::RxBackendKind::Socket, st2110::RxMediaKind::Audio});
    assert(audio_backend.has_value());
    assert(*audio_backend != nullptr);
    assert(std::string_view((*audio_backend)->backend_name()) == "socket");
    assert(!st2110::supports_media((*audio_backend)->capabilities(), st2110::RxMediaKind::Video));
    assert(st2110::supports_media((*audio_backend)->capabilities(), st2110::RxMediaKind::Audio));
    assert(st2110::backend_is_stopped((*audio_backend)->state()));

    auto stopped_video = (*video_backend)->stop();
    assert(stopped_video.has_value());
    assert(st2110::backend_is_stopped(*stopped_video));

    auto stopped_audio = (*audio_backend)->stop();
    assert(stopped_audio.has_value());
    assert(st2110::backend_is_stopped(*stopped_audio));
}

void test_default_factory_registry_keeps_mtl_public_but_unavailable() {
    const auto factories = st2110::default_rx_backend_factories();
    const st2110::RxVideoConfig video_cfg = make_valid_video_config();

    const bool mtl_built = st2110::rx_backend_kind_built(st2110::RxBackendKind::Mtl);
    assert(mtl_built == kExpectMtlBuilt);

    auto select_video = st2110::select_rx_video_backend_factory(
        factories, st2110::RxBackendSelection{st2110::RxBackendKind::Mtl, st2110::RxMediaKind::Video}, video_cfg);
    assert(!select_video.has_value());
    assert(select_video.error() == st2110::Error::Unsupported);

    auto select_audio = st2110::select_rx_backend_factory(
        factories, st2110::RxBackendSelection{st2110::RxBackendKind::Mtl, st2110::RxMediaKind::Audio});
    assert(!select_audio.has_value());
    assert(select_audio.error() == st2110::Error::Unsupported);

    auto create_video = st2110::create_rx_video_backend(
        factories, st2110::RxBackendSelection{st2110::RxBackendKind::Mtl, st2110::RxMediaKind::Video}, video_cfg);
    assert(!create_video.has_value());
    assert(create_video.error() == st2110::Error::Unsupported);

    auto create_audio = st2110::create_rx_backend(
        factories, st2110::RxBackendSelection{st2110::RxBackendKind::Mtl, st2110::RxMediaKind::Audio});
    assert(!create_audio.has_value());
    assert(create_audio.error() == st2110::Error::Unsupported);
}

#if ST2110_TEST_EXPECT_MTL_BUILT
void test_direct_mtl_video_factory_reports_runtime_unavailable_until_start_stop_is_implemented() {
    st2110::MtlRxVideoBackendFactory factory{};

    const st2110::RxBackendDescriptor descriptor = factory.descriptor();
    assert(descriptor.kind == st2110::RxBackendKind::Mtl);
    assert(descriptor.name == std::string_view{"mtl"});
    assert(descriptor.capabilities.video_rx);
    assert(!descriptor.capabilities.audio_rx);
    assert(!descriptor.available);

    auto backend = factory.create_backend();
    assert(backend == nullptr);
}
#endif

#if ST2110_TEST_EXPECT_MTL_BUILT
void test_direct_mtl_video_backend_exposes_basic_skeleton_contract() {
    st2110::MtlRxVideoBackend backend{};

    assert(std::string_view(backend.backend_name()) == "mtl");
    assert(st2110::supports_media(backend.capabilities(), st2110::RxMediaKind::Video));
    assert(!st2110::supports_media(backend.capabilities(), st2110::RxMediaKind::Audio));
    assert(st2110::backend_is_stopped(backend.state()));
    assert_zero_backend_stats(backend.stats());

    auto stopped_once = backend.stop();
    assert(stopped_once.has_value());
    assert(st2110::backend_is_stopped(*stopped_once));

    auto stopped_twice = backend.stop();
    assert(stopped_twice.has_value());
    assert(st2110::backend_is_stopped(*stopped_twice));
}

void test_direct_mtl_video_backend_supported_mvp_start_is_explicitly_unsupported() {
    st2110::MtlRxVideoBackend backend{};
    FakeVideoSink sink{};
    const st2110::RxVideoConfig cfg = make_valid_video_config();

    auto started = backend.start_video(cfg, sink);
    assert(!started.has_value());
    assert(started.error() == st2110::Error::Unsupported);

    assert(st2110::backend_is_stopped(backend.state()));
    assert_zero_backend_stats(backend.stats());
    assert(sink.frames_received == 0);

    auto stopped = backend.stop();
    assert(stopped.has_value());
    assert(st2110::backend_is_stopped(*stopped));
}

void test_direct_mtl_video_backend_rejects_unsupported_scan_mode() {
    st2110::MtlRxVideoBackend backend{};
    FakeVideoSink sink{};
    st2110::RxVideoConfig cfg = make_valid_video_config();
    cfg.scan_mode = st2110::VideoScanMode::Interlaced;

    auto started = backend.start_video(cfg, sink);
    assert(!started.has_value());
    assert(started.error() == st2110::Error::Unsupported);

    assert(st2110::backend_is_stopped(backend.state()));
    assert_zero_backend_stats(backend.stats());
    assert(sink.frames_received == 0);
}

void test_direct_mtl_video_backend_rejects_unsupported_packing_mode() {
    st2110::MtlRxVideoBackend backend{};
    FakeVideoSink sink{};
    st2110::RxVideoConfig cfg = make_valid_video_config();
    cfg.packing_mode = st2110::VideoPackingMode::Bpm;

    auto started = backend.start_video(cfg, sink);
    assert(!started.has_value());
    assert(started.error() == st2110::Error::Unsupported);

    assert(st2110::backend_is_stopped(backend.state()));
    assert_zero_backend_stats(backend.stats());
    assert(sink.frames_received == 0);
}

void test_direct_mtl_video_backend_rejects_invalid_runtime_config() {
    st2110::MtlRxVideoBackend backend{};
    FakeVideoSink sink{};
    st2110::RxVideoConfig cfg = make_valid_video_config();
    cfg.payload_type = 95;

    auto started = backend.start_video(cfg, sink);
    assert(!started.has_value());
    assert(started.error() == st2110::Error::InvalidValue);

    assert(st2110::backend_is_stopped(backend.state()));
    assert_zero_backend_stats(backend.stats());
    assert(sink.frames_received == 0);
}
#endif

void test_create_backend_uses_selected_factory() {
    FakeBackendFactory socket_factory{
        st2110::RxBackendDescriptor{st2110::RxBackendKind::Socket, "socket", combined_capabilities(), true}};

    FakeBackendFactory mtl_factory{
        st2110::RxBackendDescriptor{st2110::RxBackendKind::Mtl, "mtl", video_only_capabilities(), true}};

    std::array<st2110::IRxBackendFactory *, 2> factories{&socket_factory, &mtl_factory};
    const st2110::RxVideoConfig video_cfg = make_valid_video_config();

    auto backend = st2110::create_rx_video_backend(
        factories, st2110::RxBackendSelection{st2110::RxBackendKind::Mtl, st2110::RxMediaKind::Video}, video_cfg);

    assert(backend.has_value());
    assert(*backend != nullptr);
    assert(std::string_view((*backend)->backend_name()) == "mtl");
    assert(st2110::supports_media((*backend)->capabilities(), st2110::RxMediaKind::Video));
    assert(!st2110::supports_media((*backend)->capabilities(), st2110::RxMediaKind::Audio));
    assert(st2110::backend_is_stopped((*backend)->state()));

    assert_zero_backend_stats((*backend)->stats());

    assert(socket_factory.create_count == 0);
    assert(mtl_factory.create_count == 1);

    auto stopped = (*backend)->stop();
    assert(stopped.has_value());
    assert(st2110::backend_is_stopped(*stopped));
}

void test_create_backend_rejects_null_factory_result() {
    FakeBackendFactory null_socket_factory{
        st2110::RxBackendDescriptor{st2110::RxBackendKind::Socket, "socket", combined_capabilities(), true}, true};

    std::array<st2110::IRxBackendFactory *, 1> factories{&null_socket_factory};
    const st2110::RxVideoConfig video_cfg = make_valid_video_config();

    auto backend = st2110::create_rx_video_backend(
        factories, st2110::RxBackendSelection{st2110::RxBackendKind::Socket, st2110::RxMediaKind::Video}, video_cfg);

    assert(!backend.has_value());
    assert(backend.error() == st2110::Error::InvalidValue);
    assert(null_socket_factory.create_count == 1);
}
} // namespace

int main() {
    test_backend_kind_validation_and_string_mapping();
    test_backend_kind_built_helper();
    test_descriptor_validation();
    test_selection_validation();
    test_selector_picks_requested_backend_and_media();
    test_selector_rejects_unavailable_backend();
    test_selector_rejects_missing_backend_kind();
    test_selector_rejects_invalid_factory_entry();
    test_video_selector_rejects_socket_unsupported_mode_before_factory_creation();
    test_default_factory_registry_shape();
    test_default_factory_registry_selects_socket_backends();
    test_default_factory_registry_keeps_mtl_public_but_unavailable();
#if ST2110_TEST_EXPECT_MTL_BUILT
    test_direct_mtl_video_factory_reports_runtime_unavailable_until_start_stop_is_implemented();
    test_direct_mtl_video_backend_exposes_basic_skeleton_contract();
    test_direct_mtl_video_backend_supported_mvp_start_is_explicitly_unsupported();
    test_direct_mtl_video_backend_rejects_unsupported_scan_mode();
    test_direct_mtl_video_backend_rejects_unsupported_packing_mode();
    test_direct_mtl_video_backend_rejects_invalid_runtime_config();
#endif
    test_create_backend_uses_selected_factory();
    test_create_backend_rejects_null_factory_result();
    return 0;
}
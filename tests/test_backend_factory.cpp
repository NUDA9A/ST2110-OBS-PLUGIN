#include <array>
#include <cassert>
#include <expected>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

#include <st2110/backend.hpp>
#include <st2110/backend_factory.hpp>
#include <st2110/error.hpp>

static_assert(std::is_enum_v<st2110::RxBackendKind>);
static_assert(!std::is_convertible_v<st2110::RxBackendKind, int>);

static_assert(std::is_same_v<decltype(st2110::validate_rx_backend_kind(st2110::RxBackendKind::Socket)), st2110::Error>);

static_assert(std::is_same_v<decltype(st2110::rx_backend_kind_name(st2110::RxBackendKind::Socket)), std::string_view>);

static_assert(std::is_same_v<decltype(st2110::parse_rx_backend_kind(std::declval<std::string_view>())),
                             std::expected<st2110::RxBackendKind, st2110::Error>>);

static_assert(std::is_same_v<
              decltype(st2110::validate_rx_backend_descriptor(std::declval<const st2110::RxBackendDescriptor &>())),
              st2110::Error>);

static_assert(
    std::is_same_v<decltype(st2110::validate_rx_backend_selection(std::declval<const st2110::RxBackendSelection &>())),
                   st2110::Error>);

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

namespace {
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

    auto socket_video = st2110::select_rx_backend_factory(
        factories, st2110::RxBackendSelection{st2110::RxBackendKind::Socket, st2110::RxMediaKind::Video});
    assert(socket_video.has_value());
    assert(*socket_video == &socket_factory);

    auto socket_audio = st2110::select_rx_backend_factory(
        factories, st2110::RxBackendSelection{st2110::RxBackendKind::Socket, st2110::RxMediaKind::Audio});
    assert(socket_audio.has_value());
    assert(*socket_audio == &socket_factory);

    auto mtl_video = st2110::select_rx_backend_factory(
        factories, st2110::RxBackendSelection{st2110::RxBackendKind::Mtl, st2110::RxMediaKind::Video});
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

    auto selected = st2110::select_rx_backend_factory(
        factories, st2110::RxBackendSelection{st2110::RxBackendKind::Mtl, st2110::RxMediaKind::Video});

    assert(!selected.has_value());
    assert(selected.error() == st2110::Error::Unsupported);
    assert(unavailable_mtl_factory.create_count == 0);
}

void test_selector_rejects_missing_backend_kind() {
    FakeBackendFactory socket_factory{
        st2110::RxBackendDescriptor{st2110::RxBackendKind::Socket, "socket", combined_capabilities(), true}};

    std::array<st2110::IRxBackendFactory *, 1> factories{&socket_factory};

    auto selected = st2110::select_rx_backend_factory(
        factories, st2110::RxBackendSelection{st2110::RxBackendKind::Mtl, st2110::RxMediaKind::Video});

    assert(!selected.has_value());
    assert(selected.error() == st2110::Error::Unsupported);
    assert(socket_factory.create_count == 0);
}

void test_selector_rejects_invalid_factory_entry() {
    FakeBackendFactory socket_factory{
        st2110::RxBackendDescriptor{st2110::RxBackendKind::Socket, "socket", combined_capabilities(), true}};

    std::array<st2110::IRxBackendFactory *, 2> factories{&socket_factory, nullptr};

    auto selected = st2110::select_rx_backend_factory(
        factories, st2110::RxBackendSelection{st2110::RxBackendKind::Mtl, st2110::RxMediaKind::Video});

    assert(!selected.has_value());
    assert(selected.error() == st2110::Error::InvalidValue);
    assert(socket_factory.create_count == 0);
}

void test_create_backend_uses_selected_factory() {
    FakeBackendFactory socket_factory{
        st2110::RxBackendDescriptor{st2110::RxBackendKind::Socket, "socket", combined_capabilities(), true}};

    FakeBackendFactory mtl_factory{
        st2110::RxBackendDescriptor{st2110::RxBackendKind::Mtl, "mtl", video_only_capabilities(), true}};

    std::array<st2110::IRxBackendFactory *, 2> factories{&socket_factory, &mtl_factory};

    auto backend = st2110::create_rx_backend(
        factories, st2110::RxBackendSelection{st2110::RxBackendKind::Mtl, st2110::RxMediaKind::Video});

    assert(backend.has_value());
    assert(*backend != nullptr);
    assert(std::string_view((*backend)->backend_name()) == "mtl");
    assert(st2110::supports_media((*backend)->capabilities(), st2110::RxMediaKind::Video));
    assert(!st2110::supports_media((*backend)->capabilities(), st2110::RxMediaKind::Audio));
    assert(st2110::backend_is_stopped((*backend)->state()));

    const auto stats = (*backend)->stats();
    assert(stats.datagrams_received == 0);
    assert(stats.bytes_received == 0);
    assert(stats.control_datagrams_ignored == 0);
    assert(stats.nonmedia_datagrams_ignored == 0);
    assert(stats.packets_parsed_ok == 0);
    assert(stats.packets_rejected == 0);
    assert(stats.datagrams_dropped == 0);
    assert(stats.frames_delivered == 0);
    assert(stats.media_units_delivered == 0);

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

    auto backend = st2110::create_rx_backend(
        factories, st2110::RxBackendSelection{st2110::RxBackendKind::Socket, st2110::RxMediaKind::Video});

    assert(!backend.has_value());
    assert(backend.error() == st2110::Error::InvalidValue);
    assert(null_socket_factory.create_count == 1);
}
} // namespace

int main() {
    test_backend_kind_validation_and_string_mapping();
    test_descriptor_validation();
    test_selection_validation();
    test_selector_picks_requested_backend_and_media();
    test_selector_rejects_unavailable_backend();
    test_selector_rejects_missing_backend_kind();
    test_selector_rejects_invalid_factory_entry();
    test_create_backend_uses_selected_factory();
    test_create_backend_rejects_null_factory_result();
    return 0;
}
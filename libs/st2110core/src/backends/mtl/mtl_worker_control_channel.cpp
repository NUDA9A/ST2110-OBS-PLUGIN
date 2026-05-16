#include <st2110/backends/mtl/mtl_worker_control_channel.hpp>

#include <utility>

namespace st2110 {
namespace {

class UnsupportedMtlWorkerControlChannel final : public IMtlWorkerControlChannel {
  public:
    [[nodiscard]] std::expected<MtlWorkerControlEventEnvelope, Error>
    transact_with_fds(const MtlWorkerControlRequest &request, std::span<const int> file_descriptors) override {
        (void)request;
        (void)file_descriptors;
        return std::unexpected(Error::Unsupported);
    }

    [[nodiscard]] std::expected<bool, Error> register_async_event_handler(MtlWorkerGraphId graph_id,
                                                                          MtlWorkerAsyncEventHandler handler) override {
        (void)graph_id;
        (void)handler;
        return std::unexpected(Error::Unsupported);
    }

    void unregister_async_event_handler_noexcept(MtlWorkerGraphId graph_id) noexcept override { (void)graph_id; }

    [[nodiscard]] bool healthy() const noexcept override { return false; }
};

} // namespace

std::expected<MtlWorkerControlEvent, Error> IMtlWorkerControlChannel::transact(const MtlWorkerControlRequest &request) {
    auto envelope = transact_with_fds(request, {});
    if (!envelope.has_value()) {
        return std::unexpected(envelope.error());
    }

    if (envelope->has_file_descriptors()) {
        return std::unexpected(Error::InvalidValue);
    }

    return std::move(envelope->event);
}

std::shared_ptr<IMtlWorkerControlChannel> create_unsupported_mtl_worker_control_channel() {
    return std::make_shared<UnsupportedMtlWorkerControlChannel>();
}

} // namespace st2110
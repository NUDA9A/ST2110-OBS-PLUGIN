#include <st2110/backends/mtl/mtl_worker_control_channel.hpp>

namespace st2110 {
namespace {

class UnsupportedMtlWorkerControlChannel final : public IMtlWorkerControlChannel {
public:
    [[nodiscard]] std::expected<MtlWorkerControlEvent, Error>
    transact(const MtlWorkerControlRequest &request) override {
        (void)request;
        return std::unexpected(Error::Unsupported);
    }
};

} // namespace

std::shared_ptr<IMtlWorkerControlChannel> create_unsupported_mtl_worker_control_channel() {
    return std::make_shared<UnsupportedMtlWorkerControlChannel>();
}

} // namespace st2110
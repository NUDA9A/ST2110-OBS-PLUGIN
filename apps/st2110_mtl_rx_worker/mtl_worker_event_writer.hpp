#ifndef ST2110_OBS_PLUGIN_MTL_RX_WORKER_EVENT_WRITER_HPP
#define ST2110_OBS_PLUGIN_MTL_RX_WORKER_EVENT_WRITER_HPP

#include <st2110/backends/mtl/mtl_worker_protocol.hpp>
#include <st2110/foundation/error.hpp>

#include <expected>
#include <mutex>
#include <span>

namespace st2110_mtl_rx_worker {
class MtlWorkerEventWriter final {
  public:
    explicit MtlWorkerEventWriter(int fd) noexcept;

    MtlWorkerEventWriter(const MtlWorkerEventWriter &) = delete;
    MtlWorkerEventWriter &operator=(const MtlWorkerEventWriter &) = delete;

    MtlWorkerEventWriter(MtlWorkerEventWriter &&) noexcept = delete;
    MtlWorkerEventWriter &operator=(MtlWorkerEventWriter &&) noexcept = delete;

    [[nodiscard]] std::expected<bool, st2110::Error> write_event(const st2110::MtlWorkerControlEvent &event);

    [[nodiscard]] std::expected<bool, st2110::Error> write_event_with_fds(const st2110::MtlWorkerControlEvent &event,
                                                                          std::span<const int> file_descriptors);

    [[nodiscard]] int fd() const noexcept;

  private:
    int fd_ = -1;
    std::mutex write_mutex_{};
};

} // namespace st2110_mtl_rx_worker

#endif // ST2110_OBS_PLUGIN_MTL_RX_WORKER_EVENT_WRITER_HPP
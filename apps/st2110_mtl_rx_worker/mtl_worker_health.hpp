#ifndef ST2110_OBS_PLUGIN_MTL_RX_WORKER_HEALTH_HPP
#define ST2110_OBS_PLUGIN_MTL_RX_WORKER_HEALTH_HPP

#include <st2110/foundation/error.hpp>

#include <mutex>
#include <string>
#include <utility>

namespace st2110_mtl_rx_worker {

class MtlWorkerHealthState final {
  public:
    void mark_unhealthy(st2110::Error error, std::string message) noexcept {
        try {
            std::lock_guard lock(mutex_);

            if (!healthy_) {
                return;
            }

            healthy_ = false;
            error_ = error == st2110::Error::Ok ? st2110::Error::InvalidBackendState : error;
            message_ = std::move(message);
        } catch (...) {
        }
    }

    [[nodiscard]] bool healthy() const noexcept {
        try {
            std::lock_guard lock(mutex_);
            return healthy_;
        } catch (...) {
            return false;
        }
    }

    [[nodiscard]] st2110::Error error() const noexcept {
        try {
            std::lock_guard lock(mutex_);
            return error_;
        } catch (...) {
            return st2110::Error::InvalidBackendState;
        }
    }

    [[nodiscard]] std::string message() const {
        std::lock_guard lock(mutex_);
        return message_;
    }

  private:
    mutable std::mutex mutex_{};
    bool healthy_ = true;
    st2110::Error error_ = st2110::Error::Ok;
    std::string message_ = "MTL worker component healthy";
};

} // namespace st2110_mtl_rx_worker

#endif // ST2110_OBS_PLUGIN_MTL_RX_WORKER_HEALTH_HPP
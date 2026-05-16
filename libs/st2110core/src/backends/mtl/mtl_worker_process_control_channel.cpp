#include <st2110/backends/mtl/mtl_worker_ipc_codec.hpp>
#include <st2110/backends/mtl/mtl_worker_ipc_framing.hpp>
#include <st2110/backends/mtl/mtl_worker_process_control_channel.hpp>

#include <csignal>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>

namespace st2110 {
namespace {

using namespace std::chrono_literals;

inline constexpr auto mtlWorkerConfigHandshakeTimeout = 30s;
inline constexpr auto mtlWorkerStartSessionsTimeout = 30s;
inline constexpr auto mtlWorkerStopSessionsTimeout = 5s;
inline constexpr auto mtlWorkerStatsTimeout = 2s;
inline constexpr auto mtlWorkerHealthCheckTimeout = 2s;
inline constexpr auto mtlWorkerShutdownTimeout = 5s;
inline constexpr auto mtlWorkerSocketSendTimeout = 5s;
inline constexpr auto mtlWorkerTerminateGraceTimeout = 1s;
inline constexpr auto mtlWorkerKillGraceTimeout = 2s;

[[nodiscard]] std::chrono::milliseconds timeout_for_request(const MtlWorkerControlRequest &request) noexcept {
    return std::visit(
        [](const auto &typed_request) -> std::chrono::milliseconds {
            using Request = std::decay_t<decltype(typed_request)>;

            if constexpr (std::is_same_v<Request, MtlWorkerConfigHandshakeRequest>) {
                return std::chrono::duration_cast<std::chrono::milliseconds>(mtlWorkerConfigHandshakeTimeout);
            } else if constexpr (std::is_same_v<Request, MtlWorkerStartSessionsRequest>) {
                return std::chrono::duration_cast<std::chrono::milliseconds>(mtlWorkerStartSessionsTimeout);
            } else if constexpr (std::is_same_v<Request, MtlWorkerStopSessionsRequest>) {
                return std::chrono::duration_cast<std::chrono::milliseconds>(mtlWorkerStopSessionsTimeout);
            } else if constexpr (std::is_same_v<Request, MtlWorkerStatsRequest>) {
                return std::chrono::duration_cast<std::chrono::milliseconds>(mtlWorkerStatsTimeout);
            } else if constexpr (std::is_same_v<Request, MtlWorkerHealthCheckRequest>) {
                return std::chrono::duration_cast<std::chrono::milliseconds>(mtlWorkerHealthCheckTimeout);
            } else if constexpr (std::is_same_v<Request, MtlWorkerShutdownRequest>) {
                return std::chrono::duration_cast<std::chrono::milliseconds>(mtlWorkerShutdownTimeout);
            } else {
                return 5s;
            }
        },
        request);
}

[[nodiscard]] std::expected<bool, Error>
configure_socket_send_timeout(const int fd, const std::chrono::milliseconds timeout) noexcept {
    if (fd < 0 || timeout.count() <= 0) {
        return std::unexpected(Error::InvalidValue);
    }

    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(timeout);
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(timeout - seconds);

    timeval value{};
    value.tv_sec = static_cast<time_t>(seconds.count());
    value.tv_usec = static_cast<suseconds_t>(micros.count());

    if (::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &value, sizeof(value)) != 0) {
        return std::unexpected(Error::SystemFailure);
    }

    return true;
}

void close_fd_noexcept(int &fd) noexcept {
    if (fd >= 0) {
        (void)::close(fd);
        fd = -1;
    }
}

void shutdown_fd_noexcept(int &fd) noexcept {
    if (fd >= 0) {
        (void)::shutdown(fd, SHUT_RDWR);
        (void)::close(fd);
        fd = -1;
    }
}

[[nodiscard]] bool wait_process_exit_noexcept(const pid_t pid, const std::chrono::milliseconds timeout) noexcept {
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    for (;;) {
        int status = 0;
        const pid_t rc = ::waitpid(pid, &status, WNOHANG);

        if (rc == pid) {
            return true;
        }

        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }

            return true;
        }

        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }

        std::this_thread::sleep_for(50ms);
    }
}

void terminate_process_noexcept(pid_t &pid) noexcept {
    if (pid <= 0) {
        pid = -1;
        return;
    }

    (void)::kill(pid, SIGTERM);

    if (!wait_process_exit_noexcept(
            pid, std::chrono::duration_cast<std::chrono::milliseconds>(mtlWorkerTerminateGraceTimeout))) {
        (void)::kill(pid, SIGKILL);
        (void)wait_process_exit_noexcept(
            pid, std::chrono::duration_cast<std::chrono::milliseconds>(mtlWorkerKillGraceTimeout));
    }

    /*
     * Do not block indefinitely here. If the child is stuck in an uninterruptible
     * kernel state, the parent must still be able to invalidate the channel and
     * continue.
     */
    pid = -1;
}

[[nodiscard]] MtlWorkerRequestId request_id_from_request(const MtlWorkerControlRequest &request) noexcept {
    return std::visit([](const auto &typed_request) noexcept -> MtlWorkerRequestId { return typed_request.request_id; },
                      request);
}

[[nodiscard]] MtlWorkerRequestId request_id_from_event(const MtlWorkerControlEvent &event) noexcept {
    return std::visit(
        [](const auto &typed_event) noexcept -> MtlWorkerRequestId {
            using Event = std::decay_t<decltype(typed_event)>;

            if constexpr (std::is_same_v<Event, MtlWorkerFrameReadyEvent> ||
                          std::is_same_v<Event, MtlWorkerAudioBlockReadyEvent>) {
                return 0;
            } else {
                return typed_event.request_id;
            }
        },
        event);
}

[[nodiscard]] MtlWorkerGraphId graph_id_from_async_event(const MtlWorkerControlEvent &event) noexcept {
    return std::visit(
        [](const auto &typed_event) noexcept -> MtlWorkerGraphId {
            using Event = std::decay_t<decltype(typed_event)>;

            if constexpr (std::is_same_v<Event, MtlWorkerFrameReadyEvent> ||
                          std::is_same_v<Event, MtlWorkerAudioBlockReadyEvent>) {
                return typed_event.graph_id;
            } else {
                return 0;
            }
        },
        event);
}

} // namespace

struct MtlWorkerProcessControlChannel::Impl {
    std::filesystem::path worker_executable_path{};

    pid_t worker_pid = -1;
    int worker_control_fd = -1;

    std::thread reader_thread{};

    std::mutex write_mutex{};
    std::mutex state_mutex{};
    std::condition_variable state_cv{};

    std::unordered_map<MtlWorkerRequestId, MtlWorkerControlEventEnvelope> completed_responses{};
    std::unordered_map<MtlWorkerGraphId, MtlWorkerAsyncEventHandler> async_event_handlers{};

    bool reader_failed = false;
    Error reader_error = Error::Ok;
    bool shutdown_requested = false;

    explicit Impl(std::filesystem::path path) : worker_executable_path(std::move(path)) {}

    ~Impl() { shutdown_noexcept(); }

    [[nodiscard]] bool worker_started() const noexcept { return worker_pid > 0 && worker_control_fd >= 0; }

    [[nodiscard]] std::expected<bool, Error> ensure_worker_started() {
        {
            std::lock_guard lock(state_mutex);
            if (shutdown_requested) {
                return std::unexpected(Error::OperationAborted);
            }

            if (reader_failed) {
                return std::unexpected(reader_error == Error::Ok ? Error::OperationAborted : reader_error);
            }
        }

        if (worker_started()) {
            return true;
        }

        int control_socket[2] = {-1, -1};

        if (::socketpair(AF_UNIX, SOCK_STREAM, 0, control_socket) != 0) {
            return std::unexpected(Error::SystemFailure);
        }

        const pid_t pid = ::fork();
        if (pid < 0) {
            close_fd_noexcept(control_socket[0]);
            close_fd_noexcept(control_socket[1]);
            return std::unexpected(Error::SystemFailure);
        }

        if (pid == 0) {
            /*
             * Child process:
             * - stdin  reads framed requests from OBS-process socket;
             * - stdout writes framed events/responses to OBS-process socket.
             *
             * stderr remains diagnostic-only.
             */
            if (::dup2(control_socket[1], STDIN_FILENO) < 0) {
                _exit(127);
            }

            if (::dup2(control_socket[1], STDOUT_FILENO) < 0) {
                _exit(127);
            }

            close_fd_noexcept(control_socket[0]);
            close_fd_noexcept(control_socket[1]);

            const std::string executable = worker_executable_path.string();
            ::execlp(executable.c_str(), executable.c_str(), static_cast<char *>(nullptr));

            _exit(127);
        }

        close_fd_noexcept(control_socket[1]);

        worker_pid = pid;
        worker_control_fd = control_socket[0];

        auto send_timeout = configure_socket_send_timeout(
            worker_control_fd, std::chrono::duration_cast<std::chrono::milliseconds>(mtlWorkerSocketSendTimeout));
        if (!send_timeout.has_value()) {
            shutdown_fd_noexcept(worker_control_fd);
            terminate_process_noexcept(worker_pid);
            return std::unexpected(send_timeout.error());
        }

        reader_thread = std::thread([this, fd = worker_control_fd]() { reader_loop_noexcept(fd); });

        return true;
    }

    void reader_loop_noexcept(const int fd) noexcept {
        for (;;) {
            auto frame = read_mtl_worker_control_frame_with_fds(fd);
            if (!frame.has_value()) {
                notify_reader_failed(frame.error());
                return;
            }

            auto event = deserialize_mtl_worker_control_event(frame->payload());
            if (!event.has_value()) {
                notify_reader_failed(event.error());
                return;
            }

            const MtlWorkerRequestId request_id = request_id_from_event(*event);

            MtlWorkerControlEventEnvelope envelope{std::move(*event), std::move(*frame)};

            if (request_id == 0) {
                dispatch_async_event_noexcept(std::move(envelope));
                continue;
            }

            {
                std::lock_guard lock(state_mutex);

                if (shutdown_requested || reader_failed) {
                    return;
                }

                completed_responses.insert_or_assign(request_id, std::move(envelope));
            }

            state_cv.notify_all();
        }
    }

    void dispatch_async_event_noexcept(MtlWorkerControlEventEnvelope envelope) noexcept {
        const MtlWorkerGraphId graph_id = graph_id_from_async_event(envelope.event);
        if (graph_id == 0) {
            return;
        }

        MtlWorkerAsyncEventHandler handler{};

        {
            std::lock_guard lock(state_mutex);

            if (shutdown_requested || reader_failed) {
                return;
            }

            auto found = async_event_handlers.find(graph_id);
            if (found == async_event_handlers.end()) {
                return;
            }

            handler = found->second;
        }

        if (!handler) {
            return;
        }

        try {
            handler(std::move(envelope));
        } catch (...) {
            /*
             * The reader thread must remain alive even if a graph-level async event
             * handler fails. Future higher-level delivery code should convert
             * handler-side failures into graph-local stats/errors instead of
             * throwing through this boundary.
             */
        }
    }

    void mark_channel_failed_no_lock(const Error error) noexcept {
        if (reader_failed) {
            return;
        }

        reader_error = error == Error::Ok ? Error::OperationAborted : error;
        reader_failed = true;
    }

    void fail_channel_noexcept(const Error error) noexcept {
        {
            std::lock_guard lock(state_mutex);
            mark_channel_failed_no_lock(error);
        }

        state_cv.notify_all();
    }

    void notify_reader_failed(const Error error) noexcept {
        {
            std::lock_guard lock(state_mutex);

            if (shutdown_requested) {
                mark_channel_failed_no_lock(Error::OperationAborted);
            } else {
                mark_channel_failed_no_lock(error);
            }
        }

        state_cv.notify_all();
    }

    [[nodiscard]] std::expected<MtlWorkerControlEventEnvelope, Error>
    wait_for_response(const MtlWorkerRequestId request_id, const std::chrono::milliseconds timeout) {
        if (timeout.count() <= 0) {
            return std::unexpected(Error::InvalidValue);
        }

        std::unique_lock lock(state_mutex);

        const bool ready = state_cv.wait_for(lock, timeout, [this, request_id]() {
            return completed_responses.contains(request_id) || reader_failed || shutdown_requested;
        });

        if (!ready) {
            completed_responses.erase(request_id);
            mark_channel_failed_no_lock(Error::OperationAborted);

            lock.unlock();
            state_cv.notify_all();

            return std::unexpected(Error::OperationAborted);
        }

        auto found = completed_responses.find(request_id);
        if (found != completed_responses.end()) {
            MtlWorkerControlEventEnvelope envelope = std::move(found->second);
            completed_responses.erase(found);
            return std::move(envelope);
        }

        if (reader_failed) {
            return std::unexpected(reader_error == Error::Ok ? Error::OperationAborted : reader_error);
        }

        return std::unexpected(Error::OperationAborted);
    }

    void shutdown_noexcept() noexcept {
        {
            std::lock_guard lock(state_mutex);
            shutdown_requested = true;
        }

        state_cv.notify_all();

        shutdown_fd_noexcept(worker_control_fd);

        if (reader_thread.joinable()) {
            reader_thread.join();
        }

        terminate_process_noexcept(worker_pid);

        {
            std::lock_guard lock(state_mutex);
            completed_responses.clear();
            async_event_handlers.clear();
            reader_failed = true;
            reader_error = Error::OperationAborted;
        }

        state_cv.notify_all();
    }
};

MtlWorkerProcessControlChannel::MtlWorkerProcessControlChannel(std::filesystem::path worker_executable_path)
    : impl_(std::make_unique<Impl>(std::move(worker_executable_path))) {}

MtlWorkerProcessControlChannel::~MtlWorkerProcessControlChannel() { shutdown_noexcept(); }

std::expected<MtlWorkerControlEventEnvelope, Error>
MtlWorkerProcessControlChannel::transact_with_fds(const MtlWorkerControlRequest &request,
                                                  std::span<const int> file_descriptors) {
    auto started = impl_->ensure_worker_started();
    if (!started.has_value()) {
        return std::unexpected(started.error());
    }

    const MtlWorkerRequestId request_id = request_id_from_request(request);
    if (request_id == 0) {
        return std::unexpected(Error::InvalidValue);
    }

    auto request_payload = serialize_mtl_worker_control_request(request);
    if (!request_payload.has_value()) {
        return std::unexpected(request_payload.error());
    }

    {
        std::lock_guard write_lock(impl_->write_mutex);

        auto wrote =
            write_mtl_worker_control_frame_with_fds(impl_->worker_control_fd, *request_payload, file_descriptors);
        if (!wrote.has_value()) {
            impl_->fail_channel_noexcept(wrote.error());
            return std::unexpected(wrote.error());
        }
    }

    return impl_->wait_for_response(request_id, timeout_for_request(request));
}

std::expected<bool, Error>
MtlWorkerProcessControlChannel::register_async_event_handler(MtlWorkerGraphId graph_id,
                                                             MtlWorkerAsyncEventHandler handler) {
    if (graph_id == 0 || !handler) {
        return std::unexpected(Error::InvalidValue);
    }

    std::lock_guard lock(impl_->state_mutex);

    if (impl_->shutdown_requested) {
        return std::unexpected(Error::OperationAborted);
    }

    if (impl_->reader_failed) {
        return std::unexpected(impl_->reader_error == Error::Ok ? Error::OperationAborted : impl_->reader_error);
    }

    impl_->async_event_handlers.insert_or_assign(graph_id, std::move(handler));
    return true;
}

void MtlWorkerProcessControlChannel::unregister_async_event_handler_noexcept(MtlWorkerGraphId graph_id) noexcept {
    if (graph_id == 0) {
        return;
    }

    try {
        std::lock_guard lock(impl_->state_mutex);
        impl_->async_event_handlers.erase(graph_id);
    } catch (...) {
        /*
         * noexcept boundary.
         */
    }
}

void MtlWorkerProcessControlChannel::shutdown_noexcept() noexcept { impl_->shutdown_noexcept(); }

const std::filesystem::path &MtlWorkerProcessControlChannel::worker_executable_path() const noexcept {
    return impl_->worker_executable_path;
}

std::shared_ptr<IMtlWorkerControlChannel>
create_mtl_worker_process_control_channel(std::filesystem::path worker_executable_path) {
    return std::make_shared<MtlWorkerProcessControlChannel>(std::move(worker_executable_path));
}

bool MtlWorkerProcessControlChannel::healthy() const noexcept {
    try {
        std::lock_guard lock(impl_->state_mutex);
        return !impl_->shutdown_requested && !impl_->reader_failed && impl_->worker_pid > 0 &&
               impl_->worker_control_fd >= 0;
    } catch (...) {
        return false;
    }
}

} // namespace st2110
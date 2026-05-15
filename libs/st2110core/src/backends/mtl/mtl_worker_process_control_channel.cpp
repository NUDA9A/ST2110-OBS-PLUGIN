#include <st2110/backends/mtl/mtl_worker_ipc_codec.hpp>
#include <st2110/backends/mtl/mtl_worker_ipc_framing.hpp>
#include <st2110/backends/mtl/mtl_worker_process_control_channel.hpp>

#include <csignal>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
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

void terminate_process_noexcept(pid_t &pid) noexcept {
    if (pid <= 0) {
        pid = -1;
        return;
    }

    (void)::kill(pid, SIGTERM);

    int status = 0;
    while (::waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            break;
        }
    }

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
                /*
                 * Future async media/event route:
                 *
                 * - FrameReady
                 * - AudioBlockReady
                 * - worker health notifications not tied to transact()
                 *
                 * For now, control-plane transact() responses are the only
                 * consumed events. Any descriptors attached to an unhandled async
                 * event are closed by envelope destruction here.
                 */
                continue;
            }

            {
                std::lock_guard lock(state_mutex);
                completed_responses.insert_or_assign(request_id, std::move(envelope));
            }

            state_cv.notify_all();
        }
    }

    void notify_reader_failed(const Error error) noexcept {
        {
            std::lock_guard lock(state_mutex);

            if (shutdown_requested) {
                reader_error = Error::OperationAborted;
            } else {
                reader_error = error;
            }

            reader_failed = true;
        }

        state_cv.notify_all();
    }

    [[nodiscard]] std::expected<MtlWorkerControlEventEnvelope, Error>
    wait_for_response(const MtlWorkerRequestId request_id) {
        std::unique_lock lock(state_mutex);

        state_cv.wait(lock, [this, request_id]() {
            return completed_responses.contains(request_id) || reader_failed || shutdown_requested;
        });

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
            return std::unexpected(wrote.error());
        }
    }

    return impl_->wait_for_response(request_id);
}

void MtlWorkerProcessControlChannel::shutdown_noexcept() noexcept { impl_->shutdown_noexcept(); }

const std::filesystem::path &MtlWorkerProcessControlChannel::worker_executable_path() const noexcept {
    return impl_->worker_executable_path;
}

std::shared_ptr<IMtlWorkerControlChannel>
create_mtl_worker_process_control_channel(std::filesystem::path worker_executable_path) {
    return std::make_shared<MtlWorkerProcessControlChannel>(std::move(worker_executable_path));
}

} // namespace st2110
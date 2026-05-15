#include <st2110/backends/mtl/mtl_worker_ipc_framing.hpp>

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <span>
#include <utility>
#include <vector>

namespace st2110 {
namespace {

void close_fd_noexcept(const int fd) noexcept {
    if (fd >= 0) {
        (void)::close(fd);
    }
}

void close_fds_noexcept(std::vector<int> &fds) noexcept {
    for (const int fd : fds) {
        close_fd_noexcept(fd);
    }

    fds.clear();
}

[[nodiscard]] bool set_close_on_exec_noexcept(const int fd) noexcept {
    if (fd < 0) {
        return false;
    }

    const int flags = ::fcntl(fd, F_GETFD);
    if (flags < 0) {
        return false;
    }

    return ::fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == 0;
}

[[nodiscard]] constexpr int recvmsg_fd_flags() noexcept {
#ifdef MSG_CMSG_CLOEXEC
    return MSG_CMSG_CLOEXEC;
#else
    return 0;
#endif
}

void encode_u32_be(const std::uint32_t value, std::array<std::uint8_t, 4> &out) noexcept {
    out[0] = static_cast<std::uint8_t>((value >> 24) & 0xFF);
    out[1] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
    out[2] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
    out[3] = static_cast<std::uint8_t>(value & 0xFF);
}

[[nodiscard]] std::uint32_t decode_u32_be(const std::array<std::uint8_t, 4> &in) noexcept {
    return (static_cast<std::uint32_t>(in[0]) << 24) | (static_cast<std::uint32_t>(in[1]) << 16) |
           (static_cast<std::uint32_t>(in[2]) << 8) | static_cast<std::uint32_t>(in[3]);
}

[[nodiscard]] std::expected<bool, Error> write_all_no_fds(int fd, const std::uint8_t *data, std::size_t size) {
    if (fd < 0 || (!data && size != 0)) {
        return std::unexpected(Error::InvalidValue);
    }

    std::size_t written = 0;
    while (written < size) {
        const ssize_t n = ::send(fd, data + written, size - written, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }

            return std::unexpected(Error::SystemFailure);
        }

        if (n == 0) {
            return std::unexpected(Error::OperationAborted);
        }

        written += static_cast<std::size_t>(n);
    }

    return true;
}

[[nodiscard]] std::expected<bool, Error> write_remaining_no_fds(int fd, std::span<const std::uint8_t> header,
                                                                std::span<const std::uint8_t> payload,
                                                                std::size_t already_written) {
    const std::size_t total_size = header.size() + payload.size();
    if (already_written > total_size) {
        return std::unexpected(Error::InvalidValue);
    }

    if (already_written < header.size()) {
        auto wrote_header = write_all_no_fds(fd, header.data() + already_written, header.size() - already_written);
        if (!wrote_header.has_value()) {
            return std::unexpected(wrote_header.error());
        }

        already_written = header.size();
    }

    const std::size_t payload_offset = already_written - header.size();
    if (payload_offset < payload.size()) {
        return write_all_no_fds(fd, payload.data() + payload_offset, payload.size() - payload_offset);
    }

    return true;
}

[[nodiscard]] std::expected<bool, Error> validate_outbound_frame(int fd, std::span<const std::uint8_t> payload,
                                                                 std::span<const int> file_descriptors) {
    if (fd < 0 || payload.empty() || payload.size() > defaultMtlWorkerMaxControlFrameBytes ||
        file_descriptors.size() > defaultMtlWorkerMaxAncillaryFileDescriptors) {
        return std::unexpected(Error::InvalidValue);
    }

    for (const int descriptor : file_descriptors) {
        if (descriptor < 0) {
            return std::unexpected(Error::InvalidValue);
        }
    }

    return true;
}

[[nodiscard]] std::expected<bool, Error> send_frame_with_fds(int fd, std::span<const std::uint8_t> header,
                                                             std::span<const std::uint8_t> payload,
                                                             std::span<const int> file_descriptors) {
    std::array<iovec, 2> iov{};
    iov[0].iov_base = const_cast<std::uint8_t *>(header.data());
    iov[0].iov_len = header.size();
    iov[1].iov_base = const_cast<std::uint8_t *>(payload.data());
    iov[1].iov_len = payload.size();

    std::vector<char> control(CMSG_SPACE(sizeof(int) * file_descriptors.size()));

    msghdr message{};
    message.msg_iov = iov.data();
    message.msg_iovlen = iov.size();
    message.msg_control = control.data();
    message.msg_controllen = control.size();

    auto *cmsg = CMSG_FIRSTHDR(&message);
    if (!cmsg) {
        return std::unexpected(Error::SystemFailure);
    }

    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int) * file_descriptors.size());
    std::memcpy(CMSG_DATA(cmsg), file_descriptors.data(), sizeof(int) * file_descriptors.size());

    const std::size_t total_size = header.size() + payload.size();

    ssize_t sent = -1;
    do {
        sent = ::sendmsg(fd, &message, MSG_NOSIGNAL);
    } while (sent < 0 && errno == EINTR);

    if (sent < 0) {
        return std::unexpected(Error::SystemFailure);
    }

    if (sent == 0) {
        return std::unexpected(Error::OperationAborted);
    }

    const auto written = static_cast<std::size_t>(sent);
    if (written > total_size) {
        return std::unexpected(Error::SystemFailure);
    }

    /*
     * File descriptors are transferred at most once, with the first sendmsg().
     * If the stream write was partial, only the remaining bytes are sent here;
     * descriptors must not be sent again.
     */
    if (written < total_size) {
        return write_remaining_no_fds(fd, header, payload, written);
    }

    return true;
}

[[nodiscard]] std::expected<bool, Error> append_received_fds(msghdr &message, std::size_t max_file_descriptors,
                                                             std::vector<int> &out_fds) {
    bool too_many_descriptors = false;
    bool close_on_exec_failed = false;

    for (cmsghdr *cmsg = CMSG_FIRSTHDR(&message); cmsg; cmsg = CMSG_NXTHDR(&message, cmsg)) {
        if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS) {
            return std::unexpected(Error::Unsupported);
        }

        if (cmsg->cmsg_len < CMSG_LEN(0)) {
            return std::unexpected(Error::InvalidValue);
        }

        const std::size_t byte_count = cmsg->cmsg_len - CMSG_LEN(0);
        if (byte_count % sizeof(int) != 0) {
            return std::unexpected(Error::InvalidValue);
        }

        const std::size_t fd_count = byte_count / sizeof(int);
        const auto *fds = reinterpret_cast<const int *>(CMSG_DATA(cmsg));

        for (std::size_t i = 0; i < fd_count; ++i) {
            const int received_fd = fds[i];

            if (out_fds.size() >= max_file_descriptors) {
                close_fd_noexcept(received_fd);
                too_many_descriptors = true;
                continue;
            }

            if (!set_close_on_exec_noexcept(received_fd)) {
                close_fd_noexcept(received_fd);
                close_on_exec_failed = true;
                continue;
            }

            out_fds.push_back(received_fd);
        }
    }

    if (close_on_exec_failed) {
        return std::unexpected(Error::SystemFailure);
    }

    if (too_many_descriptors) {
        return std::unexpected(Error::InvalidValue);
    }

    return true;
}

[[nodiscard]] std::expected<bool, Error> read_all_with_fds(int fd, std::uint8_t *data, std::size_t size,
                                                           std::size_t max_file_descriptors,
                                                           std::vector<int> &out_fds) {
    if (fd < 0 || (!data && size != 0) || max_file_descriptors > defaultMtlWorkerMaxAncillaryFileDescriptors) {
        return std::unexpected(Error::InvalidValue);
    }

    std::size_t read_bytes = 0;

    while (read_bytes < size) {
        iovec iov{};
        iov.iov_base = data + read_bytes;
        iov.iov_len = size - read_bytes;

        const std::size_t remaining_fd_capacity =
            out_fds.size() < max_file_descriptors ? max_file_descriptors - out_fds.size() : 0;

        /*
         * Allocate one control-fd slot even when this caller accepts zero fds.
         * That lets us receive and close a protocol-violating descriptor
         * instead of relying only on MSG_CTRUNC behavior.
         */
        const std::size_t control_fd_capacity = remaining_fd_capacity > 0 ? remaining_fd_capacity : 1;
        std::vector<char> control(CMSG_SPACE(sizeof(int) * control_fd_capacity));

        msghdr message{};
        message.msg_iov = &iov;
        message.msg_iovlen = 1;
        message.msg_control = control.data();
        message.msg_controllen = control.size();

        ssize_t n = -1;
        do {
            n = ::recvmsg(fd, &message, recvmsg_fd_flags());
        } while (n < 0 && errno == EINTR);

        if (n < 0) {
            return std::unexpected(Error::SystemFailure);
        }

        if (n == 0) {
            return std::unexpected(Error::OperationAborted);
        }

        auto appended = append_received_fds(message, max_file_descriptors, out_fds);
        if (!appended.has_value()) {
            return std::unexpected(appended.error());
        }

        if ((message.msg_flags & MSG_CTRUNC) != 0) {
            return std::unexpected(Error::InvalidValue);
        }

#ifdef MSG_TRUNC
        if ((message.msg_flags & MSG_TRUNC) != 0) {
            return std::unexpected(Error::InvalidValue);
        }
#endif

        read_bytes += static_cast<std::size_t>(n);
    }

    return true;
}

} // namespace

MtlWorkerIpcFrame::MtlWorkerIpcFrame(std::vector<std::uint8_t> payload, std::vector<int> file_descriptors)
    : payload_(std::move(payload)), file_descriptors_(std::move(file_descriptors)) {}

MtlWorkerIpcFrame::~MtlWorkerIpcFrame() { close_fds_noexcept(file_descriptors_); }

MtlWorkerIpcFrame::MtlWorkerIpcFrame(MtlWorkerIpcFrame &&other) noexcept
    : payload_(std::exchange(other.payload_, {})), file_descriptors_(std::exchange(other.file_descriptors_, {})) {}

MtlWorkerIpcFrame &MtlWorkerIpcFrame::operator=(MtlWorkerIpcFrame &&other) noexcept {
    if (this == &other) {
        return *this;
    }

    close_fds_noexcept(file_descriptors_);

    payload_ = std::exchange(other.payload_, {});
    file_descriptors_ = std::exchange(other.file_descriptors_, {});

    return *this;
}

const std::vector<std::uint8_t> &MtlWorkerIpcFrame::payload() const noexcept { return payload_; }

std::vector<std::uint8_t> &MtlWorkerIpcFrame::payload() noexcept { return payload_; }

std::vector<std::uint8_t> MtlWorkerIpcFrame::release_payload() noexcept { return std::exchange(payload_, {}); }

const std::vector<int> &MtlWorkerIpcFrame::file_descriptors() const noexcept { return file_descriptors_; }

bool MtlWorkerIpcFrame::has_file_descriptors() const noexcept { return !file_descriptors_.empty(); }

std::vector<int> MtlWorkerIpcFrame::release_file_descriptors() noexcept { return std::exchange(file_descriptors_, {}); }

std::expected<bool, Error> write_mtl_worker_control_frame(int fd, std::span<const std::uint8_t> payload) {
    return write_mtl_worker_control_frame_with_fds(fd, payload, {});
}

std::expected<bool, Error> write_mtl_worker_control_frame_with_fds(int fd, std::span<const std::uint8_t> payload,
                                                                   std::span<const int> file_descriptors) {
    auto valid = validate_outbound_frame(fd, payload, file_descriptors);
    if (!valid.has_value()) {
        return std::unexpected(valid.error());
    }

    std::array<std::uint8_t, 4> header{};
    encode_u32_be(static_cast<std::uint32_t>(payload.size()), header);

    if (file_descriptors.empty()) {
        auto wrote_header = write_all_no_fds(fd, header.data(), header.size());
        if (!wrote_header.has_value()) {
            return std::unexpected(wrote_header.error());
        }

        return write_all_no_fds(fd, payload.data(), payload.size());
    }

    return send_frame_with_fds(fd, header, payload, file_descriptors);
}

std::expected<std::vector<std::uint8_t>, Error> read_mtl_worker_control_frame(int fd, std::uint32_t max_frame_bytes) {
    auto frame = read_mtl_worker_control_frame_with_fds(fd, max_frame_bytes, 0);
    if (!frame.has_value()) {
        return std::unexpected(frame.error());
    }

    if (frame->has_file_descriptors()) {
        return std::unexpected(Error::InvalidValue);
    }

    return frame->release_payload();
}

std::expected<MtlWorkerIpcFrame, Error> read_mtl_worker_control_frame_with_fds(int fd, std::uint32_t max_frame_bytes,
                                                                               std::size_t max_file_descriptors) {
    if (fd < 0 || max_frame_bytes == 0 || max_file_descriptors > defaultMtlWorkerMaxAncillaryFileDescriptors) {
        return std::unexpected(Error::InvalidValue);
    }

    std::vector<int> received_fds{};

    std::array<std::uint8_t, 4> header{};
    auto read_header = read_all_with_fds(fd, header.data(), header.size(), max_file_descriptors, received_fds);
    if (!read_header.has_value()) {
        close_fds_noexcept(received_fds);
        return std::unexpected(read_header.error());
    }

    const std::uint32_t payload_size = decode_u32_be(header);
    if (payload_size == 0 || payload_size > max_frame_bytes) {
        close_fds_noexcept(received_fds);
        return std::unexpected(Error::InvalidValue);
    }

    std::vector<std::uint8_t> payload(payload_size);
    auto read_payload = read_all_with_fds(fd, payload.data(), payload.size(), max_file_descriptors, received_fds);
    if (!read_payload.has_value()) {
        close_fds_noexcept(received_fds);
        return std::unexpected(read_payload.error());
    }

    return MtlWorkerIpcFrame{std::move(payload), std::move(received_fds)};
}

} // namespace st2110
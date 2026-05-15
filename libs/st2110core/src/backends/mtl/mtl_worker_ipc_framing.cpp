#include <st2110/backends/mtl/mtl_worker_ipc_framing.hpp>

#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstddef>

namespace st2110 {
namespace {

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

[[nodiscard]] std::expected<bool, Error> write_all(int fd, const std::uint8_t *data, std::size_t size) {
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

[[nodiscard]] std::expected<bool, Error> read_all(int fd, std::uint8_t *data, std::size_t size) {
    if (fd < 0 || (!data && size != 0)) {
        return std::unexpected(Error::InvalidValue);
    }

    std::size_t read_bytes = 0;
    while (read_bytes < size) {
        const ssize_t n = ::read(fd, data + read_bytes, size - read_bytes);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }

            return std::unexpected(Error::SystemFailure);
        }

        if (n == 0) {
            return std::unexpected(Error::OperationAborted);
        }

        read_bytes += static_cast<std::size_t>(n);
    }

    return true;
}

} // namespace

std::expected<bool, Error> write_mtl_worker_control_frame(int fd, std::span<const std::uint8_t> payload) {
    if (fd < 0 || payload.empty() || payload.size() > defaultMtlWorkerMaxControlFrameBytes) {
        return std::unexpected(Error::InvalidValue);
    }

    std::array<std::uint8_t, 4> header{};
    encode_u32_be(static_cast<std::uint32_t>(payload.size()), header);

    auto wrote_header = write_all(fd, header.data(), header.size());
    if (!wrote_header.has_value()) {
        return std::unexpected(wrote_header.error());
    }

    return write_all(fd, payload.data(), payload.size());
}

std::expected<std::vector<std::uint8_t>, Error> read_mtl_worker_control_frame(int fd, std::uint32_t max_frame_bytes) {
    if (fd < 0 || max_frame_bytes == 0) {
        return std::unexpected(Error::InvalidValue);
    }

    std::array<std::uint8_t, 4> header{};
    auto read_header = read_all(fd, header.data(), header.size());
    if (!read_header.has_value()) {
        return std::unexpected(read_header.error());
    }

    const std::uint32_t payload_size = decode_u32_be(header);
    if (payload_size == 0 || payload_size > max_frame_bytes) {
        return std::unexpected(Error::InvalidValue);
    }

    std::vector<std::uint8_t> payload(payload_size);
    auto read_payload = read_all(fd, payload.data(), payload.size());
    if (!read_payload.has_value()) {
        return std::unexpected(read_payload.error());
    }

    return payload;
}

} // namespace st2110
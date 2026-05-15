#include <st2110/backends/mtl/mtl_worker_ipc_codec.hpp>

#include <array>
#include <cstddef>
#include <cstring>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace st2110 {
namespace {

enum class MessageTag : std::uint8_t {
    ConfigHandshakeRequest = 1,
    StartSessionsRequest = 2,
    StopSessionsRequest = 3,
    ShutdownRequest = 4,
    StatsRequest = 5,
    HealthCheckRequest = 6,

    HealthEvent = 101,
    ErrorEvent = 102,
    StartedEvent = 103,
    StoppedEvent = 104,
    StatsEvent = 105,
    FrameReadyEvent = 106,
    AudioBlockReadyEvent = 107,
};

class Writer {
  public:
    void u8(const std::uint8_t value) { bytes_.push_back(value); }

    void u16(const std::uint16_t value) {
        bytes_.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
        bytes_.push_back(static_cast<std::uint8_t>(value & 0xFF));
    }

    void u32(const std::uint32_t value) {
        bytes_.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
        bytes_.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
        bytes_.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
        bytes_.push_back(static_cast<std::uint8_t>(value & 0xFF));
    }

    void u64(const std::uint64_t value) {
        for (int shift = 56; shift >= 0; shift -= 8) {
            bytes_.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFF));
        }
    }

    [[nodiscard]] std::expected<bool, Error> string(const std::string &value) {
        if (value.size() > std::numeric_limits<std::uint32_t>::max()) {
            return std::unexpected(Error::InvalidValue);
        }

        u32(static_cast<std::uint32_t>(value.size()));
        bytes_.insert(bytes_.end(), value.begin(), value.end());
        return true;
    }

    void bytes(std::span<const std::uint8_t> value) { bytes_.insert(bytes_.end(), value.begin(), value.end()); }

    [[nodiscard]] std::vector<std::uint8_t> finish() && { return std::move(bytes_); }

  private:
    std::vector<std::uint8_t> bytes_{};
};

class Reader {
  public:
    explicit Reader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

    [[nodiscard]] std::expected<std::uint8_t, Error> u8() {
        if (remaining() < 1) {
            return std::unexpected(Error::InvalidValue);
        }

        return bytes_[offset_++];
    }

    [[nodiscard]] std::expected<std::uint16_t, Error> u16() {
        if (remaining() < 2) {
            return std::unexpected(Error::InvalidValue);
        }

        const std::uint16_t value = static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes_[offset_]) << 8) |
                                                               static_cast<std::uint16_t>(bytes_[offset_ + 1]));

        offset_ += 2;
        return value;
    }

    [[nodiscard]] std::expected<std::uint32_t, Error> u32() {
        if (remaining() < 4) {
            return std::unexpected(Error::InvalidValue);
        }

        const std::uint32_t value = (static_cast<std::uint32_t>(bytes_[offset_]) << 24) |
                                    (static_cast<std::uint32_t>(bytes_[offset_ + 1]) << 16) |
                                    (static_cast<std::uint32_t>(bytes_[offset_ + 2]) << 8) |
                                    static_cast<std::uint32_t>(bytes_[offset_ + 3]);

        offset_ += 4;
        return value;
    }

    [[nodiscard]] std::expected<std::uint64_t, Error> u64() {
        if (remaining() < 8) {
            return std::unexpected(Error::InvalidValue);
        }

        std::uint64_t value = 0;
        for (int i = 0; i < 8; ++i) {
            value = (value << 8) | static_cast<std::uint64_t>(bytes_[offset_ + i]);
        }

        offset_ += 8;
        return value;
    }

    [[nodiscard]] std::expected<std::string, Error> string() {
        auto size = u32();
        if (!size.has_value()) {
            return std::unexpected(size.error());
        }

        if (remaining() < *size) {
            return std::unexpected(Error::InvalidValue);
        }

        std::string value(reinterpret_cast<const char *>(bytes_.data() + offset_), *size);
        offset_ += *size;
        return value;
    }

    [[nodiscard]] std::expected<std::array<std::uint8_t, 4>, Error> ipv4() {
        if (remaining() < 4) {
            return std::unexpected(Error::InvalidValue);
        }

        std::array<std::uint8_t, 4> value{};
        std::memcpy(value.data(), bytes_.data() + offset_, value.size());
        offset_ += value.size();
        return value;
    }

    [[nodiscard]] bool empty() const noexcept { return offset_ == bytes_.size(); }

  private:
    [[nodiscard]] std::size_t remaining() const noexcept { return bytes_.size() - offset_; }

    std::span<const std::uint8_t> bytes_{};
    std::size_t offset_ = 0;
};

void write_size_t_u64(Writer &writer, const std::size_t value) { writer.u64(static_cast<std::uint64_t>(value)); }

[[nodiscard]] std::expected<std::size_t, Error> read_size_t_u64(Reader &reader) {
    auto value = reader.u64();
    if (!value.has_value()) {
        return std::unexpected(value.error());
    }

    if (*value > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        return std::unexpected(Error::InvalidValue);
    }

    return static_cast<std::size_t>(*value);
}

[[nodiscard]] bool add_overflows_u64(const std::uint64_t a, const std::uint64_t b, std::uint64_t &out) noexcept {
    if (std::numeric_limits<std::uint64_t>::max() - a < b) {
        return true;
    }

    out = a + b;
    return false;
}

[[nodiscard]] bool mul_overflows_u64(const std::uint64_t a, const std::uint64_t b, std::uint64_t &out) noexcept {
    if (a != 0 && b > std::numeric_limits<std::uint64_t>::max() / a) {
        return true;
    }

    out = a * b;
    return false;
}

[[nodiscard]] std::expected<MtlWorkerMediaKind, Error> read_worker_media_kind(Reader &reader) {
    auto value = reader.u32();
    if (!value.has_value()) {
        return std::unexpected(value.error());
    }

    const auto media_kind = static_cast<MtlWorkerMediaKind>(*value);
    switch (media_kind) {
    case MtlWorkerMediaKind::Video:
    case MtlWorkerMediaKind::Audio:
        return media_kind;
    }

    return std::unexpected(Error::InvalidValue);
}

[[nodiscard]] std::expected<bool, Error>
validate_shared_memory_ring_descriptor(const MtlWorkerSharedMemoryRingDescriptor &descriptor) {
    if (descriptor.ring_id == 0 || descriptor.layout_version != mtlWorkerSharedMemoryRingLayoutVersion ||
        descriptor.mapped_size_bytes == 0 || descriptor.slot_count == 0 || descriptor.slot_stride_bytes == 0 ||
        descriptor.slot_payload_capacity_bytes == 0) {
        return std::unexpected(Error::InvalidValue);
    }

    switch (descriptor.media_kind) {
    case MtlWorkerMediaKind::Video:
    case MtlWorkerMediaKind::Audio:
        break;
    default:
        return std::unexpected(Error::InvalidValue);
    }

    std::uint64_t payload_end_within_slot = 0;
    if (add_overflows_u64(descriptor.slot_payload_offset_bytes, descriptor.slot_payload_capacity_bytes,
                          payload_end_within_slot)) {
        return std::unexpected(Error::InvalidValue);
    }

    if (payload_end_within_slot > descriptor.slot_stride_bytes) {
        return std::unexpected(Error::InvalidValue);
    }

    std::uint64_t last_slot_relative_offset = 0;
    if (mul_overflows_u64(static_cast<std::uint64_t>(descriptor.slot_count - 1), descriptor.slot_stride_bytes,
                          last_slot_relative_offset)) {
        return std::unexpected(Error::InvalidValue);
    }

    std::uint64_t last_slot_payload_end = 0;
    if (add_overflows_u64(last_slot_relative_offset, payload_end_within_slot, last_slot_payload_end)) {
        return std::unexpected(Error::InvalidValue);
    }

    std::uint64_t mapped_payload_end = 0;
    if (add_overflows_u64(descriptor.slot_region_offset_bytes, last_slot_payload_end, mapped_payload_end)) {
        return std::unexpected(Error::InvalidValue);
    }

    if (mapped_payload_end > descriptor.mapped_size_bytes) {
        return std::unexpected(Error::InvalidValue);
    }

    return true;
}

[[nodiscard]] std::expected<bool, Error>
write_shared_memory_ring_descriptor(Writer &writer, const MtlWorkerSharedMemoryRingDescriptor &descriptor) {
    auto valid = validate_shared_memory_ring_descriptor(descriptor);
    if (!valid.has_value()) {
        return std::unexpected(valid.error());
    }

    writer.u64(descriptor.ring_id);
    writer.u32(static_cast<std::uint32_t>(descriptor.media_kind));
    writer.u32(descriptor.fd_index);
    writer.u32(descriptor.layout_version);
    writer.u64(descriptor.mapped_size_bytes);
    writer.u64(descriptor.slot_region_offset_bytes);
    writer.u32(descriptor.slot_count);
    writer.u64(descriptor.slot_stride_bytes);
    writer.u64(descriptor.slot_payload_offset_bytes);
    writer.u64(descriptor.slot_payload_capacity_bytes);

    return true;
}

[[nodiscard]] std::expected<MtlWorkerSharedMemoryRingDescriptor, Error>
read_shared_memory_ring_descriptor(Reader &reader) {
    auto ring_id = reader.u64();
    if (!ring_id.has_value()) {
        return std::unexpected(ring_id.error());
    }

    auto media_kind = read_worker_media_kind(reader);
    if (!media_kind.has_value()) {
        return std::unexpected(media_kind.error());
    }

    auto fd_index = reader.u32();
    auto layout_version = reader.u32();
    auto mapped_size_bytes = reader.u64();
    auto slot_region_offset_bytes = reader.u64();
    auto slot_count = reader.u32();
    auto slot_stride_bytes = reader.u64();
    auto slot_payload_offset_bytes = reader.u64();
    auto slot_payload_capacity_bytes = reader.u64();

    if (!fd_index.has_value() || !layout_version.has_value() || !mapped_size_bytes.has_value() ||
        !slot_region_offset_bytes.has_value() || !slot_count.has_value() || !slot_stride_bytes.has_value() ||
        !slot_payload_offset_bytes.has_value() || !slot_payload_capacity_bytes.has_value()) {
        return std::unexpected(Error::InvalidValue);
    }

    MtlWorkerSharedMemoryRingDescriptor descriptor{
        .ring_id = *ring_id,
        .media_kind = *media_kind,
        .fd_index = *fd_index,
        .layout_version = *layout_version,
        .mapped_size_bytes = *mapped_size_bytes,
        .slot_region_offset_bytes = *slot_region_offset_bytes,
        .slot_count = *slot_count,
        .slot_stride_bytes = *slot_stride_bytes,
        .slot_payload_offset_bytes = *slot_payload_offset_bytes,
        .slot_payload_capacity_bytes = *slot_payload_capacity_bytes,
    };

    auto valid = validate_shared_memory_ring_descriptor(descriptor);
    if (!valid.has_value()) {
        return std::unexpected(valid.error());
    }

    return descriptor;
}

[[nodiscard]] std::expected<bool, Error>
validate_shared_memory_ring_descriptors(const std::vector<MtlWorkerSharedMemoryRingDescriptor> &descriptors) {
    if (descriptors.size() > defaultMtlWorkerMaxSharedMemoryRingDescriptors) {
        return std::unexpected(Error::InvalidValue);
    }

    for (std::size_t i = 0; i < descriptors.size(); ++i) {
        auto valid = validate_shared_memory_ring_descriptor(descriptors[i]);
        if (!valid.has_value()) {
            return std::unexpected(valid.error());
        }

        for (std::size_t j = 0; j < i; ++j) {
            if (descriptors[j].ring_id == descriptors[i].ring_id) {
                return std::unexpected(Error::InvalidValue);
            }
        }
    }

    return true;
}

[[nodiscard]] bool media_kind_has_matching_session(const MtlWorkerMediaKind media_kind, const bool has_video,
                                                   const bool has_audio) noexcept {
    switch (media_kind) {
    case MtlWorkerMediaKind::Video:
        return has_video;
    case MtlWorkerMediaKind::Audio:
        return has_audio;
    }

    return false;
}

[[nodiscard]] std::expected<bool, Error>
validate_start_sessions_media_rings(const bool has_video, const bool has_audio,
                                    const std::vector<MtlWorkerSharedMemoryRingDescriptor> &descriptors) {
    auto valid = validate_shared_memory_ring_descriptors(descriptors);
    if (!valid.has_value()) {
        return std::unexpected(valid.error());
    }

    for (const auto &descriptor : descriptors) {
        if (!media_kind_has_matching_session(descriptor.media_kind, has_video, has_audio)) {
            return std::unexpected(Error::InvalidValue);
        }
    }

    return true;
}

[[nodiscard]] std::expected<bool, Error>
write_shared_memory_ring_descriptors(Writer &writer,
                                     const std::vector<MtlWorkerSharedMemoryRingDescriptor> &descriptors) {
    auto valid = validate_shared_memory_ring_descriptors(descriptors);
    if (!valid.has_value()) {
        return std::unexpected(valid.error());
    }

    writer.u32(static_cast<std::uint32_t>(descriptors.size()));

    for (const auto &descriptor : descriptors) {
        auto wrote = write_shared_memory_ring_descriptor(writer, descriptor);
        if (!wrote.has_value()) {
            return std::unexpected(wrote.error());
        }
    }

    return true;
}

[[nodiscard]] std::expected<std::vector<MtlWorkerSharedMemoryRingDescriptor>, Error>
read_shared_memory_ring_descriptors(Reader &reader) {
    auto count = reader.u32();
    if (!count.has_value()) {
        return std::unexpected(count.error());
    }

    if (*count > defaultMtlWorkerMaxSharedMemoryRingDescriptors) {
        return std::unexpected(Error::InvalidValue);
    }

    std::vector<MtlWorkerSharedMemoryRingDescriptor> descriptors{};
    descriptors.reserve(*count);

    for (std::uint32_t i = 0; i < *count; ++i) {
        auto descriptor = read_shared_memory_ring_descriptor(reader);
        if (!descriptor.has_value()) {
            return std::unexpected(descriptor.error());
        }

        descriptors.push_back(std::move(*descriptor));
    }

    auto valid = validate_shared_memory_ring_descriptors(descriptors);
    if (!valid.has_value()) {
        return std::unexpected(valid.error());
    }

    return descriptors;
}

[[nodiscard]] std::expected<bool, Error> write_runtime_port(Writer &writer, const MtlRuntimePortConfig &port) {
    auto wrote_name = writer.string(port.port_name);
    if (!wrote_name.has_value()) {
        return std::unexpected(wrote_name.error());
    }

    writer.bytes(port.sip_addr);
    return true;
}

[[nodiscard]] std::expected<bool, Error> write_runtime_config(Writer &writer, const MtlRuntimeConfig &runtime) {
    auto wrote_primary = write_runtime_port(writer, runtime.primary_port);
    if (!wrote_primary.has_value()) {
        return std::unexpected(wrote_primary.error());
    }

    writer.u8(runtime.redundant_port.has_value() ? 1 : 0);

    if (runtime.redundant_port.has_value()) {
        auto wrote_redundant = write_runtime_port(writer, *runtime.redundant_port);
        if (!wrote_redundant.has_value()) {
            return std::unexpected(wrote_redundant.error());
        }
    }

    return true;
}

[[nodiscard]] std::expected<MtlRuntimePortConfig, Error> read_runtime_port(Reader &reader) {
    auto name = reader.string();
    if (!name.has_value()) {
        return std::unexpected(name.error());
    }

    auto ip = reader.ipv4();
    if (!ip.has_value()) {
        return std::unexpected(ip.error());
    }

    return MtlRuntimePortConfig{
        .port_name = std::move(*name),
        .sip_addr = *ip,
    };
}

[[nodiscard]] std::expected<bool, Error> read_bool_u8(Reader &reader) {
    auto value = reader.u8();
    if (!value.has_value()) {
        return std::unexpected(value.error());
    }

    if (*value > 1) {
        return std::unexpected(Error::InvalidValue);
    }

    return *value != 0;
}

[[nodiscard]] std::expected<MtlRuntimeConfig, Error> read_runtime_config(Reader &reader) {
    auto primary = read_runtime_port(reader);
    if (!primary.has_value()) {
        return std::unexpected(primary.error());
    }

    auto has_redundant = read_bool_u8(reader);
    if (!has_redundant.has_value()) {
        return std::unexpected(has_redundant.error());
    }

    MtlRuntimeConfig runtime{
        .primary_port = std::move(*primary),
        .redundant_port = std::nullopt,
    };

    if (*has_redundant) {
        auto redundant = read_runtime_port(reader);
        if (!redundant.has_value()) {
            return std::unexpected(redundant.error());
        }

        runtime.redundant_port = std::move(*redundant);
    }

    return runtime;
}

void write_optional_ipv4(Writer &writer, const std::optional<std::array<std::uint8_t, 4>> &ip) {
    writer.u8(ip.has_value() ? 1 : 0);
    if (ip.has_value()) {
        writer.bytes(*ip);
    }
}

[[nodiscard]] std::expected<std::optional<std::array<std::uint8_t, 4>>, Error> read_optional_ipv4(Reader &reader) {
    auto has_value = read_bool_u8(reader);
    if (!has_value.has_value()) {
        return std::unexpected(has_value.error());
    }

    if (!*has_value) {
        return std::optional<std::array<std::uint8_t, 4>>{};
    }

    auto ip = reader.ipv4();
    if (!ip.has_value()) {
        return std::unexpected(ip.error());
    }

    return std::optional<std::array<std::uint8_t, 4>>{*ip};
}

void write_video_session_port(Writer &writer, const MtlVideoSessionPortConfig &port) {
    writer.bytes(port.ip_addr);
    write_optional_ipv4(writer, port.source_ip);
    writer.u16(port.udp_port);
}

[[nodiscard]] std::expected<MtlVideoSessionPortConfig, Error> read_video_session_port(Reader &reader) {
    auto ip = reader.ipv4();
    if (!ip.has_value()) {
        return std::unexpected(ip.error());
    }

    auto source_ip = read_optional_ipv4(reader);
    if (!source_ip.has_value()) {
        return std::unexpected(source_ip.error());
    }

    auto udp_port = reader.u16();
    if (!udp_port.has_value()) {
        return std::unexpected(udp_port.error());
    }

    return MtlVideoSessionPortConfig{
        .ip_addr = *ip,
        .source_ip = std::move(*source_ip),
        .udp_port = *udp_port,
    };
}

void write_audio_session_port(Writer &writer, const MtlAudioSessionPortConfig &port) {
    writer.bytes(port.ip_addr);
    write_optional_ipv4(writer, port.source_ip);
    writer.u16(port.udp_port);
}

[[nodiscard]] std::expected<MtlAudioSessionPortConfig, Error> read_audio_session_port(Reader &reader) {
    auto ip = reader.ipv4();
    if (!ip.has_value()) {
        return std::unexpected(ip.error());
    }

    auto source_ip = read_optional_ipv4(reader);
    if (!source_ip.has_value()) {
        return std::unexpected(source_ip.error());
    }

    auto udp_port = reader.u16();
    if (!udp_port.has_value()) {
        return std::unexpected(udp_port.error());
    }

    return MtlAudioSessionPortConfig{
        .ip_addr = *ip,
        .source_ip = std::move(*source_ip),
        .udp_port = *udp_port,
    };
}

[[nodiscard]] std::expected<bool, Error> write_video_start_config(Writer &writer, const MtlVideoStartConfig &cfg) {
    auto wrote_runtime = write_runtime_config(writer, cfg.runtime);
    if (!wrote_runtime.has_value()) {
        return std::unexpected(wrote_runtime.error());
    }

    write_video_session_port(writer, cfg.primary);

    writer.u8(cfg.redundant.has_value() ? 1 : 0);
    if (cfg.redundant.has_value()) {
        write_video_session_port(writer, *cfg.redundant);
    }

    writer.u8(cfg.expected_payload_type);
    writer.u32(cfg.width);
    writer.u32(cfg.height);
    writer.u32(static_cast<std::uint32_t>(cfg.fps));
    writer.u32(static_cast<std::uint32_t>(cfg.scan_mode));
    writer.u32(static_cast<std::uint32_t>(cfg.transport_format));
    writer.u32(static_cast<std::uint32_t>(cfg.output_format));
    writer.u16(cfg.frame_buffer_count);

    return true;
}

[[nodiscard]] std::expected<MtlVideoStartConfig, Error> read_video_start_config(Reader &reader) {
    auto runtime = read_runtime_config(reader);
    if (!runtime.has_value()) {
        return std::unexpected(runtime.error());
    }

    auto primary = read_video_session_port(reader);
    if (!primary.has_value()) {
        return std::unexpected(primary.error());
    }

    auto has_redundant = read_bool_u8(reader);
    if (!has_redundant.has_value()) {
        return std::unexpected(has_redundant.error());
    }

    std::optional<MtlVideoSessionPortConfig> redundant{};
    if (*has_redundant) {
        auto redundant_port = read_video_session_port(reader);
        if (!redundant_port.has_value()) {
            return std::unexpected(redundant_port.error());
        }

        redundant = std::move(*redundant_port);
    }

    auto payload_type = reader.u8();
    auto width = reader.u32();
    auto height = reader.u32();
    auto fps = reader.u32();
    auto scan_mode = reader.u32();
    auto transport_format = reader.u32();
    auto output_format = reader.u32();
    auto frame_buffer_count = reader.u16();

    if (!payload_type.has_value() || !width.has_value() || !height.has_value() || !fps.has_value() ||
        !scan_mode.has_value() || !transport_format.has_value() || !output_format.has_value() ||
        !frame_buffer_count.has_value()) {
        return std::unexpected(Error::InvalidValue);
    }

    return MtlVideoStartConfig{
        .runtime = std::move(*runtime),
        .primary = std::move(*primary),
        .redundant = std::move(redundant),
        .expected_payload_type = *payload_type,
        .width = *width,
        .height = *height,
        .fps = static_cast<MtlVideoFrameRate>(*fps),
        .scan_mode = static_cast<VideoScanMode>(*scan_mode),
        .transport_format = static_cast<MtlVideoTransportFormat>(*transport_format),
        .output_format = static_cast<PixelFormat>(*output_format),
        .frame_buffer_count = *frame_buffer_count,
    };
}

void write_audio_media_description(Writer &writer, const AudioMediaDescription &media) {
    writer.u32(static_cast<std::uint32_t>(media.pcm_encoding));
    writer.u32(static_cast<std::uint32_t>(media.pcm_bit_depth));
    writer.u32(media.sampling_rate_hz);
    writer.u32(media.packet_time_us);
    writer.u16(media.channel_count);
}

[[nodiscard]] std::expected<AudioMediaDescription, Error> read_audio_media_description(Reader &reader) {
    auto pcm_encoding = reader.u32();
    auto pcm_bit_depth = reader.u32();
    auto sampling_rate_hz = reader.u32();
    auto packet_time_us = reader.u32();
    auto channel_count = reader.u16();

    if (!pcm_encoding.has_value() || !pcm_bit_depth.has_value() || !sampling_rate_hz.has_value() ||
        !packet_time_us.has_value() || !channel_count.has_value()) {
        return std::unexpected(Error::InvalidValue);
    }

    return AudioMediaDescription{
        .pcm_encoding = static_cast<AudioPcmEncoding>(*pcm_encoding),
        .pcm_bit_depth = static_cast<AudioPcmBitDepth>(*pcm_bit_depth),
        .sampling_rate_hz = *sampling_rate_hz,
        .packet_time_us = *packet_time_us,
        .channel_count = *channel_count,
    };
}

[[nodiscard]] std::expected<bool, Error> write_audio_start_config(Writer &writer, const MtlAudioStartConfig &cfg) {
    auto wrote_runtime = write_runtime_config(writer, cfg.runtime);
    if (!wrote_runtime.has_value()) {
        return std::unexpected(wrote_runtime.error());
    }

    write_audio_session_port(writer, cfg.primary);

    writer.u8(cfg.redundant.has_value() ? 1 : 0);
    if (cfg.redundant.has_value()) {
        write_audio_session_port(writer, *cfg.redundant);
    }

    writer.u8(cfg.expected_payload_type);
    write_audio_media_description(writer, cfg.media);
    writer.u32(cfg.samples_per_packet);
    writer.u32(static_cast<std::uint32_t>(cfg.pcm_format));
    writer.u32(static_cast<std::uint32_t>(cfg.sampling));
    writer.u32(static_cast<std::uint32_t>(cfg.packet_time));
    writer.u16(cfg.frame_buffer_count);
    writer.u64(cfg.frame_buffer_duration_ns);

    return true;
}

[[nodiscard]] std::expected<MtlAudioStartConfig, Error> read_audio_start_config(Reader &reader) {
    auto runtime = read_runtime_config(reader);
    if (!runtime.has_value()) {
        return std::unexpected(runtime.error());
    }

    auto primary = read_audio_session_port(reader);
    if (!primary.has_value()) {
        return std::unexpected(primary.error());
    }

    auto has_redundant = read_bool_u8(reader);
    if (!has_redundant.has_value()) {
        return std::unexpected(has_redundant.error());
    }

    std::optional<MtlAudioSessionPortConfig> redundant{};
    if (*has_redundant) {
        auto redundant_port = read_audio_session_port(reader);
        if (!redundant_port.has_value()) {
            return std::unexpected(redundant_port.error());
        }

        redundant = std::move(*redundant_port);
    }

    auto payload_type = reader.u8();
    if (!payload_type.has_value()) {
        return std::unexpected(payload_type.error());
    }

    auto media = read_audio_media_description(reader);
    if (!media.has_value()) {
        return std::unexpected(media.error());
    }

    auto samples_per_packet = reader.u32();
    auto pcm_format = reader.u32();
    auto sampling = reader.u32();
    auto packet_time = reader.u32();
    auto frame_buffer_count = reader.u16();
    auto frame_buffer_duration_ns = reader.u64();

    if (!samples_per_packet.has_value() || !pcm_format.has_value() || !sampling.has_value() ||
        !packet_time.has_value() || !frame_buffer_count.has_value() || !frame_buffer_duration_ns.has_value()) {
        return std::unexpected(Error::InvalidValue);
    }

    return MtlAudioStartConfig{
        .runtime = std::move(*runtime),
        .primary = std::move(*primary),
        .redundant = std::move(redundant),
        .expected_payload_type = *payload_type,
        .media = std::move(*media),
        .samples_per_packet = *samples_per_packet,
        .pcm_format = static_cast<MtlAudioPcmFormat>(*pcm_format),
        .sampling = static_cast<MtlAudioSampling>(*sampling),
        .packet_time = static_cast<MtlAudioPacketTime>(*packet_time),
        .frame_buffer_count = *frame_buffer_count,
        .frame_buffer_duration_ns = *frame_buffer_duration_ns,
    };
}

[[nodiscard]] std::expected<bool, Error> ensure_consumed(const Reader &reader) {
    if (!reader.empty()) {
        return std::unexpected(Error::InvalidValue);
    }

    return true;
}

} // namespace

std::expected<std::vector<std::uint8_t>, Error>
serialize_mtl_worker_control_request(const MtlWorkerControlRequest &request) {
    return std::visit(
        [](const auto &typed_request) -> std::expected<std::vector<std::uint8_t>, Error> {
            using Request = std::decay_t<decltype(typed_request)>;

            Writer writer{};

            if constexpr (std::is_same_v<Request, MtlWorkerConfigHandshakeRequest>) {
                writer.u8(static_cast<std::uint8_t>(MessageTag::ConfigHandshakeRequest));
                writer.u64(typed_request.request_id);

                auto wrote_runtime = write_runtime_config(writer, typed_request.runtime);
                if (!wrote_runtime.has_value()) {
                    return std::unexpected(wrote_runtime.error());
                }

                return std::move(writer).finish();
            } else if constexpr (std::is_same_v<Request, MtlWorkerStartSessionsRequest>) {
                writer.u8(static_cast<std::uint8_t>(MessageTag::StartSessionsRequest));
                writer.u64(typed_request.request_id);
                writer.u64(typed_request.graph_id);

                writer.u8(typed_request.video.has_value() ? 1 : 0);
                if (typed_request.video.has_value()) {
                    auto wrote_video = write_video_start_config(writer, *typed_request.video);
                    if (!wrote_video.has_value()) {
                        return std::unexpected(wrote_video.error());
                    }
                }

                writer.u8(typed_request.audio.has_value() ? 1 : 0);
                if (typed_request.audio.has_value()) {
                    auto wrote_audio = write_audio_start_config(writer, *typed_request.audio);
                    if (!wrote_audio.has_value()) {
                        return std::unexpected(wrote_audio.error());
                    }
                }

                auto valid_media_rings = validate_start_sessions_media_rings(
                    typed_request.video.has_value(), typed_request.audio.has_value(), typed_request.media_rings);
                if (!valid_media_rings.has_value()) {
                    return std::unexpected(valid_media_rings.error());
                }

                auto wrote_media_rings = write_shared_memory_ring_descriptors(writer, typed_request.media_rings);
                if (!wrote_media_rings.has_value()) {
                    return std::unexpected(wrote_media_rings.error());
                }

                return std::move(writer).finish();
            } else if constexpr (std::is_same_v<Request, MtlWorkerStopSessionsRequest>) {
                writer.u8(static_cast<std::uint8_t>(MessageTag::StopSessionsRequest));
                writer.u64(typed_request.request_id);
                writer.u64(typed_request.graph_id);
                return std::move(writer).finish();
            } else if constexpr (std::is_same_v<Request, MtlWorkerStatsRequest>) {
                writer.u8(static_cast<std::uint8_t>(MessageTag::StatsRequest));
                writer.u64(typed_request.request_id);
                writer.u64(typed_request.graph_id);
                return std::move(writer).finish();
            } else if constexpr (std::is_same_v<Request, MtlWorkerHealthCheckRequest>) {
                writer.u8(static_cast<std::uint8_t>(MessageTag::HealthCheckRequest));
                writer.u64(typed_request.request_id);
                return std::move(writer).finish();
            } else if constexpr (std::is_same_v<Request, MtlWorkerShutdownRequest>) {
                writer.u8(static_cast<std::uint8_t>(MessageTag::ShutdownRequest));
                writer.u64(typed_request.request_id);
                return std::move(writer).finish();
            } else {
                return std::unexpected(Error::Unsupported);
            }
        },
        request);
}

std::expected<MtlWorkerControlRequest, Error>
deserialize_mtl_worker_control_request(std::span<const std::uint8_t> payload) {
    Reader reader(payload);

    auto tag = reader.u8();
    if (!tag.has_value()) {
        return std::unexpected(tag.error());
    }

    switch (static_cast<MessageTag>(*tag)) {
    case MessageTag::ConfigHandshakeRequest: {
        auto request_id = reader.u64();
        if (!request_id.has_value()) {
            return std::unexpected(request_id.error());
        }

        auto runtime = read_runtime_config(reader);
        if (!runtime.has_value()) {
            return std::unexpected(runtime.error());
        }

        auto consumed = ensure_consumed(reader);
        if (!consumed.has_value()) {
            return std::unexpected(consumed.error());
        }

        return MtlWorkerControlRequest{
            MtlWorkerConfigHandshakeRequest{
                .request_id = *request_id,
                .runtime = std::move(*runtime),
            },
        };
    }
    case MessageTag::StartSessionsRequest: {
        auto request_id = reader.u64();
        if (!request_id.has_value()) {
            return std::unexpected(request_id.error());
        }

        auto graph_id = reader.u64();
        if (!graph_id.has_value()) {
            return std::unexpected(graph_id.error());
        }

        auto has_video = read_bool_u8(reader);
        if (!has_video.has_value()) {
            return std::unexpected(has_video.error());
        }

        std::optional<MtlVideoStartConfig> video{};
        if (*has_video) {
            auto video_cfg = read_video_start_config(reader);
            if (!video_cfg.has_value()) {
                return std::unexpected(video_cfg.error());
            }

            video = std::move(*video_cfg);
        }

        auto has_audio = read_bool_u8(reader);
        if (!has_audio.has_value()) {
            return std::unexpected(has_audio.error());
        }

        std::optional<MtlAudioStartConfig> audio{};
        if (*has_audio) {
            auto audio_cfg = read_audio_start_config(reader);
            if (!audio_cfg.has_value()) {
                return std::unexpected(audio_cfg.error());
            }

            audio = std::move(*audio_cfg);
        }

        auto media_rings = read_shared_memory_ring_descriptors(reader);
        if (!media_rings.has_value()) {
            return std::unexpected(media_rings.error());
        }

        auto valid_media_rings =
            validate_start_sessions_media_rings(video.has_value(), audio.has_value(), *media_rings);
        if (!valid_media_rings.has_value()) {
            return std::unexpected(valid_media_rings.error());
        }

        if (!video.has_value() && !audio.has_value()) {
            return std::unexpected(Error::InvalidValue);
        }

        auto consumed = ensure_consumed(reader);
        if (!consumed.has_value()) {
            return std::unexpected(consumed.error());
        }

        return MtlWorkerControlRequest{
            MtlWorkerStartSessionsRequest{
                .request_id = *request_id,
                .graph_id = *graph_id,
                .video = std::move(video),
                .audio = std::move(audio),
                .media_rings = std::move(*media_rings),
            },
        };
    }

    case MessageTag::StopSessionsRequest: {
        auto request_id = reader.u64();
        if (!request_id.has_value()) {
            return std::unexpected(request_id.error());
        }

        auto graph_id = reader.u64();
        if (!graph_id.has_value()) {
            return std::unexpected(graph_id.error());
        }

        auto consumed = ensure_consumed(reader);
        if (!consumed.has_value()) {
            return std::unexpected(consumed.error());
        }

        return MtlWorkerControlRequest{
            MtlWorkerStopSessionsRequest{
                .request_id = *request_id,
                .graph_id = *graph_id,
            },
        };
    }

    case MessageTag::StatsRequest: {
        auto request_id = reader.u64();
        if (!request_id.has_value()) {
            return std::unexpected(request_id.error());
        }

        auto graph_id = reader.u64();
        if (!graph_id.has_value()) {
            return std::unexpected(graph_id.error());
        }

        auto consumed = ensure_consumed(reader);
        if (!consumed.has_value()) {
            return std::unexpected(consumed.error());
        }

        return MtlWorkerControlRequest{
            MtlWorkerStatsRequest{
                .request_id = *request_id,
                .graph_id = *graph_id,
            },
        };
    }

    case MessageTag::HealthCheckRequest: {
        auto request_id = reader.u64();
        if (!request_id.has_value()) {
            return std::unexpected(request_id.error());
        }

        auto consumed = ensure_consumed(reader);
        if (!consumed.has_value()) {
            return std::unexpected(consumed.error());
        }

        return MtlWorkerControlRequest{
            MtlWorkerHealthCheckRequest{
                .request_id = *request_id,
            },
        };
    }

    case MessageTag::ShutdownRequest: {
        auto request_id = reader.u64();
        if (!request_id.has_value()) {
            return std::unexpected(request_id.error());
        }

        auto consumed = ensure_consumed(reader);
        if (!consumed.has_value()) {
            return std::unexpected(consumed.error());
        }

        return MtlWorkerControlRequest{
            MtlWorkerShutdownRequest{
                .request_id = *request_id,
            },
        };
    }

    default:
        return std::unexpected(Error::Unsupported);
    }
}

std::expected<std::vector<std::uint8_t>, Error> serialize_mtl_worker_control_event(const MtlWorkerControlEvent &event) {
    return std::visit(
        [](const auto &typed_event) -> std::expected<std::vector<std::uint8_t>, Error> {
            using Event = std::decay_t<decltype(typed_event)>;

            Writer writer{};

            if constexpr (std::is_same_v<Event, MtlWorkerHealthEvent>) {
                writer.u8(static_cast<std::uint8_t>(MessageTag::HealthEvent));
                writer.u64(typed_event.request_id);
                writer.u8(typed_event.healthy ? 1 : 0);

                auto wrote_message = writer.string(typed_event.message);
                if (!wrote_message.has_value()) {
                    return std::unexpected(wrote_message.error());
                }

                return std::move(writer).finish();
            } else if constexpr (std::is_same_v<Event, MtlWorkerErrorEvent>) {
                writer.u8(static_cast<std::uint8_t>(MessageTag::ErrorEvent));
                writer.u64(typed_event.request_id);
                writer.u64(typed_event.graph_id);
                writer.u32(static_cast<std::uint32_t>(typed_event.error));

                auto wrote_message = writer.string(typed_event.message);
                if (!wrote_message.has_value()) {
                    return std::unexpected(wrote_message.error());
                }

                return std::move(writer).finish();
            } else if constexpr (std::is_same_v<Event, MtlWorkerStartedEvent>) {
                writer.u8(static_cast<std::uint8_t>(MessageTag::StartedEvent));
                writer.u64(typed_event.request_id);
                writer.u64(typed_event.graph_id);
                return std::move(writer).finish();
            } else if constexpr (std::is_same_v<Event, MtlWorkerStoppedEvent>) {
                writer.u8(static_cast<std::uint8_t>(MessageTag::StoppedEvent));
                writer.u64(typed_event.request_id);
                writer.u64(typed_event.graph_id);
                return std::move(writer).finish();
            } else if constexpr (std::is_same_v<Event, MtlWorkerStatsEvent>) {
                writer.u8(static_cast<std::uint8_t>(MessageTag::StatsEvent));
                writer.u64(typed_event.request_id);
                writer.u64(typed_event.graph_id);
                writer.u64(typed_event.video_frames_received);
                writer.u64(typed_event.audio_blocks_received);
                writer.u64(typed_event.video_frames_dropped);
                writer.u64(typed_event.audio_blocks_dropped);
                return std::move(writer).finish();
            } else if constexpr (std::is_same_v<Event, MtlWorkerFrameReadyEvent>) {
                writer.u8(static_cast<std::uint8_t>(MessageTag::FrameReadyEvent));
                writer.u64(typed_event.graph_id);
                writer.u64(typed_event.ring_id);
                writer.u64(typed_event.slot_id);
                writer.u32(typed_event.width);
                writer.u32(typed_event.height);
                writer.u32(typed_event.rtp_timestamp);
                writer.u64(typed_event.receive_timestamp_ns);
                write_size_t_u64(writer, typed_event.payload_size);
                writer.u8(typed_event.partial ? 1 : 0);
                return std::move(writer).finish();
            } else if constexpr (std::is_same_v<Event, MtlWorkerAudioBlockReadyEvent>) {
                writer.u8(static_cast<std::uint8_t>(MessageTag::AudioBlockReadyEvent));
                writer.u64(typed_event.graph_id);
                writer.u64(typed_event.ring_id);
                writer.u64(typed_event.slot_id);
                writer.u32(typed_event.sample_rate_hz);
                writer.u32(typed_event.channels);
                writer.u32(typed_event.samples_per_channel);
                writer.u32(typed_event.rtp_timestamp);
                writer.u64(typed_event.receive_timestamp_ns);
                write_size_t_u64(writer, typed_event.payload_size);
                writer.u8(typed_event.partial ? 1 : 0);
                return std::move(writer).finish();
            } else {
                return std::unexpected(Error::Unsupported);
            }
        },
        event);
}

std::expected<MtlWorkerControlEvent, Error>
deserialize_mtl_worker_control_event(std::span<const std::uint8_t> payload) {
    Reader reader(payload);

    auto tag = reader.u8();
    if (!tag.has_value()) {
        return std::unexpected(tag.error());
    }

    switch (static_cast<MessageTag>(*tag)) {
    case MessageTag::HealthEvent: {
        auto request_id = reader.u64();
        if (!request_id.has_value()) {
            return std::unexpected(request_id.error());
        }

        auto healthy = read_bool_u8(reader);
        if (!healthy.has_value()) {
            return std::unexpected(healthy.error());
        }

        auto message = reader.string();
        if (!message.has_value()) {
            return std::unexpected(message.error());
        }

        auto consumed = ensure_consumed(reader);
        if (!consumed.has_value()) {
            return std::unexpected(consumed.error());
        }

        return MtlWorkerControlEvent{
            MtlWorkerHealthEvent{
                .request_id = *request_id,
                .healthy = *healthy,
                .message = std::move(*message),
            },
        };
    }

    case MessageTag::ErrorEvent: {
        auto request_id = reader.u64();
        if (!request_id.has_value()) {
            return std::unexpected(request_id.error());
        }

        auto graph_id = reader.u64();
        if (!graph_id.has_value()) {
            return std::unexpected(graph_id.error());
        }

        auto error = reader.u32();
        if (!error.has_value()) {
            return std::unexpected(error.error());
        }

        auto message = reader.string();
        if (!message.has_value()) {
            return std::unexpected(message.error());
        }

        auto consumed = ensure_consumed(reader);
        if (!consumed.has_value()) {
            return std::unexpected(consumed.error());
        }

        return MtlWorkerControlEvent{
            MtlWorkerErrorEvent{
                .request_id = *request_id,
                .graph_id = *graph_id,
                .error = static_cast<Error>(*error),
                .message = std::move(*message),
            },
        };
    }

    case MessageTag::StartedEvent: {
        auto request_id = reader.u64();
        if (!request_id.has_value()) {
            return std::unexpected(request_id.error());
        }

        auto graph_id = reader.u64();
        if (!graph_id.has_value()) {
            return std::unexpected(graph_id.error());
        }

        auto consumed = ensure_consumed(reader);
        if (!consumed.has_value()) {
            return std::unexpected(consumed.error());
        }

        return MtlWorkerControlEvent{
            MtlWorkerStartedEvent{
                .request_id = *request_id,
                .graph_id = *graph_id,
            },
        };
    }

    case MessageTag::StoppedEvent: {
        auto request_id = reader.u64();
        if (!request_id.has_value()) {
            return std::unexpected(request_id.error());
        }

        auto graph_id = reader.u64();
        if (!graph_id.has_value()) {
            return std::unexpected(graph_id.error());
        }

        auto consumed = ensure_consumed(reader);
        if (!consumed.has_value()) {
            return std::unexpected(consumed.error());
        }

        return MtlWorkerControlEvent{
            MtlWorkerStoppedEvent{
                .request_id = *request_id,
                .graph_id = *graph_id,
            },
        };
    }

    case MessageTag::StatsEvent: {
        auto request_id = reader.u64();
        auto graph_id = reader.u64();
        auto video_frames_received = reader.u64();
        auto audio_blocks_received = reader.u64();
        auto video_frames_dropped = reader.u64();
        auto audio_blocks_dropped = reader.u64();

        if (!request_id.has_value() || !graph_id.has_value() || !video_frames_received.has_value() ||
            !audio_blocks_received.has_value() || !video_frames_dropped.has_value() ||
            !audio_blocks_dropped.has_value()) {
            return std::unexpected(Error::InvalidValue);
        }

        auto consumed = ensure_consumed(reader);
        if (!consumed.has_value()) {
            return std::unexpected(consumed.error());
        }

        return MtlWorkerControlEvent{
            MtlWorkerStatsEvent{
                .request_id = *request_id,
                .graph_id = *graph_id,
                .video_frames_received = *video_frames_received,
                .audio_blocks_received = *audio_blocks_received,
                .video_frames_dropped = *video_frames_dropped,
                .audio_blocks_dropped = *audio_blocks_dropped,
            },
        };
    }

    case MessageTag::FrameReadyEvent: {
        auto graph_id = reader.u64();
        auto ring_id = reader.u64();
        auto slot_id = reader.u64();
        auto width = reader.u32();
        auto height = reader.u32();
        auto rtp_timestamp = reader.u32();
        auto receive_timestamp_ns = reader.u64();
        auto payload_size = read_size_t_u64(reader);
        auto partial = read_bool_u8(reader);

        if (!graph_id.has_value() || !ring_id.has_value() || !slot_id.has_value() || !width.has_value() ||
            !height.has_value() || !rtp_timestamp.has_value() || !receive_timestamp_ns.has_value() ||
            !payload_size.has_value() || !partial.has_value()) {
            return std::unexpected(Error::InvalidValue);
        }

        auto consumed = ensure_consumed(reader);
        if (!consumed.has_value()) {
            return std::unexpected(consumed.error());
        }

        return MtlWorkerControlEvent{
            MtlWorkerFrameReadyEvent{
                .graph_id = *graph_id,
                .ring_id = *ring_id,
                .slot_id = *slot_id,
                .width = *width,
                .height = *height,
                .rtp_timestamp = *rtp_timestamp,
                .receive_timestamp_ns = *receive_timestamp_ns,
                .payload_size = *payload_size,
                .partial = *partial,
            },
        };
    }

    case MessageTag::AudioBlockReadyEvent: {
        auto graph_id = reader.u64();
        auto ring_id = reader.u64();
        auto slot_id = reader.u64();
        auto sample_rate_hz = reader.u32();
        auto channels = reader.u32();
        auto samples_per_channel = reader.u32();
        auto rtp_timestamp = reader.u32();
        auto receive_timestamp_ns = reader.u64();
        auto payload_size = read_size_t_u64(reader);
        auto partial = read_bool_u8(reader);

        if (!graph_id.has_value() || !ring_id.has_value() || !slot_id.has_value() || !sample_rate_hz.has_value() ||
            !channels.has_value() || !samples_per_channel.has_value() || !rtp_timestamp.has_value() ||
            !receive_timestamp_ns.has_value() || !payload_size.has_value() || !partial.has_value()) {
            return std::unexpected(Error::InvalidValue);
        }

        auto consumed = ensure_consumed(reader);
        if (!consumed.has_value()) {
            return std::unexpected(consumed.error());
        }

        return MtlWorkerControlEvent{
            MtlWorkerAudioBlockReadyEvent{
                .graph_id = *graph_id,
                .ring_id = *ring_id,
                .slot_id = *slot_id,
                .sample_rate_hz = *sample_rate_hz,
                .channels = *channels,
                .samples_per_channel = *samples_per_channel,
                .rtp_timestamp = *rtp_timestamp,
                .receive_timestamp_ns = *receive_timestamp_ns,
                .payload_size = *payload_size,
                .partial = *partial,
            },
        };
    }

    default:
        return std::unexpected(Error::Unsupported);
    }
}

} // namespace st2110
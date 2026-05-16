#include <st2110/backends/mtl/mtl_worker_graph_client.hpp>

#include <st2110/backends/mtl/mtl_worker_shared_memory_ring_owner.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace st2110 {
namespace {

inline constexpr MtlWorkerSharedMemoryRingId videoRingId = 1;
inline constexpr MtlWorkerSharedMemoryRingId audioRingId = 2;

[[nodiscard]] MtlWorkerGraphId next_graph_id() noexcept {
    static std::atomic<MtlWorkerGraphId> next{1};
    return next.fetch_add(1, std::memory_order_relaxed);
}

[[nodiscard]] MtlWorkerRequestId next_request_id() noexcept {
    static std::atomic<MtlWorkerRequestId> next{1};
    return next.fetch_add(1, std::memory_order_relaxed);
}

[[nodiscard]] std::expected<std::uint64_t, Error> add_u64(const std::uint64_t a, const std::uint64_t b) noexcept {
    if (std::numeric_limits<std::uint64_t>::max() - a < b) {
        return std::unexpected(Error::InvalidValue);
    }

    return a + b;
}

[[nodiscard]] std::expected<std::uint64_t, Error> mul_u64(const std::uint64_t a, const std::uint64_t b) noexcept {
    if (a != 0 && b > std::numeric_limits<std::uint64_t>::max() / a) {
        return std::unexpected(Error::InvalidValue);
    }

    return a * b;
}

[[nodiscard]] std::expected<std::uint64_t, Error> ceil_div_u64(const std::uint64_t value,
                                                               const std::uint64_t divisor) noexcept {
    if (divisor == 0) {
        return std::unexpected(Error::InvalidValue);
    }

    const std::uint64_t quotient = value / divisor;
    const std::uint64_t remainder = value % divisor;

    if (remainder == 0) {
        return quotient;
    }

    return add_u64(quotient, 1);
}

[[nodiscard]] std::expected<std::uint64_t, Error> round_up_to_multiple_u64(const std::uint64_t value,
                                                                           const std::uint64_t multiple) noexcept {
    auto divided = ceil_div_u64(value, multiple);
    if (!divided.has_value()) {
        return std::unexpected(divided.error());
    }

    return mul_u64(*divided, multiple);
}

[[nodiscard]] std::expected<std::uint64_t, Error> pixel_count_u64(const std::uint32_t width,
                                                                  const std::uint32_t height) noexcept {
    if (width == 0 || height == 0) {
        return std::unexpected(Error::InvalidValue);
    }

    return mul_u64(width, height);
}

[[nodiscard]] std::expected<std::uint64_t, Error>
video_slot_payload_capacity_bytes(const MtlVideoStartConfig &cfg) noexcept {
    auto pixels = pixel_count_u64(cfg.width, cfg.height);
    if (!pixels.has_value()) {
        return std::unexpected(pixels.error());
    }

    const std::uint64_t width = cfg.width;
    const std::uint64_t height = cfg.height;

    switch (cfg.output_format) {
    case PixelFormat::UYVY:
    case PixelFormat::YUV422PLANAR8:
        return mul_u64(*pixels, 2);

    case PixelFormat::RGB8:
        return mul_u64(*pixels, 3);

    case PixelFormat::BGRA:
    case PixelFormat::ARGB:
    case PixelFormat::Y210:
    case PixelFormat::YUV422PLANAR10LE:
    case PixelFormat::YUV422PLANAR12LE:
    case PixelFormat::YUV422PLANAR16LE:
        return mul_u64(*pixels, 4);

    case PixelFormat::YUV444PLANAR10LE:
    case PixelFormat::YUV444PLANAR12LE:
        return mul_u64(*pixels, 6);

    case PixelFormat::YUV420PLANAR8: {
        auto triple = mul_u64(*pixels, 3);
        if (!triple.has_value()) {
            return std::unexpected(triple.error());
        }

        return ceil_div_u64(*triple, 2);
    }

    case PixelFormat::V210: {
        auto groups = ceil_div_u64(width, 6);
        if (!groups.has_value()) {
            return std::unexpected(groups.error());
        }

        auto bytes_per_line = mul_u64(*groups, 16);
        if (!bytes_per_line.has_value()) {
            return std::unexpected(bytes_per_line.error());
        }

        return mul_u64(*bytes_per_line, height);
    }

    case PixelFormat::YUV422RFC4175PG2BE10: {
        auto pairs = ceil_div_u64(width, 2);
        if (!pairs.has_value()) {
            return std::unexpected(pairs.error());
        }

        auto bytes_per_line = mul_u64(*pairs, 5);
        if (!bytes_per_line.has_value()) {
            return std::unexpected(bytes_per_line.error());
        }

        return mul_u64(*bytes_per_line, height);
    }

    case PixelFormat::YUV422RFC4175PG2BE12: {
        auto pairs = ceil_div_u64(width, 2);
        if (!pairs.has_value()) {
            return std::unexpected(pairs.error());
        }

        auto bytes_per_line = mul_u64(*pairs, 6);
        if (!bytes_per_line.has_value()) {
            return std::unexpected(bytes_per_line.error());
        }

        return mul_u64(*bytes_per_line, height);
    }

    case PixelFormat::YUV444RFC4175PG4BE10:
    case PixelFormat::RGBRFC4175PG4BE10: {
        auto groups = ceil_div_u64(width, 4);
        if (!groups.has_value()) {
            return std::unexpected(groups.error());
        }

        auto bytes_per_line = mul_u64(*groups, 15);
        if (!bytes_per_line.has_value()) {
            return std::unexpected(bytes_per_line.error());
        }

        return mul_u64(*bytes_per_line, height);
    }

    case PixelFormat::YUV444RFC4175PG2BE12:
    case PixelFormat::RGBRFC4175PG2BE12: {
        auto pairs = ceil_div_u64(width, 2);
        if (!pairs.has_value()) {
            return std::unexpected(pairs.error());
        }

        auto bytes_per_line = mul_u64(*pairs, 9);
        if (!bytes_per_line.has_value()) {
            return std::unexpected(bytes_per_line.error());
        }

        return mul_u64(*bytes_per_line, height);
    }
    }

    return std::unexpected(Error::Unsupported);
}

[[nodiscard]] std::expected<std::uint64_t, Error> audio_bytes_per_sample(const MtlAudioPcmFormat format) noexcept {
    switch (format) {
    case MtlAudioPcmFormat::Pcm16:
        return 2;
    case MtlAudioPcmFormat::Pcm24:
        return 3;
    }

    return std::unexpected(Error::Unsupported);
}

[[nodiscard]] std::expected<std::uint64_t, Error>
audio_slot_payload_capacity_bytes(const MtlAudioStartConfig &cfg) noexcept {
    if (cfg.media.sampling_rate_hz == 0 || cfg.media.channel_count == 0 || cfg.frame_buffer_duration_ns == 0) {
        return std::unexpected(Error::InvalidValue);
    }

    auto sample_rate_times_duration =
        mul_u64(static_cast<std::uint64_t>(cfg.media.sampling_rate_hz), cfg.frame_buffer_duration_ns);
    if (!sample_rate_times_duration.has_value()) {
        return std::unexpected(sample_rate_times_duration.error());
    }

    auto samples_per_channel = ceil_div_u64(*sample_rate_times_duration, 1'000'000'000ULL);
    if (!samples_per_channel.has_value()) {
        return std::unexpected(samples_per_channel.error());
    }

    auto bytes_per_sample = audio_bytes_per_sample(cfg.pcm_format);
    if (!bytes_per_sample.has_value()) {
        return std::unexpected(bytes_per_sample.error());
    }

    auto samples_all_channels = mul_u64(*samples_per_channel, cfg.media.channel_count);
    if (!samples_all_channels.has_value()) {
        return std::unexpected(samples_all_channels.error());
    }

    return mul_u64(*samples_all_channels, *bytes_per_sample);
}

[[nodiscard]] std::uint32_t ring_slot_count_from_frame_buffer_count(const std::uint16_t frame_buffer_count) noexcept {
    return std::max<std::uint32_t>(2, frame_buffer_count);
}

struct PreparedMediaRings {
    std::vector<MtlWorkerSharedMemoryRingOwner> owners{};
    std::vector<MtlWorkerSharedMemoryRingDescriptor> descriptors{};
    std::vector<int> file_descriptors{};
};

[[nodiscard]] std::expected<bool, Error> append_ring_owner(PreparedMediaRings &rings,
                                                           MtlWorkerSharedMemoryRingOwnerConfig cfg) {
    cfg.fd_index = static_cast<std::uint32_t>(rings.file_descriptors.size());

    auto owner = MtlWorkerSharedMemoryRingOwner::create(cfg);
    if (!owner.has_value()) {
        return std::unexpected(owner.error());
    }

    const int fd = owner->fd();
    const MtlWorkerSharedMemoryRingDescriptor descriptor = owner->descriptor();

    rings.owners.push_back(std::move(*owner));
    rings.descriptors.push_back(descriptor);
    rings.file_descriptors.push_back(fd);

    return true;
}

[[nodiscard]] std::expected<PreparedMediaRings, Error>
prepare_media_rings(const MtlWorkerGraphId graph_id, const std::optional<MtlVideoStartConfig> &video,
                    const std::optional<MtlAudioStartConfig> &audio) {
    PreparedMediaRings rings{};

    if (video.has_value()) {
        auto capacity = video_slot_payload_capacity_bytes(*video);
        if (!capacity.has_value()) {
            return std::unexpected(capacity.error());
        }

        auto appended = append_ring_owner(
            rings, MtlWorkerSharedMemoryRingOwnerConfig{
                       .ring_id = videoRingId,
                       .media_kind = MtlWorkerMediaKind::Video,
                       .fd_index = 0,
                       .slot_count = ring_slot_count_from_frame_buffer_count(video->frame_buffer_count),
                       .slot_payload_capacity_bytes = *capacity,
                       .debug_name = "st2110_mtl_video_graph_" + std::to_string(graph_id),
                   });

        if (!appended.has_value()) {
            return std::unexpected(appended.error());
        }
    }

    if (audio.has_value()) {
        auto capacity = audio_slot_payload_capacity_bytes(*audio);
        if (!capacity.has_value()) {
            return std::unexpected(capacity.error());
        }

        auto appended = append_ring_owner(
            rings, MtlWorkerSharedMemoryRingOwnerConfig{
                       .ring_id = audioRingId,
                       .media_kind = MtlWorkerMediaKind::Audio,
                       .fd_index = 0,
                       .slot_count = ring_slot_count_from_frame_buffer_count(audio->frame_buffer_count),
                       .slot_payload_capacity_bytes = *capacity,
                       .debug_name = "st2110_mtl_audio_graph_" + std::to_string(graph_id),
                   });

        if (!appended.has_value()) {
            return std::unexpected(appended.error());
        }
    }

    return rings;
}

[[nodiscard]] std::expected<MtlRuntimeConfig, Error>
resolve_graph_runtime_config(const std::optional<MtlVideoStartConfig> &video,
                             const std::optional<MtlAudioStartConfig> &audio) {
    if (!video.has_value() && !audio.has_value()) {
        return std::unexpected(Error::InvalidValue);
    }

    if (video.has_value()) {
        MtlRuntimeConfig runtime = video->runtime;

        if (audio.has_value() && audio->runtime != runtime) {
            return std::unexpected(Error::InvalidValue);
        }

        return runtime;
    }

    return audio->runtime;
}

[[nodiscard]] std::expected<bool, Error> interpret_start_sessions_event(const MtlWorkerControlEvent &event,
                                                                        const MtlWorkerRequestId expected_request_id,
                                                                        const MtlWorkerGraphId expected_graph_id) {
    return std::visit(
        [expected_request_id, expected_graph_id](const auto &typed_event) -> std::expected<bool, Error> {
            using Event = std::decay_t<decltype(typed_event)>;

            if constexpr (std::is_same_v<Event, MtlWorkerStartedEvent>) {
                if (typed_event.request_id != expected_request_id || typed_event.graph_id != expected_graph_id) {
                    return std::unexpected(Error::InvalidBackendState);
                }

                return true;
            } else if constexpr (std::is_same_v<Event, MtlWorkerErrorEvent>) {
                if (typed_event.request_id != expected_request_id) {
                    return std::unexpected(Error::InvalidBackendState);
                }

                return std::unexpected(typed_event.error);
            } else {
                return std::unexpected(Error::InvalidBackendState);
            }
        },
        event);
}

[[nodiscard]] std::expected<bool, Error> interpret_stop_sessions_event(const MtlWorkerControlEvent &event,
                                                                       const MtlWorkerRequestId expected_request_id,
                                                                       const MtlWorkerGraphId expected_graph_id) {
    return std::visit(
        [expected_request_id, expected_graph_id](const auto &typed_event) -> std::expected<bool, Error> {
            using Event = std::decay_t<decltype(typed_event)>;

            if constexpr (std::is_same_v<Event, MtlWorkerStoppedEvent>) {
                if (typed_event.request_id != expected_request_id || typed_event.graph_id != expected_graph_id) {
                    return std::unexpected(Error::InvalidBackendState);
                }

                return true;
            } else if constexpr (std::is_same_v<Event, MtlWorkerErrorEvent>) {
                if (typed_event.request_id != expected_request_id) {
                    return std::unexpected(Error::InvalidBackendState);
                }

                return std::unexpected(typed_event.error);
            } else {
                return std::unexpected(Error::InvalidBackendState);
            }
        },
        event);
}

[[nodiscard]] std::expected<MtlWorkerStatsEvent, Error>
interpret_stats_event(const MtlWorkerControlEvent &event, const MtlWorkerRequestId expected_request_id,
                      const MtlWorkerGraphId expected_graph_id) {
    return std::visit(
        [expected_request_id, expected_graph_id](const auto &typed_event) -> std::expected<MtlWorkerStatsEvent, Error> {
            using Event = std::decay_t<decltype(typed_event)>;

            if constexpr (std::is_same_v<Event, MtlWorkerStatsEvent>) {
                if (typed_event.request_id != expected_request_id || typed_event.graph_id != expected_graph_id) {
                    return std::unexpected(Error::InvalidBackendState);
                }

                return typed_event;
            } else if constexpr (std::is_same_v<Event, MtlWorkerErrorEvent>) {
                if (typed_event.request_id != expected_request_id) {
                    return std::unexpected(Error::InvalidBackendState);
                }

                return std::unexpected(typed_event.error);
            } else {
                return std::unexpected(Error::InvalidBackendState);
            }
        },
        event);
}

struct MtlWorkerGraphClientAsyncStatsSnapshot {
    std::uint64_t frame_ready_events = 0;
    std::uint64_t audio_block_ready_events = 0;
    std::uint64_t video_frames_delivered = 0;
    std::uint64_t audio_blocks_delivered = 0;
    std::uint64_t released_slots = 0;
    std::uint64_t malformed_ready_events = 0;
    std::uint64_t stale_ready_events = 0;
    std::uint64_t delivery_failures = 0;
    std::uint64_t release_failures = 0;
    std::uint64_t ignored_events = 0;
};

[[nodiscard]] MtlWorkerStatsEvent
merge_async_stats(MtlWorkerStatsEvent stats, const MtlWorkerGraphClientAsyncStatsSnapshot &async_stats) noexcept {
    stats.frame_ready_events = async_stats.frame_ready_events;
    stats.audio_block_ready_events = async_stats.audio_block_ready_events;
    stats.video_frames_delivered = async_stats.video_frames_delivered;
    stats.audio_blocks_delivered = async_stats.audio_blocks_delivered;
    stats.released_slots = async_stats.released_slots;
    stats.malformed_ready_events = async_stats.malformed_ready_events;
    stats.stale_ready_events = async_stats.stale_ready_events;
    stats.delivery_failures = async_stats.delivery_failures;
    stats.release_failures = async_stats.release_failures;
    stats.ignored_events = async_stats.ignored_events;
    return stats;
}

} // namespace

struct MtlWorkerGraphClientAsyncState {
    MtlWorkerGraphClientAsyncState(MtlWorkerGraphId graph, std::optional<MtlVideoStartConfig> video,
                                   std::optional<MtlAudioStartConfig> audio, IFrameSink *attached_sink,
                                   std::vector<MtlWorkerSharedMemoryRingOwner> ring_owners)
        : graph_id(graph), video_config(std::move(video)), audio_config(std::move(audio)), sink(attached_sink),
          media_ring_owners(std::move(ring_owners)) {}

    MtlWorkerGraphId graph_id = 0;

    std::mutex mutex{};
    bool active = true;

    std::optional<MtlVideoStartConfig> video_config{};
    std::optional<MtlAudioStartConfig> audio_config{};
    IFrameSink *sink = nullptr;

    std::vector<MtlWorkerSharedMemoryRingOwner> media_ring_owners{};

    std::uint64_t frame_ready_events = 0;
    std::uint64_t audio_block_ready_events = 0;
    std::uint64_t video_frames_delivered = 0;
    std::uint64_t audio_blocks_delivered = 0;
    std::uint64_t released_slots = 0;
    std::uint64_t malformed_ready_events = 0;
    std::uint64_t stale_ready_events = 0;
    std::uint64_t delivery_failures = 0;
    std::uint64_t release_failures = 0;
    std::uint64_t ignored_events = 0;

    void deactivate_noexcept() noexcept {
        try {
            std::lock_guard lock(mutex);
            active = false;
            sink = nullptr;
        } catch (...) {
        }
    }

    void detach_sink_noexcept(IFrameSink *detached_sink) noexcept {
        try {
            std::lock_guard lock(mutex);

            if (!detached_sink || sink == detached_sink) {
                sink = nullptr;
            }
        } catch (...) {
        }
    }

    [[nodiscard]] MtlWorkerGraphClientAsyncStatsSnapshot snapshot_noexcept() noexcept {
        try {
            std::lock_guard lock(mutex);

            return MtlWorkerGraphClientAsyncStatsSnapshot{
                .frame_ready_events = frame_ready_events,
                .audio_block_ready_events = audio_block_ready_events,
                .video_frames_delivered = video_frames_delivered,
                .audio_blocks_delivered = audio_blocks_delivered,
                .released_slots = released_slots,
                .malformed_ready_events = malformed_ready_events,
                .stale_ready_events = stale_ready_events,
                .delivery_failures = delivery_failures,
                .release_failures = release_failures,
                .ignored_events = ignored_events,
            };
        } catch (...) {
            return {};
        }
    }

    [[nodiscard]] MtlWorkerSharedMemoryRingOwner *
    find_ring_owner_no_lock(const MtlWorkerMediaKind media_kind, const MtlWorkerSharedMemoryRingId ring_id) noexcept {
        for (auto &owner : media_ring_owners) {
            if (!owner.valid()) {
                continue;
            }

            const auto &descriptor = owner.descriptor();
            if (descriptor.media_kind == media_kind && descriptor.ring_id == ring_id) {
                return &owner;
            }
        }

        return nullptr;
    }

    [[nodiscard]] bool validate_common_ready_event_no_lock(const MtlWorkerGraphId event_graph_id,
                                                           const MtlWorkerSlotId event_slot_id) noexcept {
        if (event_graph_id != graph_id) {
            ++ignored_events;
            return false;
        }

        if (event_slot_id > static_cast<MtlWorkerSlotId>(std::numeric_limits<std::uint32_t>::max())) {
            ++malformed_ready_events;
            return false;
        }

        return true;
    }

    [[nodiscard]] bool begin_ready_slot_no_lock(MtlWorkerSharedMemoryRingMap &ring, const MtlWorkerSlotId slot_id,
                                                std::uint32_t &out_slot_index) noexcept {
        const auto &descriptor = ring.descriptor();
        const auto slot_index = static_cast<std::uint32_t>(slot_id);

        if (slot_index >= descriptor.slot_count) {
            ++malformed_ready_events;
            return false;
        }

        auto began = ring.begin_read_slot(slot_index);
        if (!began.has_value()) {
            ++release_failures;
            return false;
        }

        if (!*began) {
            ++ignored_events;
            return false;
        }

        out_slot_index = slot_index;
        return true;
    }

    [[nodiscard]] bool validate_ready_payload_no_lock(MtlWorkerSharedMemoryRingMap &ring,
                                                  const std::uint32_t slot_index,
                                                  const std::uint64_t event_sequence,
                                                  const std::size_t event_payload_size) noexcept {
        bool malformed = false;
        bool stale = false;

        auto header = ring.slot_header(slot_index);
        if (!header.has_value()) {
            malformed = true;
        }

        auto payload = ring.slot_payload(slot_index);
        if (!payload.has_value()) {
            malformed = true;
        }

        if (payload.has_value() && event_payload_size > payload->size()) {
            malformed = true;
        }

        if (header.has_value()) {
            if ((*header)->sequence != event_sequence) {
                stale = true;
            }

            if ((*header)->payload_size != static_cast<std::uint64_t>(event_payload_size)) {
                stale = true;
            }
        }

        if (malformed) {
            ++malformed_ready_events;
            return false;
        }

        if (stale) {
            ++stale_ready_events;
            return false;
        }

        return true;
    }

    void deliver_video_frame_no_lock(MtlWorkerSharedMemoryRingMap &ring, const std::uint32_t slot_index,
                                     const MtlWorkerFrameReadyEvent &event) noexcept {
        if (!sink || !video_config.has_value()) {
            return;
        }

        if (event.width == 0 || event.height == 0 || event.payload_size == 0) {
            ++malformed_ready_events;
            return;
        }

        auto payload = ring.slot_payload(slot_index);
        if (!payload.has_value()) {
            ++malformed_ready_events;
            return;
        }

        try {
            VideoFrame frame{event.width, event.height, video_config->output_format};

            if (frame.size_bytes() != event.payload_size || frame.size_bytes() > payload->size()) {
                ++malformed_ready_events;
                return;
            }

            std::memcpy(frame.data(), payload->data(), frame.size_bytes());

            sink->on_video_frame(std::move(frame), FrameTimingMetadata{
                                                       .rtp_timestamp = event.rtp_timestamp,
                                                       .receive_timestamp_ns = event.receive_timestamp_ns,
                                                   });

            ++video_frames_delivered;
        } catch (...) {
            ++delivery_failures;
        }
    }

    [[nodiscard]] static std::expected<std::size_t, Error>
    audio_bytes_per_sample(const MtlAudioPcmFormat format) noexcept {
        switch (format) {
        case MtlAudioPcmFormat::Pcm16:
            return 2;
        case MtlAudioPcmFormat::Pcm24:
            return 3;
        }

        return std::unexpected(Error::Unsupported);
    }

    [[nodiscard]] static std::uint8_t byte_to_u8(const std::byte value) noexcept {
        return static_cast<std::uint8_t>(value);
    }

    [[nodiscard]] static std::int32_t pcm16be_to_s32(const std::byte *src) noexcept {
        const std::uint16_t raw = static_cast<std::uint16_t>((static_cast<std::uint16_t>(byte_to_u8(src[0])) << 8) |
                                                             static_cast<std::uint16_t>(byte_to_u8(src[1])));

        const auto signed_16 = static_cast<std::int16_t>(raw);

        /*
         * Convert to left-aligned S32. Use multiplication rather than shifting a
         * negative signed value.
         */
        return static_cast<std::int32_t>(signed_16) * 65536;
    }

    [[nodiscard]] static std::int32_t pcm24be_to_s32(const std::byte *src) noexcept {
        std::uint32_t raw = (static_cast<std::uint32_t>(byte_to_u8(src[0])) << 16) |
                            (static_cast<std::uint32_t>(byte_to_u8(src[1])) << 8) |
                            static_cast<std::uint32_t>(byte_to_u8(src[2]));

        if ((raw & 0x0080'0000u) != 0) {
            raw |= 0xFF00'0000u;
        }

        const auto signed_24 = static_cast<std::int32_t>(raw);

        /*
         * Convert to left-aligned S32. PCM24 full scale maps into the high 24 bits.
         */
        return signed_24 * 256;
    }

    [[nodiscard]] bool convert_interleaved_pcm_to_s32_no_lock(std::span<const std::byte> payload,
                                                              const MtlAudioPcmFormat format,
                                                              AudioBuffer &out) noexcept {
        auto bytes_per_sample = audio_bytes_per_sample(format);
        if (!bytes_per_sample.has_value()) {
            ++delivery_failures;
            return false;
        }

        const std::uint64_t expected_samples =
            static_cast<std::uint64_t>(out.samples_per_channel()) * static_cast<std::uint64_t>(out.channel_count());

        if (expected_samples > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
            ++malformed_ready_events;
            return false;
        }

        const auto sample_count = static_cast<std::size_t>(expected_samples);
        const std::uint64_t expected_bytes = expected_samples * static_cast<std::uint64_t>(*bytes_per_sample);

        if (expected_bytes > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()) ||
            payload.size() != static_cast<std::size_t>(expected_bytes) || out.total_sample_count() != sample_count) {
            ++malformed_ready_events;
            return false;
        }

        auto *dst = out.samples();

        for (std::size_t i = 0; i < sample_count; ++i) {
            const std::byte *src = payload.data() + i * *bytes_per_sample;

            switch (format) {
            case MtlAudioPcmFormat::Pcm16:
                dst[i] = pcm16be_to_s32(src);
                break;
            case MtlAudioPcmFormat::Pcm24:
                dst[i] = pcm24be_to_s32(src);
                break;
            }
        }

        return true;
    }

    void deliver_audio_block_no_lock(MtlWorkerSharedMemoryRingMap &ring, const std::uint32_t slot_index,
                                     const MtlWorkerAudioBlockReadyEvent &event) noexcept {
        if (!sink || !audio_config.has_value()) {
            return;
        }

        if (event.sample_rate_hz == 0 || event.channels == 0 || event.samples_per_channel == 0 ||
            event.payload_size == 0) {
            ++malformed_ready_events;
            return;
        }

        if (event.channels > static_cast<std::uint32_t>(std::numeric_limits<std::uint16_t>::max())) {
            ++malformed_ready_events;
            return;
        }

        if (event.sample_rate_hz != audio_config->media.sampling_rate_hz ||
            event.channels != audio_config->media.channel_count) {
            ++malformed_ready_events;
            return;
        }

        auto payload = ring.slot_payload(slot_index);
        if (!payload.has_value()) {
            ++malformed_ready_events;
            return;
        }

        if (event.payload_size > payload->size()) {
            ++malformed_ready_events;
            return;
        }

        try {
            AudioBuffer buffer{
                event.sample_rate_hz,
                static_cast<std::uint16_t>(event.channels),
                event.samples_per_channel,
            };

            const auto active_payload = std::span<const std::byte>{payload->data(), event.payload_size};

            if (!convert_interleaved_pcm_to_s32_no_lock(active_payload, audio_config->pcm_format, buffer)) {
                return;
            }

            sink->on_audio_frame(std::move(buffer), FrameTimingMetadata{
                                                        .rtp_timestamp = event.rtp_timestamp,
                                                        .receive_timestamp_ns = event.receive_timestamp_ns,
                                                    });

            ++audio_blocks_delivered;
        } catch (...) {
            ++delivery_failures;
        }
    }

    void consume_video_ready_slot_noexcept(const MtlWorkerFrameReadyEvent &event) noexcept {
        if (!validate_common_ready_event_no_lock(event.graph_id, event.slot_id)) {
            return;
        }

        auto *owner = find_ring_owner_no_lock(MtlWorkerMediaKind::Video, event.ring_id);
        if (!owner) {
            ++ignored_events;
            return;
        }

        auto &ring = owner->ring_map();

        std::uint32_t slot_index = 0;
        if (!begin_ready_slot_no_lock(ring, event.slot_id, slot_index)) {
            return;
        }

        const bool valid_payload = validate_ready_payload_no_lock(ring, slot_index, event.sequence, event.payload_size);
        if (valid_payload) {
            deliver_video_frame_no_lock(ring, slot_index, event);
        }

        auto released = ring.release_read_slot(slot_index);
        if (!released.has_value()) {
            ++release_failures;
            return;
        }

        ++frame_ready_events;
        ++released_slots;
    }

    void consume_audio_ready_slot_noexcept(const MtlWorkerAudioBlockReadyEvent &event) noexcept {
        if (!validate_common_ready_event_no_lock(event.graph_id, event.slot_id)) {
            return;
        }

        auto *owner = find_ring_owner_no_lock(MtlWorkerMediaKind::Audio, event.ring_id);
        if (!owner) {
            ++ignored_events;
            return;
        }

        auto &ring = owner->ring_map();

        std::uint32_t slot_index = 0;
        if (!begin_ready_slot_no_lock(ring, event.slot_id, slot_index)) {
            return;
        }

        const bool valid_payload = validate_ready_payload_no_lock(ring, slot_index, event.sequence, event.payload_size);
        if (valid_payload) {
            deliver_audio_block_no_lock(ring, slot_index, event);
        }

        auto released = ring.release_read_slot(slot_index);
        if (!released.has_value()) {
            ++release_failures;
            return;
        }

        ++audio_block_ready_events;
        ++released_slots;
    }

    void handle_event_noexcept(MtlWorkerControlEventEnvelope envelope) noexcept {
        try {
            std::lock_guard lock(mutex);

            if (!active) {
                return;
            }

            std::visit(
                [this](const auto &typed_event) noexcept {
                    using Event = std::decay_t<decltype(typed_event)>;

                    if constexpr (std::is_same_v<Event, MtlWorkerFrameReadyEvent>) {
                        consume_video_ready_slot_noexcept(typed_event);
                    } else if constexpr (std::is_same_v<Event, MtlWorkerAudioBlockReadyEvent>) {
                        consume_audio_ready_slot_noexcept(typed_event);
                    } else {
                        ++ignored_events;
                    }
                },
                envelope.event);
        } catch (...) {
        }
    }
};

struct MtlWorkerGraphClient::Impl {
    MtlWorkerGraphId graph_id = next_graph_id();

    std::optional<MtlVideoStartConfig> video{};
    std::optional<MtlAudioStartConfig> audio{};

    IFrameSink *sink = nullptr;

    std::optional<MtlWorkerManager::WorkerLease> worker_lease{};
    std::shared_ptr<MtlWorkerGraphClientAsyncState> async_event_state{};
    bool async_event_handler_registered = false;
    bool running = false;
    std::uint32_t active_start_count = 0;

    void unregister_async_event_handler_noexcept() noexcept {
        if (async_event_state) {
            async_event_state->deactivate_noexcept();
        }

        if (async_event_handler_registered && worker_lease.has_value() && worker_lease->control_channel) {
            worker_lease->control_channel->unregister_async_event_handler_noexcept(graph_id);
        }

        async_event_handler_registered = false;
    }
};

MtlWorkerGraphClient::MtlWorkerGraphClient() : impl_(std::make_unique<Impl>()) {}

MtlWorkerGraphClient::~MtlWorkerGraphClient() { stop_noexcept(); }

std::expected<bool, Error> MtlWorkerGraphClient::configure_video(MtlVideoStartConfig cfg) {
    if (impl_->running) {
        return std::unexpected(Error::InvalidBackendState);
    }

    if (impl_->audio.has_value() && impl_->audio->runtime != cfg.runtime) {
        return std::unexpected(Error::InvalidValue);
    }

    impl_->video = std::move(cfg);
    return true;
}

std::expected<bool, Error> MtlWorkerGraphClient::configure_audio(MtlAudioStartConfig cfg) {
    if (impl_->running) {
        return std::unexpected(Error::InvalidBackendState);
    }

    if (impl_->video.has_value() && impl_->video->runtime != cfg.runtime) {
        return std::unexpected(Error::InvalidValue);
    }

    impl_->audio = std::move(cfg);
    return true;
}

std::expected<bool, Error> MtlWorkerGraphClient::attach_sink(IFrameSink *sink) {
    if (!sink) {
        return std::unexpected(Error::InvalidValue);
    }

    if (impl_->sink && impl_->sink != sink) {
        return std::unexpected(Error::InvalidBackendState);
    }

    impl_->sink = sink;
    return true;
}

void MtlWorkerGraphClient::detach_sink_noexcept(IFrameSink *sink) noexcept {
    if (impl_->async_event_state) {
        impl_->async_event_state->detach_sink_noexcept(sink);
    }

    if (!sink || impl_->sink == sink) {
        impl_->sink = nullptr;
    }
}

std::expected<bool, Error> MtlWorkerGraphClient::start() {
    if (impl_->running) {
        ++impl_->active_start_count;
        return true;
    }

    if (!impl_->sink) {
        return std::unexpected(Error::InvalidBackendState);
    }

    auto runtime = resolve_graph_runtime_config(impl_->video, impl_->audio);
    if (!runtime.has_value()) {
        return std::unexpected(runtime.error());
    }

    auto lease = default_mtl_worker_manager().acquire_or_spawn_compatible_worker(*runtime);
    if (!lease.has_value()) {
        return std::unexpected(lease.error());
    }

    impl_->worker_lease = *lease;

    if (!impl_->worker_lease->control_channel) {
        impl_->worker_lease.reset();
        return std::unexpected(Error::InvalidBackendState);
    }

    auto prepared_rings = prepare_media_rings(impl_->graph_id, impl_->video, impl_->audio);
    if (!prepared_rings.has_value()) {
        impl_->worker_lease.reset();
        return std::unexpected(prepared_rings.error());
    }

    auto request = make_start_sessions_request();
    if (!request.has_value()) {
        impl_->worker_lease.reset();
        return std::unexpected(request.error());
    }

    request->media_rings = prepared_rings->descriptors;

    impl_->async_event_state = std::make_shared<MtlWorkerGraphClientAsyncState>(
        impl_->graph_id, impl_->video, impl_->audio, impl_->sink, std::move(prepared_rings->owners));

    auto registered_async_handler = impl_->worker_lease->control_channel->register_async_event_handler(
        impl_->graph_id, [state = impl_->async_event_state](MtlWorkerControlEventEnvelope envelope) {
            if (state) {
                state->handle_event_noexcept(std::move(envelope));
            }
        });

    if (!registered_async_handler.has_value()) {
        impl_->async_event_state.reset();
        impl_->worker_lease.reset();
        return std::unexpected(registered_async_handler.error());
    }

    impl_->async_event_handler_registered = true;

    const MtlWorkerRequestId request_id = request->request_id;

    auto envelope = impl_->worker_lease->control_channel->transact_with_fds(MtlWorkerControlRequest{*request},
                                                                            prepared_rings->file_descriptors);
    if (!envelope.has_value()) {
        impl_->unregister_async_event_handler_noexcept();
        impl_->async_event_state.reset();
        impl_->worker_lease.reset();
        return std::unexpected(envelope.error());
    }

    auto started = interpret_start_sessions_event(envelope->event, request_id, impl_->graph_id);
    if (!started.has_value()) {
        impl_->unregister_async_event_handler_noexcept();
        impl_->async_event_state.reset();
        impl_->worker_lease.reset();
        return std::unexpected(started.error());
    }

    impl_->running = true;
    impl_->active_start_count = 1;
    return true;
}

std::expected<bool, Error> MtlWorkerGraphClient::stop() {
    if (!impl_->running) {
        impl_->active_start_count = 0;
        impl_->unregister_async_event_handler_noexcept();
        impl_->async_event_state.reset();
        return true;
    }

    if (impl_->active_start_count > 1) {
        --impl_->active_start_count;
        return true;
    }

    if (!impl_->worker_lease.has_value() || !impl_->worker_lease->control_channel) {
        impl_->running = false;
        impl_->active_start_count = 0;
        impl_->unregister_async_event_handler_noexcept();
        impl_->async_event_state.reset();
        impl_->worker_lease.reset();
        return std::unexpected(Error::InvalidBackendState);
    }

    auto request = make_stop_sessions_request();
    const MtlWorkerRequestId request_id = request.request_id;

    auto event = impl_->worker_lease->control_channel->transact(MtlWorkerControlRequest{request});
    if (!event.has_value()) {
        impl_->running = false;
        impl_->active_start_count = 0;
        impl_->unregister_async_event_handler_noexcept();
        impl_->async_event_state.reset();
        impl_->worker_lease.reset();
        return std::unexpected(event.error());
    }

    auto stopped = interpret_stop_sessions_event(*event, request_id, impl_->graph_id);
    if (!stopped.has_value()) {
        impl_->running = false;
        impl_->active_start_count = 0;
        impl_->unregister_async_event_handler_noexcept();
        impl_->async_event_state.reset();
        impl_->worker_lease.reset();
        return std::unexpected(stopped.error());
    }

    impl_->running = false;
    impl_->active_start_count = 0;
    impl_->unregister_async_event_handler_noexcept();
    impl_->async_event_state.reset();

    return true;
}

std::expected<MtlWorkerStatsEvent, Error> MtlWorkerGraphClient::stats() {
    auto runtime = resolve_graph_runtime_config(impl_->video, impl_->audio);
    if (!runtime.has_value()) {
        return std::unexpected(runtime.error());
    }

    auto lease = default_mtl_worker_manager().acquire_or_spawn_compatible_worker(*runtime);
    if (!lease.has_value()) {
        return std::unexpected(lease.error());
    }

    impl_->worker_lease = *lease;

    if (!impl_->worker_lease->control_channel) {
        return std::unexpected(Error::InvalidBackendState);
    }

    auto request = make_stats_request();
    if (!request.has_value()) {
        return std::unexpected(request.error());
    }

    const MtlWorkerRequestId request_id = request->request_id;

    auto event = impl_->worker_lease->control_channel->transact(MtlWorkerControlRequest{*request});
    if (!event.has_value()) {
        return std::unexpected(event.error());
    }

    auto stats = interpret_stats_event(*event, request_id, impl_->graph_id);
    if (!stats.has_value()) {
        return std::unexpected(stats.error());
    }

    MtlWorkerGraphClientAsyncStatsSnapshot async_snapshot{};
    if (impl_->async_event_state) {
        async_snapshot = impl_->async_event_state->snapshot_noexcept();
    }

    return merge_async_stats(*stats, async_snapshot);
}

void MtlWorkerGraphClient::stop_noexcept() noexcept {
    if (impl_->running) {
        /*
         * Force graph-level shutdown regardless of how many media proxy
         * backends previously called start().
         */
        impl_->active_start_count = 1;
    }

    (void)stop();
}

std::expected<MtlWorkerStartSessionsRequest, Error> MtlWorkerGraphClient::make_start_sessions_request() const {
    if (!configured()) {
        return std::unexpected(Error::InvalidBackendState);
    }

    auto runtime = resolve_graph_runtime_config(impl_->video, impl_->audio);
    if (!runtime.has_value()) {
        return std::unexpected(runtime.error());
    }

    return MtlWorkerStartSessionsRequest{
        .request_id = next_request_id(),
        .graph_id = impl_->graph_id,
        .video = impl_->video,
        .audio = impl_->audio,
        .media_rings = {},
    };
}

MtlWorkerStopSessionsRequest MtlWorkerGraphClient::make_stop_sessions_request() const {
    return MtlWorkerStopSessionsRequest{
        .request_id = next_request_id(),
        .graph_id = impl_->graph_id,
    };
}

std::expected<MtlWorkerStatsRequest, Error> MtlWorkerGraphClient::make_stats_request() const {
    if (!configured()) {
        return std::unexpected(Error::InvalidBackendState);
    }

    return MtlWorkerStatsRequest{
        .request_id = next_request_id(),
        .graph_id = impl_->graph_id,
    };
}

bool MtlWorkerGraphClient::configured() const noexcept { return impl_->video.has_value() || impl_->audio.has_value(); }

bool MtlWorkerGraphClient::running() const noexcept { return impl_->running; }

MtlWorkerGraphId MtlWorkerGraphClient::graph_id() const noexcept { return impl_->graph_id; }

const std::optional<MtlVideoStartConfig> &MtlWorkerGraphClient::video_config() const noexcept { return impl_->video; }

const std::optional<MtlAudioStartConfig> &MtlWorkerGraphClient::audio_config() const noexcept { return impl_->audio; }

IFrameSink *MtlWorkerGraphClient::sink() const noexcept { return impl_->sink; }

} // namespace st2110
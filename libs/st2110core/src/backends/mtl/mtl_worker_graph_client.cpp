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
#include <span>
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

[[nodiscard]] std::expected<std::uint64_t, Error>
video_slot_payload_capacity_bytes(const MtlVideoStartConfig &cfg) noexcept {
    try {
        VideoFrame frame{cfg.width, cfg.height, cfg.output_format};
        return static_cast<std::uint64_t>(frame.size_bytes());
    } catch (...) {
        return std::unexpected(Error::Unsupported);
    }
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

[[nodiscard]] std::string compose_error_detail_message(const char *operation, const Error error,
                                                       const std::string &detail) {
    std::string message = operation ? operation : "MTL worker operation";
    message += " failed";

    if (!detail.empty()) {
        message += ": ";
        message += detail;
    }

    message += " (";
    message += to_string(error);
    message += ")";

    return message;
}

[[nodiscard]] MtlWorkerErrorDetail make_worker_error_detail(const MtlWorkerErrorEvent &event, const char *operation) {
    return MtlWorkerErrorDetail{
        .error = event.error,
        .request_id = event.request_id,
        .graph_id = event.graph_id,
        .message = compose_error_detail_message(operation, event.error, event.message),
        .worker_side = true,
    };
}

[[nodiscard]] MtlWorkerErrorDetail make_worker_health_detail(const MtlWorkerHealthEvent &event, const Error error,
                                                             const MtlWorkerGraphId graph_id, const char *operation) {
    return MtlWorkerErrorDetail{
        .error = error,
        .request_id = event.request_id,
        .graph_id = graph_id,
        .message = compose_error_detail_message(operation, error, event.message),
        .worker_side = true,
    };
}

[[nodiscard]] MtlWorkerErrorDetail make_local_error_detail(const Error error, const MtlWorkerRequestId request_id,
                                                           const MtlWorkerGraphId graph_id, const char *operation,
                                                           const char *detail) {
    return MtlWorkerErrorDetail{
        .error = error,
        .request_id = request_id,
        .graph_id = graph_id,
        .message = compose_error_detail_message(operation, error, detail ? detail : ""),
        .worker_side = false,
    };
}

[[nodiscard]] std::expected<bool, Error> interpret_start_sessions_event(const MtlWorkerControlEvent &event,
                                                                        const MtlWorkerRequestId expected_request_id,
                                                                        const MtlWorkerGraphId expected_graph_id,
                                                                        std::optional<MtlWorkerErrorDetail> *detail) {
    return std::visit(
        [expected_request_id, expected_graph_id, detail](const auto &typed_event) -> std::expected<bool, Error> {
            using Event = std::decay_t<decltype(typed_event)>;

            if constexpr (std::is_same_v<Event, MtlWorkerStartedEvent>) {
                if (typed_event.request_id != expected_request_id || typed_event.graph_id != expected_graph_id) {
                    if (detail) {
                        *detail = make_local_error_detail(
                            Error::InvalidBackendState, expected_request_id, expected_graph_id, "StartSessions",
                            "worker returned StartedEvent with unexpected request_id or graph_id");
                    }

                    return std::unexpected(Error::InvalidBackendState);
                }

                return true;
            } else if constexpr (std::is_same_v<Event, MtlWorkerErrorEvent>) {
                if (typed_event.request_id != expected_request_id) {
                    if (detail) {
                        *detail = make_local_error_detail(Error::InvalidBackendState, expected_request_id,
                                                          expected_graph_id, "StartSessions",
                                                          "worker returned ErrorEvent for a different request_id");
                    }

                    return std::unexpected(Error::InvalidBackendState);
                }

                if (detail) {
                    *detail = make_worker_error_detail(typed_event, "StartSessions");
                }

                return std::unexpected(typed_event.error);
            } else {
                if (detail) {
                    *detail =
                        make_local_error_detail(Error::InvalidBackendState, expected_request_id, expected_graph_id,
                                                "StartSessions", "worker returned an unexpected event type");
                }

                return std::unexpected(Error::InvalidBackendState);
            }
        },
        event);
}

[[nodiscard]] std::expected<bool, Error> interpret_stop_sessions_event(const MtlWorkerControlEvent &event,
                                                                       const MtlWorkerRequestId expected_request_id,
                                                                       const MtlWorkerGraphId expected_graph_id,
                                                                       std::optional<MtlWorkerErrorDetail> *detail) {
    return std::visit(
        [expected_request_id, expected_graph_id, detail](const auto &typed_event) -> std::expected<bool, Error> {
            using Event = std::decay_t<decltype(typed_event)>;

            if constexpr (std::is_same_v<Event, MtlWorkerStoppedEvent>) {
                if (typed_event.request_id != expected_request_id || typed_event.graph_id != expected_graph_id) {
                    if (detail) {
                        *detail = make_local_error_detail(
                            Error::InvalidBackendState, expected_request_id, expected_graph_id, "StopSessions",
                            "worker returned StoppedEvent with unexpected request_id or graph_id");
                    }

                    return std::unexpected(Error::InvalidBackendState);
                }

                return true;
            } else if constexpr (std::is_same_v<Event, MtlWorkerErrorEvent>) {
                if (typed_event.request_id != expected_request_id) {
                    if (detail) {
                        *detail = make_local_error_detail(Error::InvalidBackendState, expected_request_id,
                                                          expected_graph_id, "StopSessions",
                                                          "worker returned ErrorEvent for a different request_id");
                    }

                    return std::unexpected(Error::InvalidBackendState);
                }

                if (detail) {
                    *detail = make_worker_error_detail(typed_event, "StopSessions");
                }

                return std::unexpected(typed_event.error);
            } else {
                if (detail) {
                    *detail =
                        make_local_error_detail(Error::InvalidBackendState, expected_request_id, expected_graph_id,
                                                "StopSessions", "worker returned an unexpected event type");
                }

                return std::unexpected(Error::InvalidBackendState);
            }
        },
        event);
}

[[nodiscard]] std::expected<bool, Error> interpret_health_event(const MtlWorkerControlEvent &event,
                                                                const MtlWorkerRequestId expected_request_id,
                                                                std::optional<MtlWorkerErrorDetail> *detail) {
    return std::visit(
        [expected_request_id, detail](const auto &typed_event) -> std::expected<bool, Error> {
            using Event = std::decay_t<decltype(typed_event)>;

            if constexpr (std::is_same_v<Event, MtlWorkerHealthEvent>) {
                if (typed_event.request_id != expected_request_id) {
                    if (detail) {
                        *detail =
                            make_local_error_detail(Error::InvalidBackendState, expected_request_id, 0, "HealthCheck",
                                                    "worker returned HealthEvent for a different request_id");
                    }

                    return std::unexpected(Error::InvalidBackendState);
                }

                if (typed_event.healthy) {
                    return true;
                }

                if (detail) {
                    *detail = make_worker_health_detail(typed_event, Error::InvalidBackendState, 0, "HealthCheck");
                }

                return std::unexpected(Error::InvalidBackendState);
            } else if constexpr (std::is_same_v<Event, MtlWorkerErrorEvent>) {
                if (typed_event.request_id != expected_request_id) {
                    if (detail) {
                        *detail =
                            make_local_error_detail(Error::InvalidBackendState, expected_request_id, 0, "HealthCheck",
                                                    "worker returned ErrorEvent for a different request_id");
                    }

                    return std::unexpected(Error::InvalidBackendState);
                }

                if (detail) {
                    *detail = make_worker_error_detail(typed_event, "HealthCheck");
                }

                return std::unexpected(typed_event.error);
            } else {
                if (detail) {
                    *detail = make_local_error_detail(Error::InvalidBackendState, expected_request_id, 0, "HealthCheck",
                                                      "worker returned an unexpected event type");
                }

                return std::unexpected(Error::InvalidBackendState);
            }
        },
        event);
}

[[nodiscard]] std::expected<bool, Error> check_worker_health(IMtlWorkerControlChannel &channel,
                                                             std::optional<MtlWorkerErrorDetail> *detail) {
    const MtlWorkerRequestId request_id = next_request_id();

    auto event = channel.transact(MtlWorkerControlRequest{
        MtlWorkerHealthCheckRequest{
            .request_id = request_id,
        },
    });

    if (!event.has_value()) {
        if (detail) {
            *detail = make_local_error_detail(event.error(), request_id, 0, "HealthCheck", "IPC transaction failed");
        }

        return std::unexpected(event.error());
    }

    return interpret_health_event(*event, request_id, detail);
}

[[nodiscard]] std::expected<MtlWorkerStatsEvent, Error>
interpret_stats_event(const MtlWorkerControlEvent &event, const MtlWorkerRequestId expected_request_id,
                      const MtlWorkerGraphId expected_graph_id, std::optional<MtlWorkerErrorDetail> *detail) {
    return std::visit(
        [expected_request_id, expected_graph_id,
         detail](const auto &typed_event) -> std::expected<MtlWorkerStatsEvent, Error> {
            using Event = std::decay_t<decltype(typed_event)>;

            if constexpr (std::is_same_v<Event, MtlWorkerStatsEvent>) {
                if (typed_event.request_id != expected_request_id || typed_event.graph_id != expected_graph_id) {
                    if (detail) {
                        *detail = make_local_error_detail(
                            Error::InvalidBackendState, expected_request_id, expected_graph_id, "Stats",
                            "worker returned StatsEvent with unexpected request_id or graph_id");
                    }

                    return std::unexpected(Error::InvalidBackendState);
                }

                return typed_event;
            } else if constexpr (std::is_same_v<Event, MtlWorkerErrorEvent>) {
                if (typed_event.request_id != expected_request_id) {
                    if (detail) {
                        *detail =
                            make_local_error_detail(Error::InvalidBackendState, expected_request_id, expected_graph_id,
                                                    "Stats", "worker returned ErrorEvent for a different request_id");
                    }

                    return std::unexpected(Error::InvalidBackendState);
                }

                if (detail) {
                    *detail = make_worker_error_detail(typed_event, "Stats");
                }

                return std::unexpected(typed_event.error);
            } else {
                if (detail) {
                    *detail =
                        make_local_error_detail(Error::InvalidBackendState, expected_request_id, expected_graph_id,
                                                "Stats", "worker returned an unexpected event type");
                }

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

[[nodiscard]] std::expected<VideoScanMode, Error> decode_video_scan_mode(const std::uint32_t raw) noexcept {
    switch (static_cast<VideoScanMode>(raw)) {
    case VideoScanMode::Progressive:
        return VideoScanMode::Progressive;
    case VideoScanMode::Interlaced:
        return VideoScanMode::Interlaced;
    case VideoScanMode::PsF:
        return VideoScanMode::PsF;
    }

    return std::unexpected(Error::InvalidValue);
}

[[nodiscard]] bool video_field_flag_set(const std::uint32_t flags, const MtlWorkerVideoFieldFlags flag) noexcept {
    return (flags & static_cast<std::uint32_t>(flag)) != 0;
}

[[nodiscard]] bool video_scan_field_metadata_is_consistent(const VideoScanMode scan_mode,
                                                           const std::uint32_t field_flags) noexcept {
    if ((field_flags & ~mtlWorkerVideoFieldFlagsMask) != 0) {
        return false;
    }

    const bool interlaced = video_field_flag_set(field_flags, MtlWorkerVideoFieldFlags::Interlaced);
    const bool second_field = video_field_flag_set(field_flags, MtlWorkerVideoFieldFlags::SecondField);

    switch (scan_mode) {
    case VideoScanMode::Progressive:
        return !interlaced && !second_field;

    case VideoScanMode::Interlaced:
    case VideoScanMode::PsF:
        return interlaced;

    default:
        return false;
    }
}

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
                                                const std::uint64_t event_sequence,
                                                std::uint32_t &out_slot_index) noexcept {
        const auto &descriptor = ring.descriptor();
        const auto slot_index = static_cast<std::uint32_t>(slot_id);

        if (slot_index >= descriptor.slot_count) {
            ++malformed_ready_events;
            return false;
        }

        auto began = ring.begin_read_slot_if_matches(slot_index, event_sequence);
        if (!began.has_value()) {
            ++release_failures;
            return false;
        }

        switch (*began) {
        case MtlWorkerSharedMemoryBeginReadResult::Acquired:
            out_slot_index = slot_index;
            return true;

        case MtlWorkerSharedMemoryBeginReadResult::Stale:
            ++stale_ready_events;
            return false;

        case MtlWorkerSharedMemoryBeginReadResult::NotReady:
            ++ignored_events;
            return false;
        }

        ++ignored_events;
        return false;
    }

    [[nodiscard]] bool validate_ready_payload_no_lock(MtlWorkerSharedMemoryRingMap &ring,
                                                      const std::uint32_t slot_index,
                                                      const std::uint64_t event_sequence,
                                                      const MtlWorkerMediaKind expected_media_kind) noexcept {
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

        if (header.has_value()) {
            if ((*header)->sequence != event_sequence) {
                stale = true;
            }

            if ((*header)->payload_size == 0) {
                malformed = true;
            }

            if (payload.has_value() && (*header)->payload_size > payload->size()) {
                malformed = true;
            }

            if ((*header)->media.media_kind != expected_media_kind) {
                malformed = true;
            }

            if ((*header)->media.plane_count == 0 || (*header)->media.plane_count > mtlWorkerSharedMemoryMaxPlanes) {
                malformed = true;
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

    void deliver_video_frame_no_lock(MtlWorkerSharedMemoryRingMap &ring, const std::uint32_t slot_index) noexcept {
        if (!sink || !video_config.has_value()) {
            return;
        }

        auto header = ring.slot_header(slot_index);
        if (!header.has_value()) {
            ++malformed_ready_events;
            return;
        }

        const auto &slot = **header;
        const auto &media = slot.media;

        if (media.media_kind != MtlWorkerMediaKind::Video ||
            media.media_format != static_cast<std::uint32_t>(video_config->output_format)) {
            ++malformed_ready_events;
            return;
        }

        if (media.width == 0 || media.height == 0 || slot.payload_size == 0) {
            ++malformed_ready_events;
            return;
        }

        if (media.width != video_config->width || media.height != video_config->height) {
            ++malformed_ready_events;
            return;
        }

        auto scan_mode = decode_video_scan_mode(media.video_scan_mode);
        if (!scan_mode.has_value()) {
            ++malformed_ready_events;
            return;
        }

        if (*scan_mode != video_config->scan_mode) {
            ++malformed_ready_events;
            return;
        }

        if (!video_scan_field_metadata_is_consistent(*scan_mode, media.video_field_flags)) {
            ++malformed_ready_events;
            return;
        }

        auto payload = ring.slot_payload(slot_index);
        if (!payload.has_value()) {
            ++malformed_ready_events;
            return;
        }

        if (slot.payload_size > static_cast<std::uint64_t>(payload->size())) {
            ++malformed_ready_events;
            return;
        }

        try {
            VideoFrame frame{media.width, media.height, video_config->output_format};

            if (media.plane_count != frame.plane_count() || media.plane_count == 0 ||
                media.plane_count > mtlWorkerSharedMemoryMaxPlanes) {
                ++malformed_ready_events;
                return;
            }

            for (std::size_t plane = 0; plane < frame.plane_count(); ++plane) {
                const std::uint64_t src_offset_u64 = media.plane_offset_bytes[plane];
                const std::uint64_t src_plane_size_u64 = media.plane_size_bytes[plane];
                const std::uint64_t src_stride_u64 = media.plane_line_size_bytes[plane];

                if (src_stride_u64 == 0 || src_plane_size_u64 == 0) {
                    ++malformed_ready_events;
                    return;
                }

                const std::size_t active_row_bytes = frame.active_row_bytes(plane);
                const std::size_t height_rows = frame.plane_height_rows(plane);

                if (active_row_bytes == 0 || height_rows == 0) {
                    ++malformed_ready_events;
                    return;
                }

                const auto active_row_bytes_u64 = static_cast<std::uint64_t>(active_row_bytes);
                const auto height_rows_u64 = static_cast<std::uint64_t>(height_rows);

                if (src_stride_u64 < active_row_bytes_u64) {
                    ++malformed_ready_events;
                    return;
                }

                auto last_row_offset = mul_u64(height_rows_u64 - 1, src_stride_u64);
                if (!last_row_offset.has_value()) {
                    ++malformed_ready_events;
                    return;
                }

                auto required_plane_bytes = add_u64(*last_row_offset, active_row_bytes_u64);
                if (!required_plane_bytes.has_value()) {
                    ++malformed_ready_events;
                    return;
                }

                if (src_plane_size_u64 < *required_plane_bytes) {
                    ++malformed_ready_events;
                    return;
                }

                if (src_offset_u64 > slot.payload_size) {
                    ++malformed_ready_events;
                    return;
                }

                if (src_plane_size_u64 > slot.payload_size - src_offset_u64) {
                    ++malformed_ready_events;
                    return;
                }

                if (src_offset_u64 > static_cast<std::uint64_t>(payload->size())) {
                    ++malformed_ready_events;
                    return;
                }

                if (*required_plane_bytes > static_cast<std::uint64_t>(payload->size()) - src_offset_u64) {
                    ++malformed_ready_events;
                    return;
                }

                if (src_offset_u64 > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()) ||
                    src_stride_u64 > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()) ||
                    active_row_bytes_u64 > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
                    ++malformed_ready_events;
                    return;
                }

                const auto src_offset = static_cast<std::size_t>(src_offset_u64);
                const auto src_stride = static_cast<std::size_t>(src_stride_u64);
                const auto active_row_bytes_size = static_cast<std::size_t>(active_row_bytes_u64);

                const std::byte *src_plane = payload->data() + src_offset;

                for (std::size_t row = 0; row < height_rows; ++row) {
                    std::memcpy(frame.row_data(static_cast<std::uint32_t>(row), plane), src_plane + row * src_stride,
                                active_row_bytes_size);
                }
            }

            const bool second_field =
                video_field_flag_set(media.video_field_flags, MtlWorkerVideoFieldFlags::SecondField);

            sink->on_video_frame(std::move(frame), FrameTimingMetadata{
                                                       .rtp_timestamp = media.rtp_timestamp,
                                                       .receive_timestamp_ns = media.receive_timestamp_ns,
                                                       .video_scan_mode = *scan_mode,
                                                       .video_second_field = second_field,
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

    void deliver_audio_block_no_lock(MtlWorkerSharedMemoryRingMap &ring, const std::uint32_t slot_index) noexcept {
        if (!sink || !audio_config.has_value()) {
            return;
        }

        auto header = ring.slot_header(slot_index);
        if (!header.has_value()) {
            ++malformed_ready_events;
            return;
        }

        const auto &slot = **header;
        const auto &media = slot.media;

        if (media.media_kind != MtlWorkerMediaKind::Audio ||
            media.media_format != static_cast<std::uint32_t>(audio_config->pcm_format)) {
            ++malformed_ready_events;
            return;
        }

        if (media.sample_rate_hz == 0 || media.channels == 0 || media.samples_per_channel == 0 ||
            slot.payload_size == 0) {
            ++malformed_ready_events;
            return;
        }

        if (media.channels > static_cast<std::uint32_t>(std::numeric_limits<std::uint16_t>::max())) {
            ++malformed_ready_events;
            return;
        }

        if (media.sample_rate_hz != audio_config->media.sampling_rate_hz ||
            media.channels != audio_config->media.channel_count) {
            ++malformed_ready_events;
            return;
        }

        auto payload = ring.slot_payload(slot_index);
        if (!payload.has_value()) {
            ++malformed_ready_events;
            return;
        }

        if (slot.payload_size > payload->size()) {
            ++malformed_ready_events;
            return;
        }

        try {
            AudioBuffer buffer{
                media.sample_rate_hz,
                static_cast<std::uint16_t>(media.channels),
                media.samples_per_channel,
            };

            const auto active_payload = std::span<const std::byte>{payload->data(), slot.payload_size};

            if (!convert_interleaved_pcm_to_s32_no_lock(active_payload, audio_config->pcm_format, buffer)) {
                return;
            }

            sink->on_audio_frame(std::move(buffer), FrameTimingMetadata{
                                                        .rtp_timestamp = media.rtp_timestamp,
                                                        .receive_timestamp_ns = media.receive_timestamp_ns,
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
        if (!begin_ready_slot_no_lock(ring, event.slot_id, event.sequence, slot_index)) {
            return;
        }

        const bool valid_payload =
            validate_ready_payload_no_lock(ring, slot_index, event.sequence, MtlWorkerMediaKind::Video);
        if (valid_payload) {
            deliver_video_frame_no_lock(ring, slot_index);
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
        if (!begin_ready_slot_no_lock(ring, event.slot_id, event.sequence, slot_index)) {
            return;
        }

        const bool valid_payload =
            validate_ready_payload_no_lock(ring, slot_index, event.sequence, MtlWorkerMediaKind::Audio);
        if (valid_payload) {
            deliver_audio_block_no_lock(ring, slot_index);
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
    bool manager_graph_registered = false;
    bool running = false;
    std::uint32_t active_start_count = 0;

    std::optional<MtlWorkerErrorDetail> last_error_detail{};

    void clear_last_error_detail_noexcept() noexcept {
        try {
            last_error_detail.reset();
        } catch (...) {
        }
    }

    void record_error_detail(std::optional<MtlWorkerErrorDetail> detail) {
        if (detail.has_value()) {
            last_error_detail = std::move(*detail);
        }
    }

    void record_local_error(const Error error, const MtlWorkerRequestId request_id, const char *operation,
                            const char *detail) {
        last_error_detail = make_local_error_detail(error, request_id, graph_id, operation, detail);
    }

    void unregister_async_event_handler_noexcept() noexcept {
        if (async_event_state) {
            async_event_state->deactivate_noexcept();
        }

        if (async_event_handler_registered && worker_lease.has_value() && worker_lease->control_channel) {
            worker_lease->control_channel->unregister_async_event_handler_noexcept(graph_id);
        }

        async_event_handler_registered = false;
    }

    void release_manager_graph_noexcept() noexcept {
        if (!manager_graph_registered) {
            return;
        }

        if (worker_lease.has_value()) {
            default_mtl_worker_manager().release_graph_noexcept(worker_lease->worker_id, graph_id);
        }

        manager_graph_registered = false;
    }

    void invalidate_worker_noexcept() noexcept {
        if (worker_lease.has_value()) {
            default_mtl_worker_manager().invalidate_worker_noexcept(worker_lease->worker_id);
        }

        manager_graph_registered = false;
    }

    void clear_worker_lease_noexcept() noexcept {
        manager_graph_registered = false;
        worker_lease.reset();
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
    impl_->clear_last_error_detail_noexcept();
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

    auto lease = default_mtl_worker_manager().acquire_or_spawn_compatible_worker_for_graph(*runtime, impl_->graph_id);
    if (!lease.has_value()) {
        return std::unexpected(lease.error());
    }

    impl_->worker_lease = *lease;
    impl_->manager_graph_registered = true;

    if (!impl_->worker_lease->control_channel) {
        impl_->release_manager_graph_noexcept();
        impl_->clear_worker_lease_noexcept();
        return std::unexpected(Error::InvalidBackendState);
    }

    std::optional<MtlWorkerErrorDetail> health_detail{};
    auto worker_health = check_worker_health(*impl_->worker_lease->control_channel, &health_detail);
    if (!worker_health.has_value()) {
        impl_->record_error_detail(std::move(health_detail));
        impl_->invalidate_worker_noexcept();
        impl_->clear_worker_lease_noexcept();
        return std::unexpected(worker_health.error());
    }

    auto prepared_rings = prepare_media_rings(impl_->graph_id, impl_->video, impl_->audio);
    if (!prepared_rings.has_value()) {
        impl_->release_manager_graph_noexcept();
        impl_->clear_worker_lease_noexcept();
        return std::unexpected(prepared_rings.error());
    }

    auto request = make_start_sessions_request();
    if (!request.has_value()) {
        impl_->release_manager_graph_noexcept();
        impl_->clear_worker_lease_noexcept();
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
        impl_->invalidate_worker_noexcept();
        impl_->clear_worker_lease_noexcept();
        return std::unexpected(registered_async_handler.error());
    }

    impl_->async_event_handler_registered = true;

    const MtlWorkerRequestId request_id = request->request_id;

    auto envelope = impl_->worker_lease->control_channel->transact_with_fds(MtlWorkerControlRequest{*request},
                                                                            prepared_rings->file_descriptors);
    if (!envelope.has_value()) {
        impl_->record_local_error(envelope.error(), request_id, "StartSessions", "IPC transaction failed");

        impl_->unregister_async_event_handler_noexcept();
        impl_->async_event_state.reset();
        impl_->invalidate_worker_noexcept();
        impl_->clear_worker_lease_noexcept();
        return std::unexpected(envelope.error());
    }

    std::optional<MtlWorkerErrorDetail> start_detail{};
    auto started = interpret_start_sessions_event(envelope->event, request_id, impl_->graph_id, &start_detail);
    if (!started.has_value()) {
        impl_->record_error_detail(std::move(start_detail));

        impl_->unregister_async_event_handler_noexcept();
        impl_->async_event_state.reset();

        if (is_backend_runtime_error(started.error())) {
            impl_->invalidate_worker_noexcept();
        } else {
            impl_->release_manager_graph_noexcept();
        }

        impl_->clear_worker_lease_noexcept();
        return std::unexpected(started.error());
    }

    impl_->running = true;
    impl_->active_start_count = 1;
    impl_->clear_last_error_detail_noexcept();
    return true;
}

std::expected<bool, Error> MtlWorkerGraphClient::stop() {
    if (!impl_->running) {
        impl_->active_start_count = 0;
        impl_->unregister_async_event_handler_noexcept();
        impl_->async_event_state.reset();
        impl_->release_manager_graph_noexcept();
        impl_->clear_worker_lease_noexcept();
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
        impl_->release_manager_graph_noexcept();
        impl_->clear_worker_lease_noexcept();
        return std::unexpected(Error::InvalidBackendState);
    }

    auto request = make_stop_sessions_request();
    const MtlWorkerRequestId request_id = request.request_id;

    auto event = impl_->worker_lease->control_channel->transact(MtlWorkerControlRequest{request});
    if (!event.has_value()) {
        impl_->record_local_error(event.error(), request_id, "StopSessions", "IPC transaction failed");

        impl_->running = false;
        impl_->active_start_count = 0;
        impl_->unregister_async_event_handler_noexcept();
        impl_->async_event_state.reset();
        impl_->invalidate_worker_noexcept();
        impl_->clear_worker_lease_noexcept();
        return std::unexpected(event.error());
    }

    std::optional<MtlWorkerErrorDetail> stop_detail{};
    auto stopped = interpret_stop_sessions_event(*event, request_id, impl_->graph_id, &stop_detail);
    if (!stopped.has_value()) {
        impl_->record_error_detail(std::move(stop_detail));

        impl_->running = false;
        impl_->active_start_count = 0;
        impl_->unregister_async_event_handler_noexcept();
        impl_->async_event_state.reset();
        impl_->invalidate_worker_noexcept();
        impl_->clear_worker_lease_noexcept();
        return std::unexpected(stopped.error());
    }

    impl_->running = false;
    impl_->active_start_count = 0;
    impl_->unregister_async_event_handler_noexcept();
    impl_->async_event_state.reset();
    impl_->release_manager_graph_noexcept();
    impl_->clear_worker_lease_noexcept();

    impl_->clear_last_error_detail_noexcept();

    return true;
}

std::expected<MtlWorkerStatsEvent, Error> MtlWorkerGraphClient::stats() {
    if (!impl_->running || !impl_->worker_lease.has_value() || !impl_->worker_lease->control_channel) {
        return std::unexpected(Error::InvalidBackendState);
    }

    std::optional<MtlWorkerErrorDetail> health_detail{};
    auto worker_health = check_worker_health(*impl_->worker_lease->control_channel, &health_detail);
    if (!worker_health.has_value()) {
        impl_->record_error_detail(std::move(health_detail));

        impl_->running = false;
        impl_->active_start_count = 0;
        impl_->unregister_async_event_handler_noexcept();
        impl_->async_event_state.reset();
        impl_->invalidate_worker_noexcept();
        impl_->clear_worker_lease_noexcept();
        return std::unexpected(worker_health.error());
    }

    auto request = make_stats_request();
    if (!request.has_value()) {
        return std::unexpected(request.error());
    }

    const MtlWorkerRequestId request_id = request->request_id;

    auto event = impl_->worker_lease->control_channel->transact(MtlWorkerControlRequest{*request});
    if (!event.has_value()) {
        impl_->record_local_error(event.error(), request_id, "Stats", "IPC transaction failed");

        impl_->running = false;
        impl_->active_start_count = 0;
        impl_->unregister_async_event_handler_noexcept();
        impl_->async_event_state.reset();
        impl_->invalidate_worker_noexcept();
        impl_->clear_worker_lease_noexcept();
        return std::unexpected(event.error());
    }

    std::optional<MtlWorkerErrorDetail> stats_detail{};
    auto stats = interpret_stats_event(*event, request_id, impl_->graph_id, &stats_detail);
    if (!stats.has_value()) {
        impl_->record_error_detail(std::move(stats_detail));

        impl_->running = false;
        impl_->active_start_count = 0;
        impl_->unregister_async_event_handler_noexcept();
        impl_->async_event_state.reset();
        impl_->invalidate_worker_noexcept();
        impl_->clear_worker_lease_noexcept();
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

std::optional<MtlWorkerErrorDetail> MtlWorkerGraphClient::last_error_detail() const { return impl_->last_error_detail; }

std::string MtlWorkerGraphClient::last_error_message() const {
    if (!impl_->last_error_detail.has_value()) {
        return {};
    }

    return impl_->last_error_detail->message;
}

} // namespace st2110
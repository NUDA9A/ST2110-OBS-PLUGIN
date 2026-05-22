#ifndef ST2110_OBS_PLUGIN_SYNCHRONIZED_FRAME_SINK_HPP
#define ST2110_OBS_PLUGIN_SYNCHRONIZED_FRAME_SINK_HPP

#include <st2110/contracts/backend/backend.hpp>
#include <st2110/contracts/rtp_timestamp_mapper_config.hpp>
#include <st2110/foundation/error.hpp>
#include <st2110/foundation/rtp_timestamp_mapper.hpp>
#include <st2110/foundation/timestamp.hpp>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <exception>
#include <limits>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>

namespace st2110 {

struct SynchronizedFrameSinkConfig {
    bool enable_video = false;
    bool enable_audio = false;

    RtpTimestampMapperConfig video_timestamp_mapper{};
    RtpTimestampMapperConfig audio_timestamp_mapper{};

    TimestampNs playout_delay_ns = 0;

    std::size_t max_queued_video_frames = 8;
    std::size_t max_queued_audio_blocks = 256;
};

struct SynchronizedFrameSinkStats {
    std::uint64_t video_frames_accepted = 0;
    std::uint64_t audio_blocks_accepted = 0;

    std::uint64_t video_frames_delivered = 0;
    std::uint64_t audio_blocks_delivered = 0;

    std::uint64_t video_frames_dropped = 0;
    std::uint64_t audio_blocks_dropped = 0;

    std::uint64_t timestamp_mapping_errors = 0;
    std::uint64_t output_errors = 0;
};

class SynchronizedFrameSink : public IFrameSink {
  public:
    explicit SynchronizedFrameSink(const SynchronizedFrameSinkConfig &cfg)
        : cfg_(cfg), video_mapper_(cfg.video_timestamp_mapper), audio_mapper_(cfg.audio_timestamp_mapper) {}

    SynchronizedFrameSink(const SynchronizedFrameSink &) = delete;
    SynchronizedFrameSink &operator=(const SynchronizedFrameSink &) = delete;

    SynchronizedFrameSink(SynchronizedFrameSink &&) = delete;
    SynchronizedFrameSink &operator=(SynchronizedFrameSink &&) = delete;

    ~SynchronizedFrameSink() override { stop(); }

    void start() {
        {
            std::lock_guard lock(mutex_);
            if (running_) {
                return;
            }

            stop_requested_ = false;
            output_error_ = nullptr;
            playout_anchor_.reset();

            video_queue_.clear();
            audio_queue_.clear();

            running_ = true;
        }

        playout_thread_ = std::jthread([this](std::stop_token stop_token) { run_playout_loop(stop_token); });
    }

    void stop() noexcept {
        {
            std::lock_guard lock(mutex_);
            stop_requested_ = true;
            running_ = false;
        }

        cv_.notify_all();
        playout_thread_ = {};

        {
            std::lock_guard lock(mutex_);
            video_queue_.clear();
            audio_queue_.clear();
            playout_anchor_.reset();
        }
    }

    [[nodiscard]] SynchronizedFrameSinkStats stats() const {
        std::lock_guard lock(mutex_);
        return stats_;
    }

    [[nodiscard]] std::exception_ptr output_error() const {
        std::lock_guard lock(mutex_);
        return output_error_;
    }

    void on_video_frame(VideoFrame frame, FrameTimingMetadata timing_metadata) override {
        if (!cfg_.enable_video) {
            return;
        }

        {
            std::lock_guard lock(mutex_);

            if (!running_ || output_error_) {
                ++stats_.video_frames_dropped;
                return;
            }

            auto mapped_timestamp = video_mapper_.map(timing_metadata.rtp_timestamp);
            if (!mapped_timestamp.has_value()) {
                ++stats_.timestamp_mapping_errors;
                ++stats_.video_frames_dropped;
                return;
            }

            ensure_playout_anchor_locked(*mapped_timestamp);

            QueuedVideoFrame queued{
                .frame = std::move(frame),
                .timing = timing_metadata,
                .media_timestamp_ns = *mapped_timestamp,
            };

            insert_sorted_by_media_timestamp(video_queue_, std::move(queued));
            trim_video_queue_locked();

            ++stats_.video_frames_accepted;
        }

        cv_.notify_all();
    }

    void on_audio_frame(AudioBuffer frame, FrameTimingMetadata timing_metadata) override {
        if (!cfg_.enable_audio) {
            return;
        }

        {
            std::lock_guard lock(mutex_);

            if (!running_ || output_error_) {
                ++stats_.audio_blocks_dropped;
                return;
            }

            auto mapped_timestamp = audio_mapper_.map(timing_metadata.rtp_timestamp);
            if (!mapped_timestamp.has_value()) {
                ++stats_.timestamp_mapping_errors;
                ++stats_.audio_blocks_dropped;
                return;
            }

            ensure_playout_anchor_locked(*mapped_timestamp);

            QueuedAudioBlock queued{
                .block = std::move(frame),
                .timing = timing_metadata,
                .media_timestamp_ns = *mapped_timestamp,
            };

            insert_sorted_by_media_timestamp(audio_queue_, std::move(queued));
            trim_audio_queue_locked();

            ++stats_.audio_blocks_accepted;
        }

        cv_.notify_all();
    }

  protected:
    virtual void deliver_video_frame_to_obs(VideoFrame &&frame, FrameTimingMetadata timing,
                                            TimestampNs media_timestamp_ns) {
        (void)frame;
        (void)timing;
        (void)media_timestamp_ns;
        throw std::logic_error("Implement me!");
    }

    virtual void deliver_audio_block_to_obs(AudioBuffer &&block, FrameTimingMetadata timing,
                                            TimestampNs media_timestamp_ns) {
        (void)block;
        (void)timing;
        (void)media_timestamp_ns;
        throw std::logic_error("Implement me!");
    }

  private:
    using Clock = std::chrono::steady_clock;

    struct QueuedVideoFrame {
        VideoFrame frame;
        FrameTimingMetadata timing{};
        TimestampNs media_timestamp_ns = 0;
    };

    struct QueuedAudioBlock {
        AudioBuffer block;
        FrameTimingMetadata timing{};
        TimestampNs media_timestamp_ns = 0;
    };

    struct PlayoutAnchor {
        TimestampNs media_timestamp_ns = 0;
        Clock::time_point local_playout_time{};
    };

    enum class PendingKind {
        Video,
        Audio,
    };

    struct PendingItem {
        PendingKind kind = PendingKind::Video;
        TimestampNs media_timestamp_ns = 0;
    };

    [[nodiscard]] static std::chrono::nanoseconds duration_from_timestamp_ns(const TimestampNs timestamp_ns) noexcept {
        using Rep = std::chrono::nanoseconds::rep;

        constexpr auto max_duration_ns = static_cast<TimestampNs>(std::numeric_limits<Rep>::max());
        const TimestampNs clamped = timestamp_ns > max_duration_ns ? max_duration_ns : timestamp_ns;

        return std::chrono::nanoseconds(static_cast<Rep>(clamped));
    }

    template <typename Queue, typename Item> static void insert_sorted_by_media_timestamp(Queue &queue, Item &&item) {
        auto it = queue.begin();

        while (it != queue.end() && it->media_timestamp_ns <= item.media_timestamp_ns) {
            ++it;
        }

        queue.insert(it, std::forward<Item>(item));
    }

    void trim_video_queue_locked() {
        while (video_queue_.size() > cfg_.max_queued_video_frames) {
            video_queue_.pop_front();
            ++stats_.video_frames_dropped;
        }
    }

    void trim_audio_queue_locked() {
        while (audio_queue_.size() > cfg_.max_queued_audio_blocks) {
            audio_queue_.pop_front();
            ++stats_.audio_blocks_dropped;
        }
    }

    void ensure_playout_anchor_locked(const TimestampNs media_timestamp_ns) {
        if (playout_anchor_.has_value()) {
            return;
        }

        playout_anchor_ = PlayoutAnchor{
            .media_timestamp_ns = media_timestamp_ns,
            .local_playout_time = Clock::now() + duration_from_timestamp_ns(cfg_.playout_delay_ns),
        };
    }

    [[nodiscard]] std::optional<PendingItem> next_pending_item_locked() const {
        if (video_queue_.empty() && audio_queue_.empty()) {
            return std::nullopt;
        }

        if (video_queue_.empty()) {
            return PendingItem{.kind = PendingKind::Audio,
                               .media_timestamp_ns = audio_queue_.front().media_timestamp_ns};
        }

        if (audio_queue_.empty()) {
            return PendingItem{.kind = PendingKind::Video,
                               .media_timestamp_ns = video_queue_.front().media_timestamp_ns};
        }

        if (video_queue_.front().media_timestamp_ns <= audio_queue_.front().media_timestamp_ns) {
            return PendingItem{.kind = PendingKind::Video,
                               .media_timestamp_ns = video_queue_.front().media_timestamp_ns};
        }

        return PendingItem{.kind = PendingKind::Audio, .media_timestamp_ns = audio_queue_.front().media_timestamp_ns};
    }

    [[nodiscard]] Clock::time_point playout_time_for_locked(const TimestampNs media_timestamp_ns) const {
        if (!playout_anchor_.has_value()) {
            return Clock::now();
        }

        if (media_timestamp_ns <= playout_anchor_->media_timestamp_ns) {
            return playout_anchor_->local_playout_time;
        }

        const TimestampNs relative_ns = media_timestamp_ns - playout_anchor_->media_timestamp_ns;
        const auto relative_duration = duration_from_timestamp_ns(relative_ns);

        const auto remaining = Clock::time_point::max() - playout_anchor_->local_playout_time;
        if (relative_duration > remaining) {
            return Clock::time_point::max();
        }

        return playout_anchor_->local_playout_time + relative_duration;
    }

    void run_playout_loop(std::stop_token stop_token) noexcept {
        while (!stop_token.stop_requested()) {
            std::optional<QueuedVideoFrame> video_to_deliver;
            std::optional<QueuedAudioBlock> audio_to_deliver;

            {
                std::unique_lock lock(mutex_);

                while (!stop_requested_ && !stop_token.stop_requested()) {
                    auto pending = next_pending_item_locked();

                    if (!pending.has_value()) {
                        cv_.wait(lock, stop_token,
                                 [this] { return stop_requested_ || !video_queue_.empty() || !audio_queue_.empty(); });
                        continue;
                    }

                    const auto deadline = playout_time_for_locked(pending->media_timestamp_ns);
                    const auto now = Clock::now();

                    if (deadline > now) {
                        cv_.wait_until(lock, stop_token, deadline, [this] { return stop_requested_; });
                        continue;
                    }

                    if (pending->kind == PendingKind::Video) {
                        video_to_deliver.emplace(std::move(video_queue_.front()));
                        video_queue_.pop_front();
                    } else {
                        audio_to_deliver.emplace(std::move(audio_queue_.front()));
                        audio_queue_.pop_front();
                    }

                    break;
                }

                if (stop_requested_ || stop_token.stop_requested()) {
                    return;
                }
            }

            try {
                if (video_to_deliver.has_value()) {
                    deliver_video_frame_to_obs(std::move(video_to_deliver->frame), video_to_deliver->timing,
                                               video_to_deliver->media_timestamp_ns);

                    std::lock_guard lock(mutex_);
                    ++stats_.video_frames_delivered;
                } else if (audio_to_deliver.has_value()) {
                    deliver_audio_block_to_obs(std::move(audio_to_deliver->block), audio_to_deliver->timing,
                                               audio_to_deliver->media_timestamp_ns);

                    std::lock_guard lock(mutex_);
                    ++stats_.audio_blocks_delivered;
                }
            } catch (...) {
                store_output_exception_and_stop();
                return;
            }
        }
    }

    void store_output_exception_and_stop() noexcept {
        {
            std::lock_guard lock(mutex_);
            output_error_ = std::current_exception();
            ++stats_.output_errors;
            stop_requested_ = true;
            running_ = false;
        }

        cv_.notify_all();
    }

    SynchronizedFrameSinkConfig cfg_{};

    RtpTimestampMapper video_mapper_;
    RtpTimestampMapper audio_mapper_;

    mutable std::mutex mutex_{};
    std::condition_variable_any cv_{};

    bool running_ = false;
    bool stop_requested_ = false;

    std::deque<QueuedVideoFrame> video_queue_{};
    std::deque<QueuedAudioBlock> audio_queue_{};

    std::optional<PlayoutAnchor> playout_anchor_{};

    SynchronizedFrameSinkStats stats_{};
    std::exception_ptr output_error_{};

    std::jthread playout_thread_{};
};

} // namespace st2110

#endif // ST2110_OBS_PLUGIN_SYNCHRONIZED_FRAME_SINK_HPP
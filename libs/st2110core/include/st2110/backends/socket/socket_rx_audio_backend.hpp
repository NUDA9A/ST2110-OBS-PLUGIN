#ifndef ST2110_OBS_SOCKET_RX_AUDIO_BACKEND_HPP
#define ST2110_OBS_SOCKET_RX_AUDIO_BACKEND_HPP

#include "platform/socket_runtime.hpp"
#include "socket_rx_single_media_backend_base.hpp"

#include <st2110/contracts/backend/backend.hpp>
#include <st2110/delivery/audio/socket_audio_start_config.hpp>
#include <st2110/foundation/bytes.hpp>
#include <st2110/receive/audio/audio_frame_assembler.hpp>
#include <st2110/receive/audio/audio_packet.hpp>
#include <st2110/receive/shared/fixed_reorder_buffer.hpp>
#include <st2110/receive/shared/reorder_buffer.hpp>

#include <cstdint>
#include <expected>
#include <memory>
#include <utility>
#include <vector>

namespace st2110 {
class SocketRxAudioBackend final : public SocketRxSingleMediaBackendBase {
  public:
    explicit SocketRxAudioBackend(const SocketAudioStartConfig &cfg)
        : SocketRxSingleMediaBackendBase(make_default_port_factory(), cfg.legs[0].max_udp_datagram_bytes,
                                         cfg.reorder_buffer_config, collect_open_configs(cfg),
                                         cfg.stream.expected_payload_type),
          media_(cfg.stream.media), samples_per_packet_(cfg.stream.samples_per_packet) {}

    RxBackendLifecycleResult start(IFrameSink *sink) override {
        reorder_buffer_ = make_reorder_buffer(reorder_buffer_config_);
        return start_common_runtime(sink);
    }

    ~SocketRxAudioBackend() override { (void)SocketRxSingleMediaBackendBase::stop(); }

  protected:
    std::expected<std::unique_ptr<PacketView>, Error> parse_packet(const std::size_t leg_index,
                                                                   const ByteSpan udp_payload) override {
        (void)leg_index;

        auto packet = parse_audio_rtp_packet_view(udp_payload, media_, samples_per_packet_);
        if (!packet) {
            return std::unexpected(packet.error());
        }

        return std::make_unique<AudioPacketView>(*packet);
    }

    [[nodiscard]] std::unique_ptr<IReorderBuffer> make_reorder_buffer(const ReorderBufferConfig &cfg) override {
        return std::make_unique<FixedWindowReorderBuffer<false>>(cfg.window_size_packets);
    }

    void deliver_media(std::unique_ptr<StoredPacket> packet) override {
        auto base = packet->view();
        auto audio_packet = std::unique_ptr<AudioPacketView>(static_cast<AudioPacketView *>(base.release()));

        auto block = audio_frame_assembler_.push(*audio_packet);
        deliver_assembled_audio_block(std::move(block));
    }

  private:
    static std::vector<SocketRxOpenConfig> collect_open_configs(const SocketAudioStartConfig &cfg) {
        std::vector<SocketRxOpenConfig> open_configs;
        open_configs.reserve(cfg.legs.size());
        for (const auto &leg : cfg.legs) {
            open_configs.emplace_back(leg.open_config);
        }

        return open_configs;
    }

    void deliver_assembled_audio_block(AssembledAudioBlock &&block) const noexcept {
        sink_->on_audio_frame(std::move(block.buffer),
                              FrameTimingMetadata{.rtp_timestamp = block.rtp_timestamp,
                                                  .receive_timestamp_ns = block.receive_timestamp_ns});
    }

    AudioFrameAssembler audio_frame_assembler_{};
    AudioMediaDescription media_;
    std::uint32_t samples_per_packet_;
};

} // namespace st2110

#endif // ST2110_OBS_SOCKET_RX_AUDIO_BACKEND_HPP
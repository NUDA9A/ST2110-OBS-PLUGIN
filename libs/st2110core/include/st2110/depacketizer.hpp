#ifndef ST2110_OBS_PLUGIN_DEPACKETIZER_HPP
#define ST2110_OBS_PLUGIN_DEPACKETIZER_HPP

#include "packet_view.hpp"
#include "frame_assembler.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace st2110 {

    struct DepacketizerConfig {
        uint32_t width = 0;
        uint32_t height = 0;
        PixelFormat format = PixelFormat::UYVY;
        PartialFramePolicy partial_frame_policy = PartialFramePolicy::EmitWithFlag;
    };

    class Depacketizer {
    public:
        explicit Depacketizer(const DepacketizerConfig& cfg) : cfg_(cfg), assembler_(cfg_.width, cfg_.height, cfg_.format, cfg_.partial_frame_policy) {}

        [[nodiscard]] std::vector<AssembledVideoFrame> push(const PacketView& packet) {
            std::vector<AssembledVideoFrame> res;

            if (!has_current_timestamp_) {
                assembler_.begin(packet.rtp.timestamp);
                has_current_timestamp_ = true;
                current_rtp_timestamp_ = packet.rtp.timestamp;
                if (packet.rtp.marker) {
                    auto end_res = assembler_.end(true);
                    if (end_res.frame.has_value()) {
                        res.push_back(*end_res.frame);
                    }
                    has_current_timestamp_ = false;
                    current_rtp_timestamp_ = 0;
                }
                return res;
            }

            if (current_rtp_timestamp_ == packet.rtp.timestamp) {
                if (packet.rtp.marker) {
                    auto end_res = assembler_.end(true);
                    if (end_res.frame.has_value()) {
                        res.push_back(*end_res.frame);
                    }
                    has_current_timestamp_ = false;
                    current_rtp_timestamp_ = 0;
                }
                return res;
            }

            (void)assembler_.end(false);
            assembler_.begin(packet.rtp.timestamp);
            has_current_timestamp_ = true;
            current_rtp_timestamp_ = packet.rtp.timestamp;
            if (packet.rtp.marker) {
                auto end_res = assembler_.end(true);
                if (end_res.frame.has_value()) {
                    res.push_back(*end_res.frame);
                }
                has_current_timestamp_ = false;
                current_rtp_timestamp_ = 0;
            }
            return res;
        }

        void reset() {
            assembler_ = FrameAssembler(cfg_.width, cfg_.height, cfg_.format, cfg_.partial_frame_policy);
            has_current_timestamp_ = false;
            current_rtp_timestamp_ = 0;
        }

        [[nodiscard]] bool has_frame_in_progress() const {
            return assembler_.in_progress();
        }

        [[nodiscard]] std::optional<uint32_t> current_rtp_timestamp() const {
            if (has_current_timestamp_) {
                return current_rtp_timestamp_;
            }
            return std::nullopt;
        }

    private:
        DepacketizerConfig cfg_;
        FrameAssembler assembler_;
        bool has_current_timestamp_ = false;
        uint32_t current_rtp_timestamp_ = 0;
    };

}

#endif //ST2110_OBS_PLUGIN_DEPACKETIZER_HPP
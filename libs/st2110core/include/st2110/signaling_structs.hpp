#ifndef ST2110_OBS_PLUGIN_SIGNALING_STRUCTS_HPP
#define ST2110_OBS_PLUGIN_SIGNALING_STRUCTS_HPP

#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "video_scan_mode.hpp"
#include "packet_parse.hpp"
#include "rx_config.hpp"
#include "video_receive_pipeline.hpp"
#include "video_packing_mode.hpp"

namespace st2110 {
    enum class MediaClockMode {
        Direct, Sender
    };

    enum class TimestampMode {
        Samp, New, Pres
    };

    enum class ReferenceClockKind {
        LocalMac, Ptp, Other
    };

    struct PtpReferenceClock {
        // IEEE1588 clock identity from ts-refclk:ptp=...
        // representation format can stay implementation-chosen for now
        std::array<uint8_t, 8> clock_identity{};
        uint16_t domain_number = 0;
        bool traceable = false;
    };

    struct LocalMacReferenceClock {
        std::array<uint8_t, 6> mac{};
    };

    struct ReferenceClock {
        ReferenceClockKind kind = ReferenceClockKind::Ptp;

        std::optional <PtpReferenceClock> ptp{};
        std::optional <LocalMacReferenceClock> local_mac{};

        // preserve future extensibility / unknown RFC forms
        std::optional <std::string> raw_token{};
    };

    enum class VideoSenderType {
        Narrow, NarrowLinear, Wide
    };

    struct VideoSampling {
        enum class Known {
            YCbCr422,
            YCbCr444,
            YCbCr420,
            RGB,
            XYZ,
            Key,
            Other
        };

        Known known = Known::YCbCr422;
        std::optional <std::string> raw_token{};
    };

    struct VideoBitDepth {
        uint8_t bits = 8;
        bool floating_point = false;
    };

    struct VideoColorimetry {
        enum class Known {
            Bt601,
            Bt709,
            Bt2020,
            Bt2100,
            St2065_1,
            Other
        };

        Known known = Known::Bt709;
        std::optional <std::string> raw_token{};
    };

    struct VideoTransferCharacteristicSystem {
        enum class Known {
            SDR,
            PQ,
            HLG,
            Linear,
            Other
        };

        Known known = Known::SDR;
        std::optional <std::string> raw_token{};
    };

    struct VideoSignalStandard {
        enum class Known {
            St2110_20_2017,
            St2110_20_2022,
            Other
        };

        Known known = Known::St2110_20_2022;
        std::optional <std::string> raw_token{};
    };

    struct VideoRange {
        enum class Known {
            Narrow,
            Full,
            Other
        };

        Known known = Known::Narrow;
        std::optional <std::string> raw_token{};
    };

    struct VideoMediaDescription {
        VideoSampling sampling{};
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t fps_num = 0;
        uint32_t fps_den = 1;
        VideoBitDepth depth{};
        VideoColorimetry colorimetry{};
        std::optional <VideoTransferCharacteristicSystem> transfer_characteristic_system{};
        std::optional <VideoSignalStandard> signal_standard{};
        std::optional <VideoRange> range{};
    };

    struct VideoStreamSignaling {
        VideoMediaDescription media{};
        VideoScanMode scan_mode = VideoScanMode::Progressive;
        VideoPackingMode packing_mode = VideoPackingMode::Gpm;
        std::optional <std::size_t> max_udp_datagram_bytes{};

        MediaClockMode media_clock_mode = MediaClockMode::Direct;
        TimestampMode timestamp_mode = TimestampMode::New;
        ReferenceClock reference_clock{};

        uint32_t ts_delay_sender_ticks = 0;

        VideoSenderType sender_type = VideoSenderType::Narrow;
        std::optional <uint32_t> troff_us{};
        std::optional <uint32_t> cmax{};
    };

    struct VideoReceiverBootstrapConfig {
        PacketParsePolicy packet_parse_policy{};
        RxVideoConfig rx_config{};
        VideoReceivePipelineConfig receive_pipeline_config{};
    };
}

#endif //ST2110_OBS_PLUGIN_SIGNALING_STRUCTS_HPP

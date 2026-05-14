#include <Processing.NDI.Lib.h>

#include <mtl/mtl_api.h>
#include <mtl/st20_api.h>
#include <mtl/st_pipeline_api.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#if defined(__linux__)
#include <dlfcn.h>
#endif

namespace {

using Clock = std::chrono::steady_clock;

enum class VideoMode {
    P1080_60,
    P720_30,
};

enum class DuplicateMode {
    Off,
    On,
};

enum class VideoPacking {
    Gpm,
    Bpm,
};

struct VideoModeSpec {
    std::uint32_t width = 1920;
    std::uint32_t height = 1080;
    std::uint32_t fps_num = 60;
    std::uint32_t fps_den = 1;
    st_fps mtl_fps = ST_FPS_P60;
};

struct MtlPortConfig {
    std::string port_name{};
    std::array<std::uint8_t, 4> local_ip{};
};

struct VideoLegConfig {
    std::array<std::uint8_t, 4> dst_ip{};
    std::uint16_t udp_port = 5004;
};

struct AppConfig {
    std::string ndi_name = "ST2110 MTL Send Test";

    VideoMode video_mode = VideoMode::P1080_60;
    DuplicateMode duplicate_mode = DuplicateMode::Off;
    VideoPacking packing = VideoPacking::Gpm;

    MtlPortConfig primary_port{};
    std::optional<MtlPortConfig> redundant_port{};

    VideoLegConfig primary_video{};
    std::optional<VideoLegConfig> redundant_video{};

    std::uint8_t video_payload_type = 112;
    std::uint16_t frame_buffer_count = 3;

    mtl_pmd_type pmd = MTL_PMD_DPDK_USER;

    std::chrono::milliseconds duration{60'000};
    std::chrono::milliseconds metadata_repeat_interval{1000};

    bool metadata_only = false;
};

[[nodiscard]] std::string ipv4_to_string(const std::array<std::uint8_t, 4> &ip) {
    return std::to_string(ip[0]) + "." + std::to_string(ip[1]) + "." + std::to_string(ip[2]) + "." +
           std::to_string(ip[3]);
}

[[nodiscard]] std::uint8_t parse_u8(std::string_view value, std::string_view option_name) {
    std::uint32_t parsed = 0;
    const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (ec != std::errc{} || ptr != value.data() + value.size() || parsed > std::numeric_limits<std::uint8_t>::max()) {
        throw std::runtime_error("Invalid uint8 value for " + std::string(option_name));
    }

    return static_cast<std::uint8_t>(parsed);
}

[[nodiscard]] std::uint16_t parse_u16(std::string_view value, std::string_view option_name) {
    std::uint32_t parsed = 0;
    const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (ec != std::errc{} || ptr != value.data() + value.size() ||
        parsed > std::numeric_limits<std::uint16_t>::max()) {
        throw std::runtime_error("Invalid uint16 value for " + std::string(option_name));
    }

    return static_cast<std::uint16_t>(parsed);
}

[[nodiscard]] long long parse_positive_int(std::string_view value, std::string_view option_name) {
    try {
        std::size_t consumed = 0;
        const auto parsed = std::stoll(std::string(value), &consumed, 10);
        if (consumed != value.size() || parsed <= 0) {
            throw std::runtime_error("");
        }
        return parsed;
    } catch (...) {
        throw std::runtime_error("Invalid positive integer for " + std::string(option_name));
    }
}

[[nodiscard]] std::array<std::uint8_t, 4> parse_ipv4(std::string_view text, std::string_view option_name) {
    std::array<std::uint8_t, 4> result{};

    for (std::size_t i = 0; i < result.size(); ++i) {
        const std::size_t dot = text.find('.');
        const std::string_view part = dot == std::string_view::npos ? text : text.substr(0, dot);

        if (part.empty() || part.size() > 3) {
            throw std::runtime_error("Invalid IPv4 address for " + std::string(option_name));
        }

        std::uint32_t parsed = 0;
        const auto [ptr, ec] = std::from_chars(part.data(), part.data() + part.size(), parsed);
        if (ec != std::errc{} || ptr != part.data() + part.size() || parsed > 255) {
            throw std::runtime_error("Invalid IPv4 address for " + std::string(option_name));
        }

        result[i] = static_cast<std::uint8_t>(parsed);

        if (i + 1 == result.size()) {
            if (dot != std::string_view::npos) {
                throw std::runtime_error("Invalid IPv4 address for " + std::string(option_name));
            }
            break;
        }

        if (dot == std::string_view::npos) {
            throw std::runtime_error("Invalid IPv4 address for " + std::string(option_name));
        }

        text.remove_prefix(dot + 1);
    }

    return result;
}

[[nodiscard]] bool is_zero_ipv4(const std::array<std::uint8_t, 4> &ip) noexcept {
    return ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0;
}

[[nodiscard]] VideoModeSpec video_mode_spec(VideoMode mode) noexcept {
    switch (mode) {
    case VideoMode::P1080_60:
        return VideoModeSpec{
            .width = 1920,
            .height = 1080,
            .fps_num = 60,
            .fps_den = 1,
            .mtl_fps = ST_FPS_P60,
        };

    case VideoMode::P720_30:
        return VideoModeSpec{
            .width = 1280,
            .height = 720,
            .fps_num = 30,
            .fps_den = 1,
            .mtl_fps = ST_FPS_P30,
        };
    }

    return {};
}

[[nodiscard]] std::string exact_frame_rate_text(const VideoModeSpec &mode) {
    if (mode.fps_den == 1) {
        return std::to_string(mode.fps_num);
    }

    return std::to_string(mode.fps_num) + "/" + std::to_string(mode.fps_den);
}

[[nodiscard]] VideoMode parse_video_mode(std::string_view value) {
    if (value == "1080p60" || value == "1920x1080p60") {
        return VideoMode::P1080_60;
    }

    if (value == "720p30" || value == "1280x720p30") {
        return VideoMode::P720_30;
    }

    throw std::runtime_error("Unsupported --video-mode. Supported values: 1080p60, 720p30");
}

[[nodiscard]] mtl_pmd_type parse_pmd(std::string_view value) {
    if (value == "dpdk-user") {
        return MTL_PMD_DPDK_USER;
    }

    if (value == "kernel-socket") {
        return MTL_PMD_KERNEL_SOCKET;
    }

    if (value == "native-af-xdp") {
        return MTL_PMD_NATIVE_AF_XDP;
    }

    if (value == "dpdk-af-xdp") {
        return MTL_PMD_DPDK_AF_XDP;
    }

    if (value == "dpdk-af-packet") {
        return MTL_PMD_DPDK_AF_PACKET;
    }

    throw std::runtime_error(
        "Unsupported --pmd. Supported values: dpdk-user, kernel-socket, native-af-xdp, dpdk-af-xdp, dpdk-af-packet");
}

[[nodiscard]] VideoPacking parse_video_packing(std::string_view value) {
    if (value == "gpm") {
        return VideoPacking::Gpm;
    }

    if (value == "bpm") {
        return VideoPacking::Bpm;
    }

    throw std::runtime_error("Unsupported --packing. Supported values: gpm, bpm");
}

[[nodiscard]] st20_packing to_mtl_packing(VideoPacking packing) noexcept {
    switch (packing) {
    case VideoPacking::Gpm:
        return ST20_PACKING_GPM;
    case VideoPacking::Bpm:
        return ST20_PACKING_BPM;
    }

    return ST20_PACKING_GPM;
}

[[nodiscard]] std::string sdp_packing_text(VideoPacking packing) {
    switch (packing) {
    case VideoPacking::Gpm:
        return "2110GPM";
    case VideoPacking::Bpm:
        return "2110BPM";
    }

    return "2110GPM";
}

[[nodiscard]] std::string xml_escape_attribute(std::string_view value) {
    std::string out;
    out.reserve(value.size());

    for (const char ch : value) {
        switch (ch) {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        case '\'':
            out += "&apos;";
            break;
        default:
            out += ch;
            break;
        }
    }

    return out;
}

[[nodiscard]] std::string wrap_sdp_metadata_element(std::string_view media, std::string_view id,
                                                    std::string_view name, std::string_view sdp) {
    std::string xml;
    xml += "<st2110_sdp media=\"";
    xml += xml_escape_attribute(media);
    xml += "\" id=\"";
    xml += xml_escape_attribute(id);
    xml += "\" name=\"";
    xml += xml_escape_attribute(name);
    xml += "\"><![CDATA[\n";
    xml += sdp;
    if (!sdp.empty() && sdp.back() != '\n') {
        xml += '\n';
    }
    xml += "]]></st2110_sdp>\n";

    return xml;
}

[[nodiscard]] std::string build_video_media_section(const AppConfig &cfg, const VideoModeSpec &mode,
                                                    const VideoLegConfig &leg, const MtlPortConfig &port,
                                                    std::string_view mid) {
    const std::string dst_ip = ipv4_to_string(leg.dst_ip);
    const std::string source_ip = ipv4_to_string(port.local_ip);

    std::string sdp;
    sdp += "m=video " + std::to_string(leg.udp_port) + " RTP/AVP " + std::to_string(cfg.video_payload_type) + "\n";
    sdp += "c=IN IP4 " + dst_ip + "/32\n";
    sdp += "a=mid:" + std::string(mid) + "\n";
    sdp += "a=source-filter: incl IN IP4 " + dst_ip + " " + source_ip + "\n";
    sdp += "a=ts-refclk:localmac=00-00-00-00-00-00\n";
    sdp += "a=mediaclk:direct=0\n";
    sdp += "a=rtpmap:" + std::to_string(cfg.video_payload_type) + " raw/90000\n";
    sdp += "a=fmtp:" + std::to_string(cfg.video_payload_type) + " sampling=YCbCr-4:2:2";
    sdp += "; width=" + std::to_string(mode.width);
    sdp += "; height=" + std::to_string(mode.height);
    sdp += "; exactframerate=" + exact_frame_rate_text(mode);
    sdp += "; depth=8";
    sdp += "; colorimetry=BT709";
    sdp += "; PM=" + sdp_packing_text(cfg.packing);
    sdp += "; SSN=ST2110-20:2022";
    sdp += "; TCS=SDR";
    sdp += "; RANGE=NARROW";
    sdp += "; TP=2110TPN";
    sdp += "; MAXUDP=1460";
    sdp += "; TSMODE=SAMP";
    sdp += "\n";

    return sdp;
}

[[nodiscard]] std::string build_video_sdp(const AppConfig &cfg) {
    const VideoModeSpec mode = video_mode_spec(cfg.video_mode);

    std::string sdp;
    sdp += "v=0\n";
    sdp += "o=- 0 0 IN IP4 " + ipv4_to_string(cfg.primary_port.local_ip) + "\n";
    sdp += "s=" + cfg.ndi_name + " Video\n";
    sdp += "t=0 0\n";

    if (cfg.duplicate_mode == DuplicateMode::On) {
        sdp += "a=group:DUP primary redundant\n";
    }

    sdp += build_video_media_section(cfg, mode, cfg.primary_video, cfg.primary_port, "primary");

    if (cfg.duplicate_mode == DuplicateMode::On) {
        sdp += build_video_media_section(cfg, mode, *cfg.redundant_video, *cfg.redundant_port, "redundant");
    }

    return sdp;
}

[[nodiscard]] std::string build_st2110_sdp_metadata(const AppConfig &cfg) {
    std::string xml;
    xml += "<st2110_sdp_bundle>\n";
    xml += wrap_sdp_metadata_element("video", "video", cfg.ndi_name + " Video", build_video_sdp(cfg));
    xml += "</st2110_sdp_bundle>\n";

    return xml;
}

void print_help() {
    std::cout
        << "Usage:\n"
        << "  st2110_mtl_send_test --port-name PORT --local-ip IP --video-dst-ip IP [options]\n\n"
        << "Video modes:\n"
        << "  --video-mode 1080p60   1920x1080 60fps UYVY, default\n"
        << "  --video-mode 720p30    1280x720 30fps UYVY\n\n"
        << "Primary leg:\n"
        << "  --port-name VALUE              MTL port, e.g. 0000:af:01.0 or kernel:eth0\n"
        << "  --local-ip IP                  Local sender/source IP for primary MTL port\n"
        << "  --video-dst-ip IP              Primary destination/multicast IP, e.g. 239.211.0.20\n"
        << "  --video-udp-port PORT          Primary video UDP port, default 5004\n\n"
        << "Duplicate stream:\n"
        << "  --duplicate                    Enable two-leg SDP DUP group and two-port MTL TX\n"
        << "  --redundant-port-name VALUE    Redundant MTL port\n"
        << "  --redundant-local-ip IP        Redundant sender/source IP\n"
        << "  --redundant-video-dst-ip IP    Redundant destination/multicast IP\n"
        << "  --redundant-video-udp-port P   Redundant UDP port, default primary+2\n\n"
        << "Other options:\n"
        << "  --name NAME                    NDI source name\n"
        << "  --video-payload-type PT        RTP payload type, default 112\n"
        << "  --packing gpm|bpm              ST 2110-20 packing, default gpm\n"
        << "  --pmd dpdk-user|kernel-socket|native-af-xdp|dpdk-af-xdp|dpdk-af-packet\n"
        << "  --frame-buffer-count N         MTL ST20P frame buffer count, default 3\n"
        << "  --duration-ms N                Run duration, default 60000\n"
        << "  --repeat-ms N                  NDI metadata repeat interval, default 1000\n"
        << "  --metadata-only                Publish NDI SDP metadata without starting MTL TX\n";
}

[[nodiscard]] AppConfig parse_args(int argc, char **argv) {
    AppConfig cfg{};

    cfg.primary_video.dst_ip = parse_ipv4("239.211.0.20", "default-primary-video-dst-ip");

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];

        const auto require_value = [&](std::string_view option) -> std::string_view {
            if (i + 1 >= argc) {
                throw std::runtime_error("Missing value for " + std::string(option));
            }
            return argv[++i];
        };

        if (arg == "--help" || arg == "-h") {
            print_help();
            std::exit(0);
        } else if (arg == "--name") {
            cfg.ndi_name = std::string(require_value(arg));
        } else if (arg == "--video-mode") {
            cfg.video_mode = parse_video_mode(require_value(arg));
        } else if (arg == "--port-name") {
            cfg.primary_port.port_name = std::string(require_value(arg));
        } else if (arg == "--local-ip") {
            cfg.primary_port.local_ip = parse_ipv4(require_value(arg), arg);
        } else if (arg == "--video-dst-ip") {
            cfg.primary_video.dst_ip = parse_ipv4(require_value(arg), arg);
        } else if (arg == "--video-udp-port") {
            cfg.primary_video.udp_port = parse_u16(require_value(arg), arg);
        } else if (arg == "--video-payload-type") {
            const auto parsed = parse_u8(require_value(arg), arg);
            if (parsed < 96 || parsed > 127) {
                throw std::runtime_error("--video-payload-type must be in dynamic RTP payload range 96..127");
            }
            cfg.video_payload_type = parsed;
        } else if (arg == "--packing") {
            cfg.packing = parse_video_packing(require_value(arg));
        } else if (arg == "--pmd") {
            cfg.pmd = parse_pmd(require_value(arg));
        } else if (arg == "--frame-buffer-count") {
            const auto parsed = parse_u16(require_value(arg), arg);
            if (parsed == 0 || parsed > ST20_FB_MAX_COUNT) {
                throw std::runtime_error("--frame-buffer-count must be in range 1.." +
                                         std::to_string(ST20_FB_MAX_COUNT));
            }
            cfg.frame_buffer_count = parsed;
        } else if (arg == "--duration-ms") {
            cfg.duration = std::chrono::milliseconds(parse_positive_int(require_value(arg), arg));
        } else if (arg == "--repeat-ms") {
            cfg.metadata_repeat_interval = std::chrono::milliseconds(parse_positive_int(require_value(arg), arg));
        } else if (arg == "--metadata-only") {
            cfg.metadata_only = true;
        } else if (arg == "--duplicate") {
            cfg.duplicate_mode = DuplicateMode::On;
            if (!cfg.redundant_video.has_value()) {
                cfg.redundant_video = VideoLegConfig{
                    .dst_ip = parse_ipv4("239.211.0.21", "default-redundant-video-dst-ip"),
                    .udp_port = static_cast<std::uint16_t>(cfg.primary_video.udp_port + 2),
                };
            }
            if (!cfg.redundant_port.has_value()) {
                cfg.redundant_port = MtlPortConfig{};
            }
        } else if (arg == "--redundant-port-name") {
            if (!cfg.redundant_port.has_value()) {
                cfg.redundant_port = MtlPortConfig{};
            }
            cfg.redundant_port->port_name = std::string(require_value(arg));
        } else if (arg == "--redundant-local-ip") {
            if (!cfg.redundant_port.has_value()) {
                cfg.redundant_port = MtlPortConfig{};
            }
            cfg.redundant_port->local_ip = parse_ipv4(require_value(arg), arg);
        } else if (arg == "--redundant-video-dst-ip") {
            if (!cfg.redundant_video.has_value()) {
                cfg.redundant_video = VideoLegConfig{};
            }
            cfg.redundant_video->dst_ip = parse_ipv4(require_value(arg), arg);
        } else if (arg == "--redundant-video-udp-port") {
            if (!cfg.redundant_video.has_value()) {
                cfg.redundant_video = VideoLegConfig{};
            }
            cfg.redundant_video->udp_port = parse_u16(require_value(arg), arg);
        } else {
            throw std::runtime_error("Unknown argument: " + std::string(arg));
        }
    }

    if (cfg.ndi_name.empty()) {
        throw std::runtime_error("--name must not be empty");
    }

    if (!cfg.metadata_only) {
        if (cfg.primary_port.port_name.empty()) {
            throw std::runtime_error("--port-name is required unless --metadata-only is set");
        }

        if (is_zero_ipv4(cfg.primary_port.local_ip)) {
            throw std::runtime_error("--local-ip is required unless --metadata-only is set");
        }
    }

    if (cfg.duplicate_mode == DuplicateMode::On) {
        if (!cfg.redundant_port.has_value() || !cfg.redundant_video.has_value()) {
            throw std::runtime_error("Internal duplicate configuration error");
        }

        if (!cfg.metadata_only) {
            if (cfg.redundant_port->port_name.empty()) {
                throw std::runtime_error("--redundant-port-name is required when --duplicate is used");
            }

            if (is_zero_ipv4(cfg.redundant_port->local_ip)) {
                throw std::runtime_error("--redundant-local-ip is required when --duplicate is used");
            }
        }

        if (is_zero_ipv4(cfg.redundant_video->dst_ip)) {
            throw std::runtime_error("--redundant-video-dst-ip is required when --duplicate is used");
        }

        if (cfg.redundant_video->udp_port == 0) {
            cfg.redundant_video->udp_port = static_cast<std::uint16_t>(cfg.primary_video.udp_port + 2);
        }
    }

    return cfg;
}

class NdiRuntime final {
  public:
    NdiRuntime() { load(); }

    ~NdiRuntime() {
        if (ndi_) {
            ndi_->destroy();
            ndi_ = nullptr;
        }

#if defined(__linux__)
        if (handle_) {
            dlclose(handle_);
            handle_ = nullptr;
        }
#endif
    }

    NdiRuntime(const NdiRuntime &) = delete;
    NdiRuntime &operator=(const NdiRuntime &) = delete;

    [[nodiscard]] const NDIlib_v6 *get() const noexcept { return ndi_; }

  private:
#if defined(__linux__)
    using NDIlibV6LoadFn = const NDIlib_v6 *(*)();

    [[nodiscard]] static std::vector<std::string> candidate_library_paths() {
        std::vector<std::string> paths{};

        if (const char *env_dir = std::getenv("NDI_RUNTIME_DIR_V6"); env_dir && env_dir[0] != '\0') {
            paths.emplace_back(std::string(env_dir) + "/libndi.so.6");
            paths.emplace_back(std::string(env_dir) + "/libndi.so");
        }

        if (const char *env_dir = std::getenv("NDILIB_REDIST_FOLDER"); env_dir && env_dir[0] != '\0') {
            paths.emplace_back(std::string(env_dir) + "/libndi.so.6");
            paths.emplace_back(std::string(env_dir) + "/libndi.so");
        }

        paths.emplace_back("libndi.so.6");
        paths.emplace_back("libndi.so");

        paths.emplace_back("/usr/lib/libndi.so.6");
        paths.emplace_back("/usr/lib/libndi.so");
        paths.emplace_back("/usr/lib64/libndi.so.6");
        paths.emplace_back("/usr/lib64/libndi.so");
        paths.emplace_back("/usr/local/lib/libndi.so.6");
        paths.emplace_back("/usr/local/lib/libndi.so");

        return paths;
    }

    void load() {
        for (const auto &path : candidate_library_paths()) {
            handle_ = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
            if (!handle_) {
                continue;
            }

            const auto load_fn = reinterpret_cast<NDIlibV6LoadFn>(dlsym(handle_, "NDIlib_v6_load"));
            if (!load_fn) {
                dlclose(handle_);
                handle_ = nullptr;
                continue;
            }

            ndi_ = load_fn();
            if (!ndi_) {
                dlclose(handle_);
                handle_ = nullptr;
                continue;
            }

            if (!ndi_->initialize()) {
                ndi_ = nullptr;
                dlclose(handle_);
                handle_ = nullptr;
                continue;
            }

            return;
        }

        throw std::runtime_error("NDI runtime was not found. Set NDI_RUNTIME_DIR_V6 or install libndi.so.6.");
    }

    void *handle_ = nullptr;
#else
    void load() {
        throw std::runtime_error("Dynamic NDI runtime loading is currently implemented only for Linux");
    }
#endif

    const NDIlib_v6 *ndi_ = nullptr;
};

class NdiMetadataSender final {
  public:
    NdiMetadataSender(const NDIlib_v6 &ndi, const std::string &name) : ndi_(ndi) {
        NDIlib_send_create_t create{};
        create.p_ndi_name = name.c_str();
        create.p_groups = nullptr;
        create.clock_video = false;
        create.clock_audio = false;

        sender_ = ndi_.send_create(&create);
        if (!sender_) {
            throw std::runtime_error("NDI send_create failed");
        }
    }

    ~NdiMetadataSender() {
        if (sender_) {
            ndi_.send_destroy(sender_);
            sender_ = nullptr;
        }
    }

    NdiMetadataSender(const NdiMetadataSender &) = delete;
    NdiMetadataSender &operator=(const NdiMetadataSender &) = delete;

    void publish_metadata(std::string &metadata) {
        if (metadata.size() + 1 > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            throw std::runtime_error("NDI metadata payload is too large");
        }

        NDIlib_metadata_frame_t frame{};
        frame.length = static_cast<int>(metadata.size() + 1);
        frame.timecode = NDIlib_send_timecode_synthesize;
        frame.p_data = metadata.data();

        ndi_.send_add_connection_metadata(sender_, &frame);
        ndi_.send_send_metadata(sender_, &frame);
    }

  private:
    const NDIlib_v6 &ndi_;
    NDIlib_send_instance_t sender_ = nullptr;
};

void fill_mtl_port(mtl_init_params &params, const mtl_port port_index, const MtlPortConfig &port_cfg,
                   const mtl_pmd_type pmd) {
    std::snprintf(params.port[port_index], MTL_PORT_MAX_LEN, "%s", port_cfg.port_name.c_str());

    params.pmd[port_index] = pmd;
    params.net_proto[port_index] = MTL_PROTO_STATIC;

    params.tx_queues_cnt[port_index] = 1;
    params.rx_queues_cnt[port_index] = 0;

    std::memcpy(params.sip_addr[port_index], port_cfg.local_ip.data(), port_cfg.local_ip.size());
}

[[nodiscard]] mtl_init_params make_mtl_init_params(const AppConfig &cfg) {
    mtl_init_params params{};

    params.num_ports = cfg.duplicate_mode == DuplicateMode::On ? 2 : 1;

    fill_mtl_port(params, MTL_PORT_P, cfg.primary_port, cfg.pmd);

    if (cfg.duplicate_mode == DuplicateMode::On) {
        fill_mtl_port(params, MTL_PORT_R, *cfg.redundant_port, cfg.pmd);
    }

    params.flags |= MTL_FLAG_DEV_AUTO_START_STOP;

    return params;
}

void fill_tx_session_leg(st20p_tx_ops &ops, const mtl_session_port session_port, const MtlPortConfig &port,
                         const VideoLegConfig &leg) {
    std::memcpy(ops.port.dip_addr[session_port], leg.dst_ip.data(), leg.dst_ip.size());
    std::snprintf(ops.port.port[session_port], MTL_PORT_MAX_LEN, "%s", port.port_name.c_str());
    ops.port.udp_port[session_port] = leg.udp_port;
}

[[nodiscard]] st20p_tx_ops make_st20p_tx_ops(const AppConfig &cfg) {
    const VideoModeSpec mode = video_mode_spec(cfg.video_mode);

    st20p_tx_ops ops{};

    ops.name = "st2110_mtl_video_tx_test";
    ops.priv = nullptr;

    ops.port.num_port = cfg.duplicate_mode == DuplicateMode::On ? 2 : 1;
    fill_tx_session_leg(ops, MTL_SESSION_PORT_P, cfg.primary_port, cfg.primary_video);

    if (cfg.duplicate_mode == DuplicateMode::On) {
        fill_tx_session_leg(ops, MTL_SESSION_PORT_R, *cfg.redundant_port, *cfg.redundant_video);
    }

    ops.port.payload_type = cfg.video_payload_type;

    ops.width = mode.width;
    ops.height = mode.height;
    ops.fps = mode.mtl_fps;
    ops.interlaced = false;

    ops.input_fmt = ST_FRAME_FMT_UYVY;
    ops.transport_fmt = ST20_FMT_YUV_422_8BIT;
    ops.transport_packing = to_mtl_packing(cfg.packing);

    ops.device = ST_PLUGIN_DEVICE_AUTO;
    ops.framebuff_cnt = cfg.frame_buffer_count;
    ops.flags = ST20P_TX_FLAG_BLOCK_GET;

    return ops;
}

[[nodiscard]] std::uint8_t scale_to_u8(std::uint32_t value, std::uint32_t max_value, std::uint8_t min_out,
                                       std::uint8_t max_out) {
    if (max_value == 0) {
        return min_out;
    }

    const std::uint32_t span = static_cast<std::uint32_t>(max_out - min_out);
    return static_cast<std::uint8_t>(min_out + (value * span) / max_value);
}

void fill_uyvy_animated_gradient(st_frame &frame, std::uint64_t frame_index) {
    if (!frame.addr[0]) {
        return;
    }

    const std::uint32_t width = frame.width;
    const std::uint32_t height = frame.height;
    const std::size_t stride = frame.linesize[0] != 0 ? frame.linesize[0] : static_cast<std::size_t>(width) * 2U;

    auto *base = static_cast<std::uint8_t *>(frame.addr[0]);

    const std::uint32_t phase_x = static_cast<std::uint32_t>((frame_index * 8U) % std::max<std::uint32_t>(width, 1));
    const std::uint32_t phase_y = static_cast<std::uint32_t>((frame_index * 5U) % std::max<std::uint32_t>(height, 1));
    const std::uint32_t phase_c =
        static_cast<std::uint32_t>((frame_index * 3U) % std::max<std::uint32_t>(width + height, 1));

    for (std::uint32_t y = 0; y < height; ++y) {
        auto *row = base + static_cast<std::size_t>(y) * stride;

        for (std::uint32_t x = 0; x + 1 < width; x += 2) {
            const std::uint32_t x0 = (x + phase_x) % width;
            const std::uint32_t x1 = (x + 1 + phase_x) % width;
            const std::uint32_t yy = (y + phase_y) % height;
            const std::uint32_t c = (x + y + phase_c) % (width + height);

            const std::uint8_t y0 = scale_to_u8(x0, width - 1, 16, 235);
            const std::uint8_t y1 = scale_to_u8(x1, width - 1, 16, 235);
            const std::uint8_t u = scale_to_u8(yy, height - 1, 64, 192);
            const std::uint8_t v = scale_to_u8(c, width + height - 1, 64, 192);

            const std::size_t offset = static_cast<std::size_t>(x) * 2U;

            row[offset + 0] = u;
            row[offset + 1] = y0;
            row[offset + 2] = v;
            row[offset + 3] = y1;
        }
    }

    frame.data_size = stride * height;
}

class MtlVideoSender final {
  public:
    explicit MtlVideoSender(AppConfig cfg) : cfg_(std::move(cfg)) {}

    ~MtlVideoSender() { stop(); }

    MtlVideoSender(const MtlVideoSender &) = delete;
    MtlVideoSender &operator=(const MtlVideoSender &) = delete;

    void start() {
        if (mt_ || tx_) {
            return;
        }

        auto params = make_mtl_init_params(cfg_);

        mt_ = mtl_init(&params);
        if (!mt_) {
            throw std::runtime_error("mtl_init failed");
        }

        auto ops = make_st20p_tx_ops(cfg_);

        tx_ = st20p_tx_create(mt_, &ops);
        if (!tx_) {
            mtl_uninit(mt_);
            mt_ = nullptr;
            throw std::runtime_error("st20p_tx_create failed");
        }

        const std::size_t frame_size = st20p_tx_frame_size(tx_);
        std::cout << "MTL video TX started. frame_size=" << frame_size << " bytes\n";

        stop_requested_.store(false);
        thread_ = std::jthread([this](std::stop_token token) { frame_loop(token); });
    }

    void stop() noexcept {
        stop_requested_.store(true);

        if (tx_) {
            st20p_tx_wake_block(tx_);
        }

        thread_ = {};

        if (tx_) {
            st20p_tx_free(tx_);
            tx_ = nullptr;
        }

        if (mt_) {
            mtl_uninit(mt_);
            mt_ = nullptr;
        }
    }

  private:
    void frame_loop(std::stop_token token) noexcept {
        std::uint64_t frame_index = 0;

        while (!token.stop_requested() && !stop_requested_.load()) {
            st_frame *frame = st20p_tx_get_frame(tx_);
            if (!frame) {
                continue;
            }

            fill_uyvy_animated_gradient(*frame, frame_index);
            st20p_tx_put_frame(tx_, frame);

            ++frame_index;
        }
    }

    AppConfig cfg_{};

    mtl_handle mt_ = nullptr;
    st20p_tx_handle tx_ = nullptr;

    std::atomic_bool stop_requested_{true};
    std::jthread thread_{};
};

} // namespace

int main(int argc, char **argv) {
    try {
        AppConfig cfg = parse_args(argc, argv);

        std::string metadata = build_st2110_sdp_metadata(cfg);

        NdiRuntime runtime{};
        const NDIlib_v6 *ndi = runtime.get();
        if (!ndi) {
            throw std::runtime_error("NDI runtime is unavailable");
        }

        NdiMetadataSender ndi_sender(*ndi, cfg.ndi_name);

        std::optional<MtlVideoSender> video_sender{};
        if (!cfg.metadata_only) {
            video_sender.emplace(cfg);
            video_sender->start();
        }

        const auto deadline = Clock::now() + cfg.duration;

        std::cout << "Publishing NDI source: " << cfg.ndi_name << '\n';
        std::cout << "Video SDP:\n" << build_video_sdp(cfg) << '\n';

        while (Clock::now() < deadline) {
            ndi_sender.publish_metadata(metadata);
            std::cout << "Published ST 2110 video SDP metadata\n";
            std::this_thread::sleep_for(cfg.metadata_repeat_interval);
        }

        video_sender.reset();

        std::cout << "Done\n";
        return 0;
    } catch (const std::exception &ex) {
        std::cerr << "st2110_mtl_send_test: " << ex.what() << '\n';
        return 1;
    }
}
#include <Processing.NDI.Lib.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#if defined(__linux__)
#include <dlfcn.h>
#endif

namespace {

struct AppConfig {
    std::string ndi_name = "ST2110 MTL Send Test";
    std::optional<std::filesystem::path> video_sdp_file{};
    std::optional<std::filesystem::path> audio_sdp_file{};
    std::chrono::milliseconds duration{60'000};
    std::chrono::milliseconds repeat_interval{1000};
};

[[nodiscard]] std::string read_text_file(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Cannot open file: " + path.string());
    }

    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
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

[[nodiscard]] std::string build_st2110_sdp_metadata(const AppConfig &cfg) {
    std::string xml;
    xml += "<st2110_sdp_bundle>\n";

    if (cfg.video_sdp_file.has_value()) {
        xml += wrap_sdp_metadata_element("video", "video", cfg.ndi_name + " Video",
                                         read_text_file(*cfg.video_sdp_file));
    }

    if (cfg.audio_sdp_file.has_value()) {
        xml += wrap_sdp_metadata_element("audio", "audio", cfg.ndi_name + " Audio",
                                         read_text_file(*cfg.audio_sdp_file));
    }

    xml += "</st2110_sdp_bundle>\n";

    return xml;
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

[[nodiscard]] AppConfig parse_args(int argc, char **argv) {
    AppConfig cfg{};

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];

        const auto require_value = [&](std::string_view option) -> std::string_view {
            if (i + 1 >= argc) {
                throw std::runtime_error("Missing value for " + std::string(option));
            }
            return argv[++i];
        };

        if (arg == "--name") {
            cfg.ndi_name = std::string(require_value(arg));
        } else if (arg == "--video-sdp-file") {
            cfg.video_sdp_file = std::filesystem::path(require_value(arg));
        } else if (arg == "--audio-sdp-file") {
            cfg.audio_sdp_file = std::filesystem::path(require_value(arg));
        } else if (arg == "--duration-ms") {
            cfg.duration = std::chrono::milliseconds(parse_positive_int(require_value(arg), arg));
        } else if (arg == "--repeat-ms") {
            cfg.repeat_interval = std::chrono::milliseconds(parse_positive_int(require_value(arg), arg));
        } else if (arg == "--help" || arg == "-h") {
            std::cout
                << "Usage:\n"
                << "  st2110_mtl_send_test --name NAME "
                   "[--video-sdp-file video.sdp] [--audio-sdp-file audio.sdp] "
                   "[--duration-ms 60000] [--repeat-ms 1000]\n\n"
                << "Current MVP publishes only NDI SDP metadata. MTL media TX is added in the next step.\n";
            std::exit(0);
        } else {
            throw std::runtime_error("Unknown argument: " + std::string(arg));
        }
    }

    if (!cfg.video_sdp_file.has_value() && !cfg.audio_sdp_file.has_value()) {
        throw std::runtime_error("At least one of --video-sdp-file or --audio-sdp-file is required");
    }

    if (cfg.ndi_name.empty()) {
        throw std::runtime_error("--name must not be empty");
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
        paths.emplace_back("/app/plugins/DistroAV/extra/lib/libndi.so.6");
        paths.emplace_back("/app/plugins/DistroAV/extra/lib/libndi.so");

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

        throw std::runtime_error(
            "NDI runtime was not found. Set NDI_RUNTIME_DIR_V6 or install libndi.so.6.");
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

} // namespace

int main(int argc, char **argv) {
    try {
        const AppConfig cfg = parse_args(argc, argv);

        std::string metadata = build_st2110_sdp_metadata(cfg);

        NdiRuntime runtime{};
        const NDIlib_v6 *ndi = runtime.get();
        if (!ndi) {
            throw std::runtime_error("NDI runtime is unavailable");
        }

        NdiMetadataSender sender(*ndi, cfg.ndi_name);

        const auto deadline = std::chrono::steady_clock::now() + cfg.duration;

        std::cout << "Publishing NDI source: " << cfg.ndi_name << '\n';
        std::cout << "Metadata payload:\n" << metadata << '\n';

        while (std::chrono::steady_clock::now() < deadline) {
            sender.publish_metadata(metadata);
            std::cout << "Published ST 2110 SDP metadata\n";
            std::this_thread::sleep_for(cfg.repeat_interval);
        }

        std::cout << "Done\n";
        return 0;
    } catch (const std::exception &ex) {
        std::cerr << "st2110_mtl_send_test: " << ex.what() << '\n';
        return 1;
    }
}
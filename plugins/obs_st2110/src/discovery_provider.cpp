#include <obs_st2110/discovery_provider.hpp>

#include <obs-module.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <cstdlib>

#ifndef ST2110_HAS_NDI_DISCOVERY
#define ST2110_HAS_NDI_DISCOVERY 0
#endif

#if ST2110_HAS_NDI_DISCOVERY
#include <Processing.NDI.Lib.h>

#if defined(__linux__)
#include <dlfcn.h>
#endif
#endif

namespace obs_st2110 {
namespace {

class NullDiscoveryProvider final : public IDiscoveryProvider {
  public:
    [[nodiscard]] std::vector<DiscoveredSourceListItem> list_sources() override { return {}; }

    [[nodiscard]] std::optional<SelectedDiscoveredSource> resolve_source(std::string_view selection_key) override {
        (void)selection_key;
        return std::nullopt;
    }
};

#if ST2110_HAS_NDI_DISCOVERY

using Clock = std::chrono::steady_clock;

[[nodiscard]] std::string trim_copy(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r' ||
                              value.front() == '\n')) {
        value.remove_prefix(1);
    }

    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r' ||
                              value.back() == '\n')) {
        value.remove_suffix(1);
    }

    return std::string(value);
}

[[nodiscard]] std::string unwrap_cdata_copy(std::string_view value) {
    value = std::string_view(trim_copy(value));

    constexpr std::string_view cdata_prefix = "<![CDATA[";
    constexpr std::string_view cdata_suffix = "]]>";

    if (value.size() >= cdata_prefix.size() + cdata_suffix.size() && value.starts_with(cdata_prefix) &&
        value.ends_with(cdata_suffix)) {
        value.remove_prefix(cdata_prefix.size());
        value.remove_suffix(cdata_suffix.size());
    }

    return trim_copy(value);
}

[[nodiscard]] std::optional<std::string> read_xml_attribute(std::string_view opening_tag, std::string_view name) {
    const std::string needle_double = std::string(name) + "=\"";
    const std::string needle_single = std::string(name) + "='";

    std::size_t pos = opening_tag.find(needle_double);
    char quote = '"';

    if (pos == std::string_view::npos) {
        pos = opening_tag.find(needle_single);
        quote = '\'';
    }

    if (pos == std::string_view::npos) {
        return std::nullopt;
    }

    pos += name.size() + 2;

    const std::size_t end = opening_tag.find(quote, pos);
    if (end == std::string_view::npos) {
        return std::nullopt;
    }

    return std::string(opening_tag.substr(pos, end - pos));
}

[[nodiscard]] std::optional<st2110::SdpMediaKind> parse_declared_media_kind(std::string_view value) {
    if (value == "video") {
        return st2110::SdpMediaKind::Video;
    }

    if (value == "audio") {
        return st2110::SdpMediaKind::Audio;
    }

    return std::nullopt;
}

void collect_sdp_metadata_elements(std::vector<ProviderSdpObject> &out, std::string_view metadata,
                                   const std::string_view ndi_source_name, const std::string_view tag_name,
                                   const std::optional<st2110::SdpMediaKind> forced_media_kind) {
    const std::string open_prefix = "<" + std::string(tag_name);
    const std::string close_tag = "</" + std::string(tag_name) + ">";

    std::size_t search_pos = 0;

    while (search_pos < metadata.size()) {
        const std::size_t open_pos = metadata.find(open_prefix, search_pos);
        if (open_pos == std::string_view::npos) {
            return;
        }

        const std::size_t open_end = metadata.find('>', open_pos);
        if (open_end == std::string_view::npos) {
            return;
        }

        const std::string_view opening_tag = metadata.substr(open_pos, open_end - open_pos + 1);
        if (!opening_tag.empty() && opening_tag.size() >= 2 && opening_tag[opening_tag.size() - 2] == '/') {
            search_pos = open_end + 1;
            continue;
        }

        const std::size_t close_pos = metadata.find(close_tag, open_end + 1);
        if (close_pos == std::string_view::npos) {
            return;
        }

        const std::string raw_sdp = unwrap_cdata_copy(metadata.substr(open_end + 1, close_pos - open_end - 1));
        if (!raw_sdp.empty()) {
            std::optional<st2110::SdpMediaKind> declared_kind = forced_media_kind;

            if (!declared_kind.has_value()) {
                if (auto media_attr = read_xml_attribute(opening_tag, "media"); media_attr.has_value()) {
                    declared_kind = parse_declared_media_kind(*media_attr);
                }
            }

            const std::string object_id =
                read_xml_attribute(opening_tag, "id").value_or(std::string("sdp_") + std::to_string(out.size()));

            const std::string display_name =
                read_xml_attribute(opening_tag, "name")
                    .value_or(std::string(ndi_source_name) + " SDP " + std::to_string(out.size() + 1));

            out.emplace_back(ProviderSdpObject{
                .provider_object_id = object_id,
                .display_name = display_name,
                .raw_sdp = raw_sdp,
                .declared_media_kind = declared_kind,
            });
        }

        search_pos = close_pos + close_tag.size();
    }
}

[[nodiscard]] std::vector<ProviderSdpObject> extract_st2110_sdp_objects(std::string_view metadata,
                                                                         std::string_view ndi_source_name) {
    std::vector<ProviderSdpObject> result{};

    collect_sdp_metadata_elements(result, metadata, ndi_source_name, "st2110_sdp", std::nullopt);
    collect_sdp_metadata_elements(result, metadata, ndi_source_name, "st2110_sdp_video", st2110::SdpMediaKind::Video);
    collect_sdp_metadata_elements(result, metadata, ndi_source_name, "st2110_sdp_audio", st2110::SdpMediaKind::Audio);

    return result;
}

class NdiLibrary final {
  public:
    NdiLibrary() { load(); }

    ~NdiLibrary() {
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

    NdiLibrary(const NdiLibrary &) = delete;
    NdiLibrary &operator=(const NdiLibrary &) = delete;

    [[nodiscard]] const NDIlib_v6 *get() const noexcept { return ndi_; }

    [[nodiscard]] bool available() const noexcept { return ndi_ != nullptr; }

  private:
#if defined(__linux__)
    using NDIlibV6LoadFn = const NDIlib_v6 *(*)();

    [[nodiscard]] static std::vector<std::string> linux_candidate_library_paths() {
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
        for (const std::string &path : linux_candidate_library_paths()) {
            handle_ = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
            if (!handle_) {
                continue;
            }

            auto load_fn = reinterpret_cast<NDIlibV6LoadFn>(dlsym(handle_, "NDIlib_v6_load"));
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
                obs_log(LOG_WARNING, "ST2110 NDI discovery: NDI library loaded but initialize() failed");
                ndi_ = nullptr;
                dlclose(handle_);
                handle_ = nullptr;
                continue;
            }

            obs_log(LOG_INFO, "ST2110 NDI discovery: NDI library initialized: %s", ndi_->version());
            return;
        }

        obs_log(LOG_WARNING, "ST2110 NDI discovery: NDI runtime library was not found");
    }

    void *handle_ = nullptr;
#else
    void load() {
        /*
         * Current MVP implements dynamic NDI loading only for Linux.
         * Keep the provider compiled as unavailable on other platforms until
         * platform-specific dynamic loading paths are added.
         */
        obs_log(LOG_WARNING, "ST2110 NDI discovery: dynamic NDI loading is not implemented for this platform");
    }
#endif

    const NDIlib_v6 *ndi_ = nullptr;
};

[[nodiscard]] NdiLibrary &ndi_library() {
    static NdiLibrary lib{};
    return lib;
}

class NdiDiscoveryProvider final : public IDiscoveryProvider {
  public:
    [[nodiscard]] std::vector<DiscoveredSourceListItem> list_sources() override {
        const NDIlib_v6 *ndi = ndi_library().get();
        if (!ndi) {
            return {};
        }

        NDIlib_find_create_t find_desc{};
        find_desc.show_local_sources = true;
        find_desc.p_groups = nullptr;

        NDIlib_find_instance_t finder = ndi->find_create_v2(&find_desc);
        if (!finder) {
            return {};
        }

        ndi->find_wait_for_sources(finder, 1000);

        std::uint32_t n_sources = 0;
        const NDIlib_source_t *sources = ndi->find_get_current_sources(finder, &n_sources);

        std::vector<DiscoveredSourceListItem> result{};
        result.reserve(n_sources);

        for (std::uint32_t i = 0; i < n_sources; ++i) {
            if (!sources[i].p_ndi_name || sources[i].p_ndi_name[0] == '\0') {
                continue;
            }

            result.emplace_back(DiscoveredSourceListItem{
                .selection_key = sources[i].p_ndi_name,
                .display_name = sources[i].p_ndi_name,
            });
        }

        ndi->find_destroy(finder);

        std::sort(result.begin(), result.end(), [](const auto &lhs, const auto &rhs) {
            return lhs.display_name < rhs.display_name;
        });

        return result;
    }

    [[nodiscard]] std::optional<SelectedDiscoveredSource> resolve_source(std::string_view selection_key) override {
        const NDIlib_v6 *ndi = ndi_library().get();
        if (!ndi || selection_key.empty()) {
            return std::nullopt;
        }

        std::string selected_name(selection_key);

        NDIlib_recv_create_v3_t recv_desc{};
        recv_desc.source_to_connect_to.p_ndi_name = selected_name.c_str();
        recv_desc.p_ndi_recv_name = "ST2110 OBS SDP Discovery";
        recv_desc.bandwidth = NDIlib_recv_bandwidth_metadata_only;
        recv_desc.color_format = NDIlib_recv_color_format_fastest;
        recv_desc.allow_video_fields = false;

        NDIlib_recv_instance_t receiver = ndi->recv_create_v3(&recv_desc);
        if (!receiver) {
            return std::nullopt;
        }

        std::vector<ProviderSdpObject> sdp_objects{};

        const auto deadline = Clock::now() + std::chrono::milliseconds(3000);

        while (Clock::now() < deadline && sdp_objects.empty()) {
            NDIlib_video_frame_v2_t video_frame{};
            NDIlib_audio_frame_v3_t audio_frame{};
            NDIlib_metadata_frame_t metadata_frame{};

            const NDIlib_frame_type_e frame_type =
                ndi->recv_capture_v3(receiver, &video_frame, &audio_frame, &metadata_frame, 250);

            switch (frame_type) {
            case NDIlib_frame_type_metadata: {
                if (metadata_frame.p_data) {
                    sdp_objects = extract_st2110_sdp_objects(metadata_frame.p_data, selected_name);
                }

                ndi->recv_free_metadata(receiver, &metadata_frame);
                break;
            }

            case NDIlib_frame_type_video:
                ndi->recv_free_video_v2(receiver, &video_frame);
                break;

            case NDIlib_frame_type_audio:
                ndi->recv_free_audio_v3(receiver, &audio_frame);
                break;

            case NDIlib_frame_type_status_change:
            case NDIlib_frame_type_none:
            case NDIlib_frame_type_error:
            default:
                break;
            }
        }

        ndi->recv_destroy(receiver);

        return SelectedDiscoveredSource{
            .provider_id = "ndi",
            .source_id = selected_name,
            .display_name = selected_name,
            .sdp_objects = std::move(sdp_objects),
        };
    }
};

#endif // ST2110_HAS_NDI_DISCOVERY

} // namespace

std::unique_ptr<IDiscoveryProvider> create_discovery_provider() {
#if ST2110_HAS_NDI_DISCOVERY
    return std::make_unique<NdiDiscoveryProvider>();
#else
    /*
     * Correct fallback provider.
     *
     * The plugin may still be built without NDI SDK headers. In that case the
     * provider boundary exists, but it does not invent fake SDP or manual debug
     * input.
     */
    return std::make_unique<NullDiscoveryProvider>();
#endif
}

} // namespace obs_st2110
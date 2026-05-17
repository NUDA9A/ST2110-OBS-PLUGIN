#ifndef ST2110_OBS_PLUGIN_SOURCE_CONFIG_HPP
#define ST2110_OBS_PLUGIN_SOURCE_CONFIG_HPP

#include <st2110/contracts/settings.hpp>
#include <st2110/ingress/shared/sdp_common.hpp>

#include <optional>
#include <string>
#include <vector>

namespace obs_st2110 {

struct ProviderSdpObject {
    std::string provider_object_id{};
    std::string display_name{};

    std::string raw_sdp{};

    std::optional<st2110::SdpMediaKind> declared_media_kind{};

    friend bool operator==(const ProviderSdpObject &, const ProviderSdpObject &) = default;
};

struct SelectedDiscoveredSource {
    std::string provider_id{};
    std::string source_id{};
    std::string display_name{};

    std::vector<ProviderSdpObject> sdp_objects{};

    friend bool operator==(const SelectedDiscoveredSource &, const SelectedDiscoveredSource &) = default;
};

struct SourceConfig {
    std::optional<SelectedDiscoveredSource> selected_source{};

    st2110::Settings receive_settings{};

    friend bool operator==(const SourceConfig &, const SourceConfig &) = default;
};

} // namespace obs_st2110

#endif // ST2110_OBS_PLUGIN_SOURCE_CONFIG_HPP
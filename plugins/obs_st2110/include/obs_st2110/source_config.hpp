#ifndef ST2110_OBS_PLUGIN_SOURCE_CONFIG_HPP
#define ST2110_OBS_PLUGIN_SOURCE_CONFIG_HPP

#include <st2110/contracts/settings.hpp>
#include <st2110/foundation/timestamp.hpp>
#include <st2110/ingress/shared/sdp_common.hpp>

#include <optional>
#include <string>
#include <vector>

namespace obs_st2110 {

struct ProviderSdpObject {
    std::string provider_object_id{};
    std::string display_name{};

    /*
     * Raw SDP supplied by discovery/provider/control plane.
     *
     * This is not a user debug override. It is the source material for the
     * project ingress pipeline:
     *
     *   raw SDP -> classification -> media-specific parser -> receive bootstrap
     */
    std::string raw_sdp{};

    /*
     * Optional provider-declared media kind.
     *
     * If absent, the source runtime/orchestrator must classify the raw SDP
     * before dispatching to the media-specific parser.
     */
    std::optional<st2110::SdpMediaKind> declared_media_kind{};

    friend bool operator==(const ProviderSdpObject &, const ProviderSdpObject &) = default;
};

struct SelectedDiscoveredSource {
    std::string provider_id{};
    std::string source_id{};
    std::string display_name{};

    /*
     * One selected provider source may expose:
     * - one video SDP object;
     * - one audio SDP object;
     * - or one object per essence selected by the provider.
     */
    std::vector<ProviderSdpObject> sdp_objects{};

    friend bool operator==(const SelectedDiscoveredSource &, const SelectedDiscoveredSource &) = default;
};

struct SourceConfig {
    bool start_when_active = true;

    std::optional<SelectedDiscoveredSource> selected_source{};

    st2110::Settings receive_settings{};

    st2110::TimestampNs playout_delay_ns = 0;

    friend bool operator==(const SourceConfig &, const SourceConfig &) = default;
};

} // namespace obs_st2110

#endif // ST2110_OBS_PLUGIN_SOURCE_CONFIG_HPP
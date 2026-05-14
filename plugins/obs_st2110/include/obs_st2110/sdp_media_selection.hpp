#ifndef ST2110_OBS_PLUGIN_SDP_MEDIA_SELECTION_HPP
#define ST2110_OBS_PLUGIN_SDP_MEDIA_SELECTION_HPP

#include <obs_st2110/source_config.hpp>

#include <st2110/foundation/error.hpp>
#include <st2110/ingress/shared/sdp_common.hpp>

#include <expected>
#include <optional>

namespace obs_st2110 {

struct ClassifiedProviderSdpObject {
    ProviderSdpObject provider_object{};
    st2110::SdpMediaKind media_kind = st2110::SdpMediaKind::Video;
};

struct SelectedSourceMediaSet {
    std::optional<ClassifiedProviderSdpObject> video{};
    std::optional<ClassifiedProviderSdpObject> audio{};

    [[nodiscard]] bool has_video() const noexcept { return video.has_value(); }
    [[nodiscard]] bool has_audio() const noexcept { return audio.has_value(); }
    [[nodiscard]] bool empty() const noexcept { return !has_video() && !has_audio(); }
};

/*
 * Resolves provider-selected SDP objects to media-kind-specific SDP objects.
 *
 * This is not full SDP parsing.
 *
 * Ownership boundary:
 * - provider-declared media kind is trusted for dispatch;
 * - otherwise use lightweight SDP classification;
 * - media-specific parsers still own full media-specific validation.
 *
 * Current receive contract:
 * - at most one video SDP object per selected source;
 * - at most one audio SDP object per selected source;
 * - duplicate/redundant RTP topology belongs inside one SDP object, not as two
 *   unrelated objects of the same media kind.
 */
[[nodiscard]] std::expected<SelectedSourceMediaSet, st2110::Error>
resolve_selected_source_media_set(const SelectedDiscoveredSource &selected_source);

} // namespace obs_st2110

#endif // ST2110_OBS_PLUGIN_SDP_MEDIA_SELECTION_HPP
#include <obs_st2110/sdp_media_selection.hpp>

#include <expected>
#include <utility>

namespace obs_st2110 {
namespace {
[[nodiscard]] std::expected<st2110::SdpMediaKind, st2110::Error>
resolve_provider_sdp_media_kind(const ProviderSdpObject &sdp_object) {
    if (sdp_object.raw_sdp.empty()) {
        return std::unexpected(st2110::Error::InvalidValue);
    }

    if (sdp_object.declared_media_kind.has_value()) {
        return *sdp_object.declared_media_kind;
    }

    return st2110::classify_sdp_media_kind(sdp_object.raw_sdp);
}

[[nodiscard]] st2110::Error add_classified_sdp_object(SelectedSourceMediaSet &out,
                                                      ClassifiedProviderSdpObject classified) {
    switch (classified.media_kind) {
    case st2110::SdpMediaKind::Video:
        if (out.video.has_value()) {
            return st2110::Error::Unsupported;
        }

        out.video = std::move(classified);
        return st2110::Error::Ok;

    case st2110::SdpMediaKind::Audio:
        if (out.audio.has_value()) {
            return st2110::Error::Unsupported;
        }

        out.audio = std::move(classified);
        return st2110::Error::Ok;

    default:
        return st2110::Error::Unsupported;
    }
}
} // namespace

std::expected<SelectedSourceMediaSet, st2110::Error>
resolve_selected_source_media_set(const SelectedDiscoveredSource &selected_source) {
    if (selected_source.sdp_objects.empty()) {
        return std::unexpected(st2110::Error::InvalidValue);
    }

    SelectedSourceMediaSet out{};

    for (const auto &sdp_object : selected_source.sdp_objects) {
        auto media_kind = resolve_provider_sdp_media_kind(sdp_object);
        if (!media_kind.has_value()) {
            return std::unexpected(media_kind.error());
        }

        ClassifiedProviderSdpObject classified{
            .provider_object = sdp_object,
            .media_kind = *media_kind,
        };

        const st2110::Error err = add_classified_sdp_object(out, std::move(classified));
        if (err != st2110::Error::Ok) {
            return std::unexpected(err);
        }
    }

    if (out.empty()) {
        return std::unexpected(st2110::Error::InvalidValue);
    }

    return out;
}

} // namespace obs_st2110
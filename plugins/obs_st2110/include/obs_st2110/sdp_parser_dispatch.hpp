#ifndef ST2110_OBS_PLUGIN_SDP_PARSER_DISPATCH_HPP
#define ST2110_OBS_PLUGIN_SDP_PARSER_DISPATCH_HPP

#include <obs_st2110/sdp_media_selection.hpp>

#include <st2110/foundation/error.hpp>
#include <st2110/ingress/shared/parsed_sdp.hpp>

#include <expected>
#include <optional>

namespace obs_st2110 {

struct ParsedSelectedSourceStreams {
    std::optional<st2110::ParsedSdpStreamSet> video{};
    std::optional<st2110::ParsedSdpStreamSet> audio{};

    [[nodiscard]] bool has_video() const noexcept { return video.has_value(); }
    [[nodiscard]] bool has_audio() const noexcept { return audio.has_value(); }
    [[nodiscard]] bool empty() const noexcept { return !has_video() && !has_audio(); }
};

/*
 * Dispatches already-classified provider SDP objects to media-specific parsers.
 *
 * This stage does not classify media kind again.
 *
 * Ownership boundary:
 * - sdp_media_selection resolved the media kind;
 * - core media-specific SDP parsers own media-specific validation and typed
 *   ParsedSdpStreamSet construction.
 */
[[nodiscard]] std::expected<ParsedSelectedSourceStreams, st2110::Error>
parse_selected_source_streams(const SelectedSourceMediaSet &media_set);

} // namespace obs_st2110

#endif // ST2110_OBS_PLUGIN_SDP_PARSER_DISPATCH_HPP
#include <obs_st2110/sdp_parser_dispatch.hpp>

#include <st2110/ingress/audio/audio_sdp_parse.hpp>
#include <st2110/ingress/video/video_sdp_parse.hpp>

#include <expected>
#include <utility>

namespace obs_st2110 {

std::expected<ParsedSelectedSourceStreams, st2110::Error>
parse_selected_source_streams(const SelectedSourceMediaSet &media_set) {
    if (media_set.empty()) {
        return std::unexpected(st2110::Error::InvalidValue);
    }

    ParsedSelectedSourceStreams out{};

    if (media_set.video.has_value()) {
        auto parsed_video =
            st2110::parse_video_stream_signaling(media_set.video->provider_object.raw_sdp);

        if (!parsed_video.has_value()) {
            return std::unexpected(parsed_video.error());
        }

        out.video = std::move(*parsed_video);
    }

    if (media_set.audio.has_value()) {
        auto parsed_audio =
            st2110::parse_audio_stream_signaling(media_set.audio->provider_object.raw_sdp);

        if (!parsed_audio.has_value()) {
            return std::unexpected(parsed_audio.error());
        }

        out.audio = std::move(*parsed_audio);
    }

    if (out.empty()) {
        return std::unexpected(st2110::Error::InvalidValue);
    }

    return out;
}

} // namespace obs_st2110
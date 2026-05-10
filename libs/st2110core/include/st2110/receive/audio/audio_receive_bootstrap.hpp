#ifndef ST2110_OBS_AUDIO_RECEIVE_BOOTSTRAP_HPP
#define ST2110_OBS_AUDIO_RECEIVE_BOOTSTRAP_HPP

#include <st2110/ingress/shared/parsed_sdp.hpp>
#include <st2110/receive/shared/receive_bootstrap.hpp>

#include <optional>
#include <vector>

namespace st2110 {
struct AudioReceiveSignaledStream {
    ReceiveSignaledStream receive_signaled_stream{};

    AudioMediaDescription media{};
    std::optional<AudioChannelOrder> channel_order{};
};

struct AudioReceiveBootstrap {
    ReceiveBootstrap receive_bootstrap;
    AudioReceiveSignaledStream stream{};
};

inline AudioReceiveBootstrap project_parsed_audio_sdp_to_receive_bootstrap(const ParsedSdpStreamSet &parsed) {
    return AudioReceiveBootstrap{
        .receive_bootstrap = project_audio_receive_bootstrap(parsed),
        .stream = AudioReceiveSignaledStream{
            .receive_signaled_stream = project_receive_signaled_stream(parsed.legs[0].expected_payload_type,
                                                                       parsed.legs[0].audio_stream_signaling->timing),
            .media = parsed.legs[0].audio_stream_signaling->media,
            .channel_order = parsed.legs[0].audio_stream_signaling->channel_order}};
}
} // namespace st2110

#endif // ST2110_OBS_AUDIO_RECEIVE_BOOTSTRAP_HPP

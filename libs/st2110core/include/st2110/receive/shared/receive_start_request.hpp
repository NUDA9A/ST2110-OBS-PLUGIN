#ifndef ST2110_OBS_RECEIVE_START_REQUEST_HPP
#define ST2110_OBS_RECEIVE_START_REQUEST_HPP

#include <st2110/receive/video/video_receive_bootstrap.hpp>
#include <st2110/receive/audio/audio_receive_bootstrap.hpp>
#include <st2110/backends/receive_local_policy.hpp>

#include <variant>

namespace st2110 {
using ReceiveMediaBootstrap = std::variant<VideoReceiveBootstrap, AudioReceiveBootstrap>;

struct ReceiveStartRequest {
    ReceiveMediaBootstrap media{};
    ReceiveLocalPolicy local{};
};
}

#endif // ST2110_OBS_RECEIVE_START_REQUEST_HPP

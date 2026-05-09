#ifndef ST2110_OBS_PLUGIN_AUDIO_CHANNEL_ORDER_HPP
#define ST2110_OBS_PLUGIN_AUDIO_CHANNEL_ORDER_HPP

#include <st2110/foundation/error.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace st2110 {

enum class AudioChannelOrderConvention { Unspecified, Smpte2110, Other };

enum class AudioChannelGroupKind {
    Mono,
    Stereo,
    DualMono,
    MatrixStereo,
    FiveOne,
    SevenOne,
    TwentyTwoTwo,
    SdiGroup,
    Undefined,
    Other
};

struct AudioChannelOrderGroup {
    AudioChannelGroupKind kind = AudioChannelGroupKind::Other;
    std::string symbol{};
    uint16_t channel_count = 0;
};

struct AudioChannelOrder {
    AudioChannelOrderConvention convention = AudioChannelOrderConvention::Unspecified;
    std::optional<std::string> raw_value{};
    std::vector<AudioChannelOrderGroup> groups{};
    uint16_t declared_channel_count = 0;
};

[[nodiscard]] inline Error validate_audio_channel_order_group(const AudioChannelOrderGroup &group) {
    if (group.channel_count == 0) {
        return Error::InvalidValue;
    }

    if (group.symbol.empty()) {
        return Error::InvalidValue;
    }

    switch (group.kind) {
    case AudioChannelGroupKind::Mono:
        return group.channel_count == 1 ? Error::Ok : Error::InvalidValue;

    case AudioChannelGroupKind::Stereo:
    case AudioChannelGroupKind::DualMono:
    case AudioChannelGroupKind::MatrixStereo:
        return group.channel_count == 2 ? Error::Ok : Error::InvalidValue;

    case AudioChannelGroupKind::FiveOne:
        return group.channel_count == 6 ? Error::Ok : Error::InvalidValue;

    case AudioChannelGroupKind::SevenOne:
        return group.channel_count == 8 ? Error::Ok : Error::InvalidValue;

    case AudioChannelGroupKind::TwentyTwoTwo:
        return group.channel_count == 24 ? Error::Ok : Error::InvalidValue;

    case AudioChannelGroupKind::SdiGroup:
        return group.channel_count == 4 ? Error::Ok : Error::InvalidValue;

    case AudioChannelGroupKind::Undefined:
        return group.channel_count <= 64 ? Error::Ok : Error::InvalidValue;

    case AudioChannelGroupKind::Other:
        return Error::Ok;

    default:
        return Error::InvalidValue;
    }
}

[[nodiscard]] inline Error validate_audio_channel_order(const AudioChannelOrder &channel_order) {
    if (channel_order.raw_value.has_value() && channel_order.raw_value->empty()) {
        return Error::InvalidValue;
    }

    switch (channel_order.convention) {
    case AudioChannelOrderConvention::Unspecified:
        if (channel_order.raw_value.has_value()) {
            return Error::InvalidValue;
        }
        if (!channel_order.groups.empty()) {
            return Error::InvalidValue;
        }
        if (channel_order.declared_channel_count != 0) {
            return Error::InvalidValue;
        }
        return Error::Ok;

    case AudioChannelOrderConvention::Smpte2110: {
        if (channel_order.groups.empty()) {
            return Error::InvalidValue;
        }

        if (channel_order.declared_channel_count == 0) {
            return Error::InvalidValue;
        }

        uint32_t total_declared = 0;
        for (const auto &group : channel_order.groups) {
            if (const Error err = validate_audio_channel_order_group(group); err != Error::Ok) {
                return err;
            }

            total_declared += static_cast<uint32_t>(group.channel_count);
            if (total_declared > 65535U) {
                return Error::InvalidValue;
            }
        }

        if (total_declared != channel_order.declared_channel_count) {
            return Error::InvalidValue;
        }

        return Error::Ok;
    }

    case AudioChannelOrderConvention::Other:
        if (!channel_order.raw_value.has_value()) {
            return Error::InvalidValue;
        }
        if (!channel_order.groups.empty()) {
            return Error::InvalidValue;
        }
        if (channel_order.declared_channel_count != 0) {
            return Error::InvalidValue;
        }
        return Error::Ok;

    default:
        return Error::InvalidValue;
    }
}

[[nodiscard]] inline Error validate_audio_channel_order_against_channel_count(const AudioChannelOrder &channel_order,
                                                                              uint16_t channel_count) {
    if (channel_count == 0) {
        return Error::InvalidValue;
    }

    if (const Error err = validate_audio_channel_order(channel_order); err != Error::Ok) {
        return err;
    }

    if (channel_order.convention == AudioChannelOrderConvention::Smpte2110 &&
        channel_order.declared_channel_count > channel_count) {
        return Error::InvalidValue;
    }

    return Error::Ok;
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_AUDIO_CHANNEL_ORDER_HPP
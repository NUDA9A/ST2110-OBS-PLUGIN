#ifndef ST2110_OBS_PLUGIN_VIDEO_SEGMENT_CONSTRAINTS_HPP
#define ST2110_OBS_PLUGIN_VIDEO_SEGMENT_CONSTRAINTS_HPP

#include "bytes.hpp"
#include "error.hpp"
#include "pixel_format.hpp"
#include "st2110_20.hpp"

#include <expected>

namespace st2110 {
struct VideoSegmentConstraints {
    std::size_t pgroup_bytes = 0;
    uint16_t offset_alignment_samples = 0;
};

[[nodiscard]] inline std::expected<VideoSegmentConstraints, Error> video_segment_constraints(PixelFormat format) {
    VideoSegmentConstraints res;
    switch (format) {
    case PixelFormat::UYVY:
        res.pgroup_bytes = 4;
        res.offset_alignment_samples = 2;
        break;
    default:
        return std::unexpected(Error::Unsupported);
    }
    return res;
}

[[nodiscard]] inline Error validate_video_segment_for_format(PixelFormat format, const SrdHeader &header,
                                                             ByteSpan data) {
    auto constraints_expected = video_segment_constraints(format);
    if (!constraints_expected.has_value()) {
        return Error::Unsupported;
    }
    auto constraints = constraints_expected.value();
    switch (format) {
    case PixelFormat::UYVY:
        if (data.size() != header.length) {
            return Error::InvalidValue;
        }
        if (header.length % constraints.pgroup_bytes != 0) {
            return Error::InvalidValue;
        }
        if (header.offset % constraints.offset_alignment_samples != 0) {
            return Error::InvalidValue;
        }
        return Error::Ok;
    default:
        return Error::Unsupported;
    }
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_VIDEO_SEGMENT_CONSTRAINTS_HPP

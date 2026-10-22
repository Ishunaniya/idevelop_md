#pragma once

#include "evs/frame.h"
#include "evs/status.h"
#include "evs/task_config.h"
#include "evs/winch_ice.h"
#include <limits>

#if defined(EVS_WITH_OPENCV)
#include <opencv2/core.hpp>
#endif

namespace evs {

// Pixel threshold validation does not depend on OpenCV.
bool valid_ice_config(const IceConfig& cfg);

#if defined(EVS_WITH_OPENCV)
struct IceMeasurement {
    cv::Mat surface; // full-resolution mask for this frame's model rectangle
    cv::Mat snow;
    float ratio = -1.f;
    uint64_t surface_pixels = 0, snow_pixels = 0;
};

struct IceFramePixels {
    cv::Mat bgr, gray, candidate;
};

uint32_t prepare_ice_pixels(const Frame&, const IceConfig&, IceFramePixels&);
uint32_t measure_ice_pixels(const IceFramePixels&, const cv::Rect&, IceMeasurement&);

#endif

} // namespace evs

namespace evs::winch_ice::detail {
inline uint32_t ice_status(EvsStatus s) {
    return with_module(8, static_cast<uint32_t>(s));
}

inline uint32_t make_frame(const ImageData& image, uint32_t camera_id, Frame& frame) {
    if (image.format == ImageFormat::Gray)
        return ice_status(EvsStatus::NotSupported);
    if (!image.data)
        return ice_status(EvsStatus::NullPointer);
    if (image.width < 2 || image.height < 2 ||
        image.width > uint32_t(std::numeric_limits<int>::max() / 3) ||
        image.height > uint32_t(std::numeric_limits<int>::max() / 2) ||
        image.stride > uint32_t(std::numeric_limits<int>::max()))
        return ice_status(EvsStatus::InvalidImageSize);
    const bool nv12 = image.format == ImageFormat::NV12;
    if (image.format != ImageFormat::RGB && image.format != ImageFormat::BGR && !nv12)
        return ice_status(EvsStatus::InvalidImageData);
    if (image.channels != (nv12 ? 1u : 3u))
        return ice_status(EvsStatus::InvalidImageData);
    const uint64_t stride = image.stride ? image.stride : uint64_t(image.width) * (nv12 ? 1 : 3);
    if (stride < uint64_t(image.width) * (nv12 ? 1 : 3) ||
        (nv12 && ((image.width | image.height | stride) & 1)))
        return ice_status(EvsStatus::InvalidImageSize);
    const uint64_t required_bytes =
        stride * (uint64_t(image.height) + (nv12 ? image.height / 2 : 0));
    if (image.size_bytes && image.size_bytes < required_bytes)
        return ice_status(EvsStatus::InvalidImageSize);
    frame.data = image.data;
    frame.width = static_cast<int>(image.width);
    frame.height = static_cast<int>(image.height);
    frame.stride = static_cast<int>(stride);
    frame.camera_id = static_cast<int>(camera_id);
    frame.format = nv12 ? PixelFormat::NV12
                        : (image.format == ImageFormat::BGR ? PixelFormat::BGR : PixelFormat::RGB);
    frame.timestamp_us = int64_t(image.timestamp_ms) * 1000;
    return 0u;
}
} // namespace evs::winch_ice::detail

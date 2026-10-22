#include "ice_measurement.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <new>

#include "evs/status.h"

#if defined(EVS_WITH_OPENCV)
#include <opencv2/imgproc.hpp>
#endif

namespace evs {
bool valid_ice_config(const IceConfig& cfg) {
    return cfg.saturation_max >= 0 && cfg.saturation_max <= 255 && cfg.value_min >= 0 &&
           cfg.value_min <= 255;
}

#if defined(EVS_WITH_OPENCV)
uint32_t prepare_ice_pixels(const Frame& frame, const IceConfig& cfg, IceFramePixels& pixels) {
    pixels = IceFramePixels{};
    if (!valid_ice_config(cfg))
        return static_cast<uint32_t>(EvsStatus::ConfigInvalid);
    if (!frame.data)
        return static_cast<uint32_t>(EvsStatus::NullPointer);
    const int w = frame.width, h = frame.height;
    if (w < 2 || h < 2 || w > std::numeric_limits<int>::max() / 3 ||
        h > std::numeric_limits<int>::max() / 2 || frame.stride < 0 ||
        static_cast<int64_t>(w) * h > std::numeric_limits<int>::max())
        return static_cast<uint32_t>(EvsStatus::InvalidImageSize);
    const bool nv12 = frame.format == PixelFormat::NV12;
    if (!nv12 && frame.format != PixelFormat::RGB && frame.format != PixelFormat::BGR)
        return static_cast<uint32_t>(EvsStatus::InvalidImageData);
    const int row_bytes = nv12 ? w : w * 3;
    const int stride = frame.stride ? frame.stride : row_bytes;
    if (stride < row_bytes || (nv12 && ((w % 2) || (h % 2) || (stride % 2))))
        return static_cast<uint32_t>(EvsStatus::InvalidImageSize);
    try {
        cv::Mat bgr;
        if (nv12) {
            cv::Mat input(h + h / 2, w, CV_8UC1, const_cast<uint8_t*>(frame.data), stride);
            cv::cvtColor(input, bgr, cv::COLOR_YUV2BGR_NV12);
        } else {
            cv::Mat input(h, w, CV_8UC3, const_cast<uint8_t*>(frame.data), stride);
            if (frame.format == PixelFormat::RGB)
                cv::cvtColor(input, bgr, cv::COLOR_RGB2BGR);
            else
                bgr = input;
        }
        pixels.bgr = bgr;
        cv::cvtColor(bgr, pixels.gray, cv::COLOR_BGR2GRAY);
        cv::Mat hsv;
        cv::cvtColor(bgr, hsv, cv::COLOR_BGR2HSV);
        cv::inRange(hsv, cv::Scalar(0, 0, cfg.value_min), cv::Scalar(179, cfg.saturation_max, 255),
                    pixels.candidate);
        return 0u;
    } catch (const std::bad_alloc&) {
        return static_cast<uint32_t>(EvsStatus::MemoryAllocation);
    } catch (const cv::Exception& ex) {
        return static_cast<uint32_t>(ex.code == cv::Error::StsNoMem ? EvsStatus::MemoryAllocation
                                                                    : EvsStatus::PostprocessFailed);
    }
}

uint32_t measure_ice_pixels(const IceFramePixels& pixels, const cv::Rect& rect,
                            IceMeasurement& out) {
    out = IceMeasurement{};
    if (pixels.bgr.empty() || pixels.gray.empty() || pixels.candidate.empty())
        return static_cast<uint32_t>(EvsStatus::InvalidImageData);
    const int w = pixels.bgr.cols, h = pixels.bgr.rows;
    if (rect.x < 0 || rect.y < 0 || rect.width < 3 || rect.height < 3 || rect.x > w - rect.width ||
        rect.y > h - rect.height)
        return static_cast<uint32_t>(EvsStatus::InvalidImageSize);
    try {
        // Preserve the original full-frame morphology, including image border handling.
        out.surface = cv::Mat::zeros(h, w, CV_8UC1);
        out.surface(rect).setTo(255);
        const int area = rect.area();
        out.surface_pixels = static_cast<uint64_t>(area);
        cv::Scalar mean, stddev;
        cv::meanStdDev(pixels.gray, mean, stddev, out.surface);
        // Only observable failure cases: this is not a general occlusion detector.
        if (mean[0] < 12.0 || (mean[0] >= 250.0 && stddev[0] < 3.0))
            return kIceUnobservable;
        cv::bitwise_and(pixels.candidate, out.surface, out.snow);
        const cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
        cv::morphologyEx(out.snow, out.snow, cv::MORPH_OPEN, kernel);
        cv::morphologyEx(out.snow, out.snow, cv::MORPH_CLOSE, kernel);
        cv::bitwise_and(out.snow, out.surface, out.snow);
        out.snow_pixels = static_cast<uint64_t>(cv::countNonZero(out.snow));
        out.ratio = static_cast<float>(static_cast<double>(out.snow_pixels) / area);
        return 0u;
    } catch (const std::bad_alloc&) {
        out.surface.release();
        return static_cast<uint32_t>(EvsStatus::MemoryAllocation);
    } catch (const cv::Exception& ex) {
        out.surface.release();
        return static_cast<uint32_t>(ex.code == cv::Error::StsNoMem ? EvsStatus::MemoryAllocation
                                                                    : EvsStatus::PostprocessFailed);
    }
}
#endif
} // namespace evs

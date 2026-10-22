#pragma once
#include "evs/testing/ice_replay.h"
#include "evs/winch_ice.h"
#include "host_gate.h"
#include <cctype>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <opencv2/imgproc.hpp>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace ice_demo {
using namespace evs::winch_ice;

inline std::string token(std::istream& f) {
    std::string s;
    while (f >> s) {
        if (s[0] != '#')
            return s;
        f.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
    throw std::runtime_error("truncated PPM header");
}

struct PpmImage {
    int width = 0, height = 0;
    std::vector<uint8_t> pixels;

    explicit PpmImage(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        if (token(f) != "P6")
            throw std::runtime_error("requires P6 PPM: " + path);
        width = std::stoi(token(f));
        height = std::stoi(token(f));
        if (token(f) != "255" || width < 2 || height < 2 || int64_t(width) * height > 100000000)
            throw std::runtime_error("invalid PPM dimensions/range");
        char sep = 0;
        f.get(sep);
        if (!std::isspace(static_cast<unsigned char>(sep)))
            throw std::runtime_error("bad PPM separator");
        if (sep == '\r' && f.peek() == '\n')
            f.get();
        pixels.resize(size_t(width) * height * 3);
        f.read(reinterpret_cast<char*>(pixels.data()), pixels.size());
        if (!f)
            throw std::runtime_error("truncated PPM pixels");
    }

    ImageData frame() const {
        ImageData im;
        im.data = pixels.data();
        im.width = width;
        im.height = height;
        im.stride = width * 3;
        im.size_bytes = pixels.size();
        im.format = ImageFormat::RGB;
        return im;
    }
};

inline uint32_t fresh_time() {
    static uint32_t last = monotonic_timestamp_ms();
    auto now = monotonic_timestamp_ms();
    while (now == last) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        now = monotonic_timestamp_ms();
    }
    return last = now;
}

inline std::string reason(Reason r) {
    switch (r) {
    case Reason::None:
        return "none";
    case Reason::ObjectNotFound:
        return "object_not_found";
    case Reason::Unobservable:
        return "unobservable";
    case Reason::InvalidSurface:
        return "invalid_surface";
    case Reason::UnsupportedInput:
        return "unsupported_input";
    case Reason::OverlappingBoxes:
        return "overlapping_boxes";
    case Reason::UnexpectedObjectCount:
        return "unexpected_object_count";
    }
    return "unknown";
}

inline std::string json(const WinchSnowResult& r, HostDecision decision = HostDecision::Detect) {
    std::ostringstream s;
    s << std::setprecision(9);
    float ratio = r.objects.size() == 1 ? r.objects[0].ice_ratio : -1.f;
    s << "{\"api_called\":" << (decision == HostDecision::Detect ? "true" : "false")
      << ",\"host_reason\":\"" << host_reason(decision) << "\",\"status\":" << r.status
      << ",\"ratio\":" << ratio
      << ",\"alarm\":" << (r.alert_level == AlertLevel::ALERT_WARN ? 1 : 0)
      << ",\"result_code\":" << static_cast<int>(r.result_code)
      << ",\"camera_channel\":" << r.camera_channel
      << ",\"processed\":" << (r.processed ? "true" : "false") << ",\"reason\":\""
      << reason(r.reason) << "\"" << ",\"inference_time_ms\":" << r.inference_time_ms
      << ",\"processing_time_ms\":" << r.processing_time_ms << ",\"objects\":[";
    for (size_t i = 0; i < r.objects.size(); ++i) {
        if (i)
            s << ',';
        const auto& o = r.objects[i];
        s << "{\"id\":" << o.winch_id << ",\"state\":" << static_cast<int>(o.state)
          << ",\"valid\":" << (o.valid ? "true" : "false") << ",\"ratio\":" << o.ice_ratio
          << ",\"status\":" << o.status << ",\"reason\":\"" << reason(o.reason) << "\""
          << ",\"measurement_timestamp_ms\":" << o.measurement_timestamp_ms
          << ",\"box\":{\"x\":" << o.box.x << ",\"y\":" << o.box.y << ",\"width\":" << o.box.width
          << ",\"height\":" << o.box.height << ",\"class_id\":" << o.box.class_id
          << ",\"confidence\":" << o.box.confidence << "}"
          << ",\"surface_pixels\":" << o.surface_pixels << ",\"snow_pixels\":" << o.snow_pixels
          << '}';
    }
    s << "]}";
    return s.str();
}

inline void save(const std::string& path, const cv::Mat& mat) {
    std::ofstream f(path, std::ios::binary);
    f << (mat.channels() == 3 ? "P6\n" : "P5\n") << mat.cols << ' ' << mat.rows << "\n255\n";
    for (int y = 0; y < mat.rows; ++y)
        f.write(reinterpret_cast<const char*>(mat.ptr(y)), mat.cols * mat.channels());
    if (!f)
        throw std::runtime_error("cannot write " + path);
}

inline void artifacts(const ImageData& im, const WinchSnowResult& r, const std::string& stem,
                      HostDecision decision, std::vector<evs::testing::IceMaskSnapshot> masks) {
    auto parent = std::filesystem::path(stem).parent_path();
    if (!parent.empty())
        std::filesystem::create_directories(parent);
    // Reusing a prefix must never leave an old mask posing as this observation.
    for (const auto* suffix : {"_original.ppm", "_overlay.ppm", "_roi.pgm", "_snow.pgm"})
        std::filesystem::remove(stem + suffix);
    const auto folder = parent.empty() ? std::filesystem::path(".") : parent;
    const auto prefix = std::filesystem::path(stem).filename().string() + "_object_";
    for (const auto& entry : std::filesystem::directory_iterator(folder)) {
        const auto name = entry.path().filename().string();
        if (name.rfind(prefix, 0) == 0 &&
            (name.size() >= 8 &&
             (name.substr(name.size() - 8) == "_roi.pgm" ||
              (name.size() >= 9 && name.substr(name.size() - 9) == "_snow.pgm"))))
            std::filesystem::remove(entry.path());
    }
    {
        std::ofstream f(stem + "_result.json");
        f << json(r, decision) << '\n';
        if (!f)
            throw std::runtime_error("cannot write JSON");
    }
    if (!r.processed)
        return; // The host skipped this image, or this call failed.
    cv::Mat rgb;
    if (im.format == ImageFormat::NV12) {
        cv::Mat nv(im.height + im.height / 2, im.width, CV_8UC1, const_cast<uint8_t*>(im.data),
                   im.stride ? im.stride : im.width);
        cv::cvtColor(nv, rgb, cv::COLOR_YUV2RGB_NV12);
    } else {
        cv::Mat input(im.height, im.width, CV_8UC3, const_cast<uint8_t*>(im.data),
                      im.stride ? im.stride : im.width * 3);
        if (im.format == ImageFormat::BGR)
            cv::cvtColor(input, rgb, cv::COLOR_BGR2RGB);
        else
            rgb = input;
    }
    save(stem + "_original.ppm", rgb);
    auto overlay = rgb.clone();
    for (auto& m : masks) {
        if (m.timestamp_ms != im.timestamp_ms)
            continue;
        cv::Mat surface(m.height, m.width, CV_8UC1, m.surface.data());
        const std::string prefix = stem + "_object_" + std::to_string(m.winch_id);
        save(prefix + "_roi.pgm", surface);
        if (!m.snow.empty()) {
            cv::Mat snow(m.height, m.width, CV_8UC1, m.snow.data());
            save(prefix + "_snow.pgm", snow);
            for (int y = 0; y < m.height; ++y)
                for (int x = 0; x < m.width; ++x)
                    if (snow.at<uint8_t>(y, x)) {
                        auto& p = overlay.at<cv::Vec3b>(y, x);
                        p = cv::Vec3b(p[0] / 2, uint8_t(p[1] / 2 + 127), p[2] / 2);
                    }
            if (r.objects.size() == 1)
                save(stem + "_snow.pgm", snow);
        }
        if (r.objects.size() == 1)
            save(stem + "_roi.pgm", surface);
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(surface.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        cv::drawContours(overlay, contours, -1, cv::Scalar(255, 70, 30), 2);
    }
    for (const auto& object : r.objects) {
        const auto& b = object.box;
        if (!b.width || !b.height)
            continue;
        cv::rectangle(overlay, cv::Rect(b.x, b.y, b.width, b.height),
                      object.valid ? cv::Scalar(255, 70, 30) : cv::Scalar(255, 0, 255), 1);
        const auto label =
            std::to_string(object.winch_id) + ": " +
            (object.valid ? std::to_string(int(std::lround(object.ice_ratio * 100))) + "%"
                          : "invalid");
        cv::putText(overlay, label, cv::Point(b.x, std::max(12, int(b.y))),
                    cv::FONT_HERSHEY_SIMPLEX, .4, cv::Scalar(255, 70, 30), 1);
    }
    cv::putText(overlay, r.alert_level == AlertLevel::ALERT_WARN ? "ALARM" : "NO ALARM",
                cv::Point(5, overlay.rows - 5), cv::FONT_HERSHEY_SIMPLEX, .4,
                cv::Scalar(255, 70, 30), 1);
    save(stem + "_overlay.ppm", overlay);
}

inline uint32_t integer(const cv::FileNode& n, const std::string& name) {
    if (n.empty() || (!n.isInt() && !n.isReal()))
        throw std::runtime_error("missing/invalid " + name);
    const double v = static_cast<double>(n);
    if (!std::isfinite(v) || v < 0 || v > std::numeric_limits<uint32_t>::max() ||
        std::floor(v) != v)
        throw std::runtime_error("invalid " + name);
    return static_cast<uint32_t>(v);
}

inline void artifacts(evs::winch_ice::Detector& detector, const ImageData& im,
                      const WinchSnowResult& r, const std::string& stem,
                      HostDecision decision = HostDecision::Detect) {
    std::vector<evs::testing::IceMaskSnapshot> masks;
    evs::testing::IceReplayAccess::copyMasks(detector, masks);
    artifacts(im, r, stem, decision, std::move(masks));
}
} // namespace ice_demo

#include "winch_ice_plugin.h"
#include "evs/status.h"
#include "evs/testing/ice_replay.h"
#include "evs_register.h"
#include <algorithm>
#include <chrono>
#include <cmath>

namespace evs {
using namespace winch_ice;

namespace {
uint32_t ice_status(EvsStatus s) {
    return with_module(8, static_cast<uint32_t>(s));
}

#if defined(EVS_WITH_OPENCV)
float overlap(const Target& a, const Target& b) {
    const float w = std::max(0.f, std::min(a.x + a.w / 2, b.x + b.w / 2) -
                                      std::max(a.x - a.w / 2, b.x - b.w / 2));
    const float h = std::max(0.f, std::min(a.y + a.h / 2, b.y + b.h / 2) -
                                      std::max(a.y - a.h / 2, b.y - b.h / 2));
    const float intersection = w * h;
    return intersection / (a.w * a.h + b.w * b.h - intersection);
}

Detection pixel_box(const Target& t, int w, int h) {
    // Same rounded half-open endpoints define both the denominator and the overlay.
    const int left = std::clamp(int(std::lround((t.x - t.w / 2) * w)), 0, w);
    const int top = std::clamp(int(std::lround((t.y - t.h / 2) * h)), 0, h);
    const int right = std::clamp(int(std::lround((t.x + t.w / 2) * w)), 0, w);
    const int bottom = std::clamp(int(std::lround((t.y + t.h / 2) * h)), 0, h);
    return {uint32_t(std::max(0, t.class_id)),
            t.confidence,
            uint32_t(left),
            uint32_t(top),
            uint32_t(std::max(0, right - left)),
            uint32_t(std::max(0, bottom - top))};
}

bool intersects(const Detection& a, const Detection& b) {
    return a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height &&
           b.y < a.y + a.height;
}
#endif
} // namespace

uint32_t WinchIcePlugin::configure(const TaskConfig& cfg) {
    if (!std::isfinite(cfg.confidence_threshold) || cfg.confidence_threshold < 0 ||
        cfg.confidence_threshold > 1 || !std::isfinite(cfg.nms_threshold) ||
        cfg.nms_threshold < 0 || cfg.nms_threshold > 1 || !std::isfinite(cfg.area_ratio) ||
        cfg.area_ratio < 0 || cfg.area_ratio > 1 || cfg.ice.saturation_max < 0 ||
        cfg.ice.saturation_max > 255 || cfg.ice.value_min < 0 || cfg.ice.value_min > 255)
        return static_cast<uint32_t>(EvsStatus::OutOfRange);
#if !defined(EVS_WITH_OPENCV)
    return static_cast<uint32_t>(EvsStatus::NotSupported);
#else
    if (cfg.video_channel.size() != 1 || cfg.video_channel[0] < 0 ||
        cfg.video_channel[0] >= kCameraChannelUnused || cfg.ground.enabled ||
        !cfg.inference_enabled)
        return static_cast<uint32_t>(EvsStatus::ConfigInvalid);
    if (configured_ && cfg.video_channel != cfg_.video_channel)
        return static_cast<uint32_t>(EvsStatus::NotSupported);
    auto next = cfg; // Allocate before committing an update.
    using std::swap;
    swap(cfg_, next);
    configured_ = true;
    return 0;
#endif
}

void WinchIcePlugin::summarize() noexcept {
    bool alarm = false, valid = false;
    for (const auto& r : report_.objects) {
        valid |= r.valid;
        alarm |= r.valid && r.state == ObjectState::Alarm;
    }
    report_.alert_level = alarm ? AlertLevel::ALERT_WARN : AlertLevel::ALERT_NONE;
    report_.result_code = valid ? ResultCode::RESULT_MATCH : ResultCode::RESULT_NO_RESULT;
}

void WinchIcePlugin::publish(Result& out, uint32_t status) noexcept {
    summarize();
    report_.status = status;
    out.status = status;
    out.ice_ratio = report_.objects.size() == 1 && report_.objects[0].valid
                        ? report_.objects[0].ice_ratio
                        : kIceRatioUnknown;
    out.alarm =
        report_.alert_level == AlertLevel::ALERT_WARN ? AlarmState::Alarm : AlarmState::None;
    out.targets.clear();
    out.camera_channel = pack_camera_channel(static_cast<uint8_t>(report_.camera_channel));
}

void WinchIcePlugin::reject_call(uint32_t status, Result& out) noexcept {
    report_ = WinchSnowResult{};
    report_.camera_channel = configured_ ? uint32_t(cfg_.video_channel[0]) : 0xFFFFFFFFu;
#if defined(EVS_WITH_OPENCV)
    measurements_.clear();
#endif
    publish(out, status);
}

uint32_t WinchIcePlugin::preflight(const Frame& frame, const MachineContext&, Result& out,
                                   bool& run) {
    reject_call(0, out); // No cached result, even for repeated timestamps or rejected calls.
    run = false;
    if (!configured_) {
        publish(out, ice_status(EvsStatus::NotInitialized));
        return out.status;
    }
    if (frame.camera_id != cfg_.video_channel[0]) {
        publish(out, ice_status(EvsStatus::InvalidCameraIndex));
        return out.status;
    }
    run = true;
    return 0;
}

uint32_t WinchIcePlugin::decide(const Frame& frame, const MachineContext& ctx,
                                const std::vector<Target>& targets, Result& out) {
    bool run = false;
    if (const auto rc = preflight(frame, ctx, out, run))
        return rc;
#if !defined(EVS_WITH_OPENCV)
    (void)targets;
    publish(out, ice_status(EvsStatus::NotSupported));
    return out.status;
#else
    const auto start = std::chrono::steady_clock::now();
    auto fail = [&](EvsStatus status) {
        reject_call(ice_status(status), out);
        return out.status;
    };
    std::vector<Target> selected;
    for (const auto& t : targets) {
        if (!std::isfinite(t.x) || !std::isfinite(t.y) || !std::isfinite(t.w) ||
            !std::isfinite(t.h) || t.w <= 0 || t.h <= 0 || t.x - t.w / 2 < -1e-6f ||
            t.y - t.h / 2 < -1e-6f || t.x + t.w / 2 > 1.000001f || t.y + t.h / 2 > 1.000001f)
            return fail(EvsStatus::InvalidParameter);
        if (!std::isfinite(t.confidence) || t.confidence < 0 || t.confidence > 1)
            return fail(EvsStatus::ModelInvalidFormat);
        if (t.class_id == cfg_.person_class_id && t.confidence >= cfg_.confidence_threshold)
            selected.push_back(t);
    }
    std::sort(selected.begin(), selected.end(),
              [](const auto& a, const auto& b) { return a.confidence > b.confidence; });
    std::vector<Target> kept;
    for (const auto& t : selected) {
        if (std::none_of(kept.begin(), kept.end(),
                         [&](const auto& other) { return overlap(t, other) > cfg_.nms_threshold; }))
            kept.push_back(t);
        if (kept.size() > 2) {
            fail(EvsStatus::OutOfRange);
            report_.reason = Reason::UnexpectedObjectCount;
            return out.status;
        }
    }
    selected.swap(kept);
    std::sort(selected.begin(), selected.end(),
              [](const auto& a, const auto& b) { return a.x != b.x ? a.x < b.x : a.y < b.y; });
    IceFramePixels pixels;
    if (const auto rc = prepare_ice_pixels(frame, cfg_.ice, pixels)) {
        reject_call(with_module(8, rc), out);
        return out.status;
    }
    report_.processed = true;
    if (selected.empty()) {
        report_.reason = Reason::ObjectNotFound;
        publish(out);
        return 0;
    }
    const auto count = selected.size();
    report_.objects.resize(count);
    measurements_.resize(count);
    for (size_t i = 0; i < count; ++i) {
        auto& r = report_.objects[i];
        r.measurement_timestamp_ms = static_cast<uint32_t>(frame.timestamp_us / 1000);
        r.winch_id = uint32_t(i);
        r.box = pixel_box(selected[i], frame.width, frame.height);
        if (r.box.width < 3 || r.box.height < 3) {
            r.state = ObjectState::Failed;
            r.reason = Reason::InvalidSurface;
            r.status = ice_status(EvsStatus::InvalidImageSize);
            continue;
        }
        bool ambiguous = false;
        for (size_t j = 0; j < selected.size(); ++j)
            if (i != j && intersects(r.box, pixel_box(selected[j], frame.width, frame.height)))
                ambiguous = true;
        if (ambiguous) {
            r.state = ObjectState::Failed;
            r.reason = Reason::OverlappingBoxes;
            r.status = ice_status(EvsStatus::InvalidParameter);
            continue;
        }
        const cv::Rect rect(int(r.box.x), int(r.box.y), int(r.box.width), int(r.box.height));
        const auto rc = measure_ice_pixels(pixels, rect, measurements_[i]);
        if (rc == uint32_t(EvsStatus::MemoryAllocation) ||
            rc == uint32_t(EvsStatus::PostprocessFailed)) {
            reject_call(with_module(8, rc), out);
            return out.status;
        }
        r.surface_pixels = measurements_[i].surface_pixels;
        r.snow_pixels = measurements_[i].snow_pixels;
        r.status = with_module(8, rc);
        if (rc) {
            r.state = ObjectState::Failed;
            r.reason = rc == kIceUnobservable ? Reason::Unobservable : Reason::InvalidSurface;
        } else {
            r.valid = true;
            r.ice_ratio = measurements_[i].ratio;
            r.state = r.ice_ratio >= cfg_.area_ratio ? ObjectState::Alarm : ObjectState::Normal;
        }
    }
    report_.processing_time_ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    publish(out);
    return 0;
#endif
}

bool WinchIcePlugin::copy_ice_masks(std::vector<testing::IceMaskSnapshot>& out) const {
    out.clear();
#if defined(EVS_WITH_OPENCV)
    for (size_t i = 0; i < measurements_.size(); ++i) {
        const auto& m = measurements_[i];
        if (m.surface.empty())
            continue;
        testing::IceMaskSnapshot s;
        s.winch_id = report_.objects[i].winch_id;
        s.timestamp_ms = report_.objects[i].measurement_timestamp_ms;
        s.width = m.surface.cols;
        s.height = m.surface.rows;
        s.surface.assign(m.surface.datastart, m.surface.dataend);
        if (!m.snow.empty())
            s.snow.assign(m.snow.datastart, m.snow.dataend);
        out.push_back(std::move(s));
    }
    return true;
#else
    return false;
#endif
}

void WinchIcePlugin::reset() {
    configured_ = false;
    report_ = WinchSnowResult{};
#if defined(EVS_WITH_OPENCV)
    measurements_.clear();
#endif
}

namespace {
std::unique_ptr<IFunctionPlugin> make_winch_ice_plugin() {
    return std::make_unique<WinchIcePlugin>();
}

const FunctionPluginRegistration kWinchIce(0x0008, make_winch_ice_plugin);
} // namespace
} // namespace evs

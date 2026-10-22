#include "evs/testing/ice_replay.h"
#include "evs/vision_task.h"
#include "evs/winch_ice.h"
#include "ice_measurement.h"
#include <chrono>
#include <exception>
#include <limits>
#include <new>

namespace evs {
namespace winch_ice {
namespace {
uint32_t ice_status(EvsStatus s) {
    return with_module(0x0008, static_cast<uint32_t>(s));
}

TaskConfig make_task_config(const WinchSnowConfig& config) {
    TaskConfig task_config;
    task_config.task = "winch_ice";
    task_config.module_id = 0x0008;
    task_config.video_channel = {static_cast<int>(config.camera_channel)};
    task_config.ground.enabled = false;
    task_config.backend = config.model_config.backend;
    task_config.output_format = config.model_config.output_format;
    task_config.decoder_classes = config.model_config.num_classes;
    task_config.person_class_id = config.model_config.winch_class_id;
    task_config.input_format = config.model_config.input_format;
    task_config.letterbox = config.model_config.letterbox;
    task_config.Model_path = config.model_config.Model_path;
    task_config.input_width = config.model_config.input_width;
    task_config.input_height = config.model_config.input_height;
    task_config.warmup = config.model_config.warmup;
    task_config.area_ratio = config.detect_config.area_ratio;
    task_config.confidence_threshold = config.detect_config.confidence_threshold;
    task_config.nms_threshold = config.detect_config.nms_threshold;
    task_config.ice.saturation_max = config.detect_config.saturation_max;
    task_config.ice.value_min = config.detect_config.value_min;
    return task_config;
}

bool valid_public_config(const WinchSnowConfig& config) {
    return config.camera_channel < 255;
}

} // namespace

struct Detector::Impl {
    VisionTask task;
    WinchSnowConfig cfg;
    bool initialized = false;

    uint32_t reject(uint32_t status, WinchSnowResult& result) noexcept {
        Result generic;
        task.rejectWinchSnow(status, generic);
        result = WinchSnowResult{};
        result.camera_channel = cfg.camera_channel;
        result.status = status;
        return status;
    }

    uint32_t detect(const ImageData& image, WinchSnowResult& result) {
        if (!initialized)
            return reject(ice_status(EvsStatus::NotInitialized), result);
        Frame frame;
        if (const auto rc = detail::make_frame(image, cfg.camera_channel, frame))
            return reject(rc, result);
        MachineContext context;
        // The generic task interface still serves modules that need context.
        // Ice ignores it: the host has already decided to inspect this image.
        Result generic;
        const auto start = std::chrono::steady_clock::now();
        const auto previous_frames = task.debugStats().frames;
        const auto rc = task.detect(frame, context, generic);
        task.lastWinchSnowResult(result);
        result.status = rc;
        result.processing_time_ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                .count();
        if (task.debugStats().frames != previous_frames)
            result.inference_time_ms = static_cast<uint32_t>(task.debugStats().infer.last_ms + .5);
        return rc;
    }
};

Detector::Detector() : impl_(std::make_unique<Impl>()) {
}

Detector::~Detector() = default;

ErrorCode Detector::initialize(const WinchSnowConfig& config) {
    try {
        if (impl_->initialized)
            return 0u;
        if (!valid_public_config(config))
            return ice_status(EvsStatus::ConfigInvalid);
        if (const auto rc = impl_->task.initialize(make_task_config(config)))
            return rc;
        impl_->cfg = config;
        impl_->initialized = true;
        return 0u;
    } catch (const std::bad_alloc&) {
        release();
        return ice_status(EvsStatus::MemoryAllocation);
    }
}

ErrorCode Detector::detect(const ImageData& image, WinchSnowResult& result) {
    try {
        return impl_->detect(image, result);
    } catch (const std::bad_alloc&) {
        return impl_->reject(ice_status(EvsStatus::MemoryAllocation), result);
    } catch (const std::exception&) {
        return impl_->reject(ice_status(EvsStatus::PostprocessFailed), result);
    }
}

ErrorCode Detector::updateConfig(const DetectConfig& config) {
    if (!impl_->initialized)
        return ice_status(EvsStatus::NotInitialized);
    try {
        auto next = impl_->cfg;
        next.detect_config = config;
        const auto rc = impl_->task.updateConfig(make_task_config(next));
        if (!rc)
            impl_->cfg = std::move(next);
        return rc;
    } catch (const std::bad_alloc&) {
        return ice_status(EvsStatus::MemoryAllocation);
    }
}

void Detector::release() {
    impl_->task.release();
    impl_->initialized = false;
}

ErrorCode load_config(const std::string& path, WinchSnowConfig& out) {
    try {
        TaskConfig t;
        uint32_t rc = 0;
        if (!load_task_config(path, t, rc))
            return with_module(0x0008, rc);
        if (t.module_id != 8 || t.video_channel.size() != 1 || t.video_channel[0] < 0 ||
            t.ground.enabled)
            return ice_status(EvsStatus::ConfigInvalid);
        WinchSnowConfig c;
        c.camera_channel = static_cast<uint32_t>(t.video_channel[0]);
        c.detect_config.area_ratio = t.area_ratio;
        c.detect_config.saturation_max = t.ice.saturation_max;
        c.detect_config.value_min = t.ice.value_min;
        c.detect_config.confidence_threshold = t.confidence_threshold;
        c.detect_config.nms_threshold = t.nms_threshold;
        c.model_config.Model_path = t.Model_path;
        c.model_config.input_width = t.input_width;
        c.model_config.input_height = t.input_height;
        c.model_config.warmup = t.warmup;
        c.model_config.backend = t.backend;
        c.model_config.output_format = t.output_format;
        c.model_config.num_classes = t.decoder_classes;
        c.model_config.winch_class_id = t.person_class_id;
        c.model_config.input_format = t.input_format;
        c.model_config.letterbox = t.letterbox;
        if (!valid_public_config(c))
            return ice_status(EvsStatus::ConfigInvalid);
        out = std::move(c);
        return 0u;
    } catch (const std::bad_alloc&) {
        return ice_status(EvsStatus::MemoryAllocation);
    } catch (const std::exception&) {
        return ice_status(EvsStatus::ConfigParse);
    }
}
} // namespace winch_ice

namespace testing {
bool IceReplayAccess::copyMasks(const winch_ice::Detector& d, std::vector<IceMaskSnapshot>& out) {
    return d.impl_->task.copyIceMasks(out);
}
} // namespace testing
} // namespace evs

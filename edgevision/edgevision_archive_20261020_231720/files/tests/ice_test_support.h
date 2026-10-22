#pragma once
// Private pixel fixtures: synthetic detections enter the actual plugin directly.
// This header is never installed and does not add an alternate Detector mode.
#include "evs/testing/ice_replay.h"
#include "tasks/winch_ice/winch_ice_plugin.h"
#include <new>
#include <stdexcept>

namespace ice_test {
using namespace evs;
using namespace evs::winch_ice;

struct Box {
    uint32_t id, x, y, width, height;
};

class PixelDetector {
public:
    uint32_t initialize(const WinchSnowConfig& config) {
        config_ = TaskConfig{};
        config_.module_id = 8;
        config_.video_channel = {int(config.camera_channel)};
        config_.ground.enabled = false;
        config_.person_class_id = config.model_config.winch_class_id;
        return updateConfig(config.detect_config);
    }

    uint32_t updateConfig(const DetectConfig& config) {
        try {
            auto next = config_;
            next.confidence_threshold = config.confidence_threshold;
            next.nms_threshold = config.nms_threshold;
            next.area_ratio = config.area_ratio;
            next.ice.saturation_max = config.saturation_max;
            next.ice.value_min = config.value_min;
            const auto rc = plugin_.configure(next);
            if (rc)
                return with_module(8, rc);
            using std::swap;
            swap(config_, next);
            initialized_ = true;
            return 0;
        } catch (const std::bad_alloc&) {
            return evs::winch_ice::detail::ice_status(EvsStatus::MemoryAllocation);
        }
    }

    uint32_t detectWithBoxes(const ImageData& image, const std::vector<Box>& boxes,
                             WinchSnowResult& result) {
        Result out;
        uint32_t rc = 0;
        try {
            Frame frame;
            rc = initialized_ ? evs::winch_ice::detail::make_frame(
                                    image, uint32_t(config_.video_channel[0]), frame)
                              : evs::winch_ice::detail::ice_status(EvsStatus::NotInitialized);
            if (rc)
                return reject(rc, result);
            std::vector<Target> targets;
            for (const auto& box : boxes) {
                Target t;
                t.class_id = config_.person_class_id;
                t.confidence = 1.f;
                t.x = (double(box.x) + double(box.width) / 2) / image.width;
                t.y = (double(box.y) + double(box.height) / 2) / image.height;
                t.w = float(box.width) / image.width;
                t.h = float(box.height) / image.height;
                targets.push_back(t);
            }
            rc = plugin_.decide(frame, MachineContext{}, targets, out);
            result = *plugin_.winch_result();
            return rc;
        } catch (const std::bad_alloc&) {
            return reject(evs::winch_ice::detail::ice_status(EvsStatus::MemoryAllocation), result);
        } catch (const std::exception&) {
            return reject(evs::winch_ice::detail::ice_status(EvsStatus::PostprocessFailed), result);
        }
    }

    bool copyMasks(std::vector<testing::IceMaskSnapshot>& out) const {
        return plugin_.copy_ice_masks(out);
    }

    void release() {
        plugin_.reset();
        initialized_ = false;
    }

private:
    uint32_t reject(uint32_t status, WinchSnowResult& result) noexcept {
        Result out;
        plugin_.reject_call(status, out);
        result = WinchSnowResult{};
        result.status = status;
        result.camera_channel = initialized_ ? uint32_t(config_.video_channel[0]) : 0xFFFFFFFFu;
        return status;
    }

    WinchIcePlugin plugin_;
    TaskConfig config_;
    bool initialized_ = false;
};
} // namespace ice_test

#include "height_limit_plugin.h"

#include "evs/status.h"
#include "evs_register.h"
#include "height_limit_mount.h"

namespace evs {
namespace {

const Target* best_switch(const std::vector<Target>& targets) {
    const Target* best = nullptr;
    for (const Target& t : targets) {
        if (!best || t.confidence > best->confidence)
            best = &t;
    }
    return best;
}

} // namespace

uint32_t HeightLimitPlugin::configure(const TaskConfig& cfg) {
    if (!(cfg.stop_angle_deg > cfg.start_angle_deg))
        return static_cast<uint32_t>(EvsStatus::OutOfRange);
    if (cfg.min_frames < 1)
        return static_cast<uint32_t>(EvsStatus::OutOfRange);
    start_angle_deg_ = cfg.start_angle_deg;
    stop_angle_deg_ = cfg.stop_angle_deg;
    min_frames_ = cfg.min_frames;
    still_timeout_s_ = cfg.still_timeout_s;
    basic_boom_only_ = cfg.basic_boom_only;
    return 0u;
}

uint32_t HeightLimitPlugin::decide(const Frame& frame, const MachineContext& ctx,
                                   const std::vector<Target>& targets, Result& out) {
    (void)ctx;
    (void)basic_boom_only_;
    (void)still_timeout_s_;
    (void)min_frames_;
    (void)start_angle_deg_;
    (void)stop_angle_deg_;
    out.alarm = AlarmState::None;
    out.ice_ratio = 0.f;
    out.targets.clear();

    // Stage 1: the trained model. No box means the switch is not installed.
    const Target* sw = best_switch(targets);
    if (!sw) {
        out.limit_state = 1;
        return 0u;
    }
    out.targets.push_back(*sw);

    // Stage 2: OpenCV on the original image. 0 means the mount is not judged.
    out.limit_state = judge_switch_mount(frame, *sw);
    return 0u;
}

void HeightLimitPlugin::reset() { h_frames_ = 0; }

namespace {

std::unique_ptr<IFunctionPlugin> make_height_limit_plugin() {
    return std::make_unique<HeightLimitPlugin>();
}

const FunctionPluginRegistration kHeightLimit(0x0007, make_height_limit_plugin);

} // namespace

} // namespace evs

#pragma once
#include "evs/function_plugin.h"

namespace evs {

// 0x0007 height-limit switch.
// The trained detector says whether a switch is in the frame.
// judge_switch_mount() is the only place that says the mount is right or wrong.
class HeightLimitPlugin : public IFunctionPlugin {
public:
    uint16_t module_id() const override { return 0x0007; }
    uint32_t configure(const TaskConfig& cfg) override;
    uint32_t decide(const Frame& frame, const MachineContext& ctx,
                    const std::vector<Target>& targets, Result& out) override;
    void reset() override;

private:
    float start_angle_deg_ = 5.f;
    float stop_angle_deg_ = 75.f;
    int min_frames_ = 10;
    int still_timeout_s_ = 15;
    bool basic_boom_only_ = true;
    int h_frames_ = 0; // TODO: count frames inside the angle window
};

} // namespace evs

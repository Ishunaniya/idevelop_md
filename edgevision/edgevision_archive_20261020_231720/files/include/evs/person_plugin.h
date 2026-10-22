#pragma once
#include <vector>

#include "evs/frame.h"
#include "evs/function_plugin.h"
#include "evs/status.h"
#include "evs/task_config.h"

namespace evs {

// Static configuration for one of the three person plugins.
// The three modules (0x0004 / 0x0005 / 0x0006) share this same decision engine
// and differ only in these fields plus the reference point used when decoding
// (hook ground projection for 0x0004/0x0005, boom root for 0x0006).
struct PersonPolicyConfig {
    enum class Zone { Circle, Sector };

    Zone zone = Zone::Circle;
    float radius_m = 10.f;       // fixed radius (load_near / load_under)
    bool dynamic_radius = false; // boom_under: radius taken from ctx.hoist_radius
    float min_radius_m = 50.f;   // clamp applied to the dynamic radius

    float sector_axis_deg = 0.f; // sector central clockwise angle
    float sector_half_deg = 0.f; // sector half-angle

    int consecutive_frames = 3;  // frames-in-zone before a stable alarm
    bool require_work = true;    // alarm only when 作业 (crane moving)
    float work_epsilon = 0.1f;   // |rate| threshold to count as 作业

    // Action when MachineContext is invalid:
    //   Alarm   (0x0004): a person in zone -> immediate alarm, no frame counting.
    //   LogOnly (0x0005/0x0006): a person in zone -> record only, never alarm.
    AlarmState missing_context = AlarmState::LogOnly;

    // load_under: exclude the load-occluded sector (cannot reliably see below the load).
    bool has_occlusion = false;
    float occ_axis_deg = 90.f;   // straight down in screen-clockwise coords
    float occ_half_deg = 25.f;
};

// Per-instance frame-counter state for the consecutive-frames rule.
struct PersonPolicyState {
    int consecutive = 0;
};

// True when the crane is 作业: context valid, and is_working is set or
// hoist / telescope is moving. epsilon is unused; kept so callers stay stable.
bool is_working(const MachineContext& ctx, float epsilon);

// Whether a decoded person (already in ground-relative polar coords relative to
// this plugin's reference point) falls inside the alert zone, accounting for the
// optional occlusion sector.
bool in_person_zone(const Target& t, const MachineContext& ctx, const PersonPolicyConfig& cfg);

// Core decision for the three person plugins.
// `persons` are detections decoded to (distance_m, angle_deg) relative to the
// plugin reference point (hook projection or boom root).
AlarmState person_decide(const std::vector<Target>& persons,
                         const MachineContext& ctx,
                         const PersonPolicyConfig& cfg,
                         PersonPolicyState& st);

// Build the per-module policy from a parsed TaskConfig.
// Returns false for non-person modules (0x0007 / 0x0008).
bool make_person_config(uint16_t module_id, const TaskConfig& cfg, PersonPolicyConfig& out);

// Decode the legacy "flat6" backend float tensor into person targets.
// Convention: each detection = [class_id, conf, x, y, w, h] in 0..1 image
// coords (6 floats). This is the single-class person-detector layout; it is now
// reached through Flat6Decoder. New models should be parsed by a dedicated
// decoder obtained from create_decoder() instead of calling this directly.
// Ground coordinates and task-relative metrics are produced by the shared
// mapping stages; they remain unknown here.
void decode_person(const std::vector<float>& output, float score_threshold,
                   std::vector<Target>& out_persons);

// 0x0004 / 0x0005 / 0x0006. decide() calls person_decide and copies targets.
class PersonPlugin : public IFunctionPlugin {
public:
    uint16_t module_id() const override { return module_id_; }
    uint32_t configure(const TaskConfig& cfg) override;
    bool ground_reference(const MachineContext& ctx,
                          GroundReference& out) const override;
    uint32_t decide(const Frame& frame, const MachineContext& ctx,
                    const std::vector<Target>& targets, Result& out) override;
    void reset() override;

private:
    uint16_t module_id_ = 0;
    bool use_task_reference_ = false;
    PersonPolicyConfig cfg_{};
    PersonPolicyState state_{};
};

} // namespace evs

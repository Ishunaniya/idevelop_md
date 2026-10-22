#include "evs/person_plugin.h"

#include "evs_register.h"

#include <algorithm>
#include <cmath>

namespace evs {

bool is_working(const MachineContext& ctx, float epsilon) {
    (void)epsilon;
    if (!ctx.context_valid) return false;
    if (ctx.is_working != 0) return true;
    return ctx.hoist_moving || ctx.telescope_moving;
}

bool in_person_zone(const Target& t, const MachineContext& ctx, const PersonPolicyConfig& cfg) {
    // No ground fix: not inside the zone, so it cannot raise Alarm.
    if (t.distance_m < 0.f) return false;
    const float radius = cfg.dynamic_radius
                             ? std::max(cfg.min_radius_m, ctx.hoist_radius)
                             : cfg.radius_m;
    if (t.distance_m > radius) return false;

    if (cfg.zone == PersonPolicyConfig::Zone::Sector) {
        float diff = std::fabs(t.angle_deg - cfg.sector_axis_deg);
        if (diff > 180.f) diff = 360.f - diff;
        if (diff > cfg.sector_half_deg) return false;
    }

    if (cfg.has_occlusion) {
        float diff = std::fabs(t.angle_deg - cfg.occ_axis_deg);
        if (diff > 180.f) diff = 360.f - diff;
        if (diff <= cfg.occ_half_deg) return false; // inside occlusion sector -> drop
    }
    return true;
}

namespace {

struct ZoneScan {
    bool in_zone = false;
    bool unknown = false;
};

ZoneScan scan_persons(const std::vector<Target>& persons, const MachineContext& ctx,
                      const PersonPolicyConfig& cfg) {
    ZoneScan s;
    for (const auto& t : persons) {
        if (t.distance_m < 0.f) {
            s.unknown = true;
            continue;
        }
        if (in_person_zone(t, ctx, cfg)) s.in_zone = true;
    }
    return s;
}

} // namespace

AlarmState person_decide(const std::vector<Target>& persons,
                         const MachineContext& ctx,
                         const PersonPolicyConfig& cfg,
                         PersonPolicyState& st) {
    const ZoneScan seen = scan_persons(persons, ctx, cfg);

    // 1) No valid context.
    if (!ctx.context_valid) {
        if (seen.in_zone && cfg.missing_context == AlarmState::Alarm)
            return AlarmState::Alarm; // 载荷附近：区内有人，立即报警
        if (seen.in_zone || seen.unknown) return AlarmState::LogOnly;
        return AlarmState::None;
    }

    // 2) Context valid but crane idle -> 静止只记录，不报警。
    if (cfg.require_work && !is_working(ctx, cfg.work_epsilon)) {
        st.consecutive = 0;
        if (seen.in_zone || seen.unknown) return AlarmState::LogOnly;
        return AlarmState::None;
    }

    // 3) Working: only a mapped in-zone person counts. An unmapped box is recorded
    // and does not advance or clear the streak.
    if (seen.in_zone) {
        st.consecutive += 1;
        if (st.consecutive >= cfg.consecutive_frames) return AlarmState::Alarm;
        return AlarmState::LogOnly;
    }
    if (seen.unknown) return AlarmState::LogOnly;
    st.consecutive = 0;
    return AlarmState::None;
}

bool make_person_config(uint16_t module_id, const TaskConfig& cfg, PersonPolicyConfig& out) {
    const int consec = (cfg.zone.consecutive_frames > 0.f)
                          ? static_cast<int>(cfg.zone.consecutive_frames)
                          : 3;
    switch (module_id) {
        case 0x0004: // 载荷附近：吊钩投影为圆心、半径 10 m
            out.zone = PersonPolicyConfig::Zone::Circle;
            out.radius_m = (cfg.zone.radius_m > 0.f) ? cfg.zone.radius_m : 10.f;
            out.consecutive_frames = consec;
            out.require_work = true;
            out.missing_context =
                (cfg.on_missing_context == "alarm") ? AlarmState::Alarm : AlarmState::LogOnly;
            out.has_occlusion = false;
            break;
        case 0x0005: // 载荷下方：10 m 圆，排除载荷遮挡扇区
            out.zone = PersonPolicyConfig::Zone::Circle;
            out.radius_m = (cfg.zone.radius_m > 0.f) ? cfg.zone.radius_m : 10.f;
            out.consecutive_frames = consec;
            out.require_work = true;
            out.missing_context = AlarmState::LogOnly; // 默认只记录，不套用立即报警
            out.has_occlusion = cfg.zone.has_occlusion;
            out.occ_axis_deg = cfg.zone.occ_axis_deg;
            out.occ_half_deg = cfg.zone.occ_half_deg;
            break;
        case 0x0006: // 大臂下方：扇形，半径取 hoist_radius(≥50)，轴线沿大臂
            out.zone = PersonPolicyConfig::Zone::Sector;
            out.dynamic_radius = true;
            out.min_radius_m = 50.f;
            out.sector_axis_deg = 0.f; // 暂按车体/局部 +Y（前方）；待接入绝对回转角
            out.sector_half_deg = (cfg.zone.half_angle_deg > 0.f) ? cfg.zone.half_angle_deg : 40.f;
            out.consecutive_frames = consec;
            out.require_work = true;
            out.missing_context = AlarmState::LogOnly;
            break;
        default:
            return false; // not a person plugin
    }
    out.work_epsilon = (cfg.work_motion_epsilon > 0.f) ? cfg.work_motion_epsilon : 0.1f;
    return true;
}

uint32_t PersonPlugin::configure(const TaskConfig& cfg) {
    PersonPolicyConfig next;
    if (!make_person_config(cfg.module_id, cfg, next))
        return static_cast<uint32_t>(EvsStatus::NotSupported);
    cfg_ = next;
    module_id_ = cfg.module_id;
    use_task_reference_ = cfg.ground.use_task_reference;
    return 0u;
}

bool PersonPlugin::ground_reference(const MachineContext& ctx,
                                    GroundReference& out) const {
    out = GroundReference{};
    out.valid = true;
    if (!use_task_reference_) {
        // Legacy tasks measured from the homography origin.
        return true;
    }

    switch (module_id_) {
        case 0x0004:
        case 0x0005:
            if (!ctx.context_valid) {
                // Preserve the configured missing-context policy: without a
                // hook fix, use the homography origin so 0x0004 can still
                // apply its "person in zone -> Alarm" rule.
                out.origin_m = Point2D{0.f, 0.f};
                return true;
            }
            // MachineContext currently has radius but no absolute slew angle.
            // Until the host supplies it, define the hook projection on local +Y.
            out.origin_m = Point2D{0.f, ctx.hoist_radius};
            return true;
        case 0x0006:
            // Boom-root frame origin.
            out.origin_m = Point2D{0.f, 0.f};
            return true;
        default:
            return false;
    }
}

uint32_t PersonPlugin::decide(const Frame& frame, const MachineContext& ctx,
                              const std::vector<Target>& targets, Result& out) {
    (void)frame;
    out.alarm = person_decide(targets, ctx, cfg_, state_);
    out.targets = targets;
    out.limit_state = 0;
    out.ice_ratio = 0.f;
    return 0u;
}

void PersonPlugin::reset() { state_ = PersonPolicyState{}; }

namespace {

std::unique_ptr<IFunctionPlugin> make_person_plugin() {
    return std::make_unique<PersonPlugin>();
}

const FunctionPluginRegistration kPersonNear(0x0004, make_person_plugin);
const FunctionPluginRegistration kPersonUnder(0x0005, make_person_plugin);
const FunctionPluginRegistration kPersonBoom(0x0006, make_person_plugin);

} // namespace

} // namespace evs

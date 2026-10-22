#include "evs/ground_metrics.h"

#include <cmath>

namespace evs {
namespace {

constexpr float kPi = 3.14159265358979323846f;

float signed_ground_bearing_deg(const GroundReference& ref,
                                const Point2D& target) {
    const float dx = target.x - ref.origin_m.x;
    const float dy = target.y - ref.origin_m.y;
    float angle = std::atan2(dx, dy) * 180.f / kPi - ref.yaw_deg;
    while (angle <= -180.f) angle += 360.f;
    while (angle > 180.f) angle -= 360.f;
    return angle;
}

} // namespace

float ground_bearing_deg(const GroundReference& ref, const Point2D& target) {
    const float dx = target.x - ref.origin_m.x;
    const float dy = target.y - ref.origin_m.y;
    float angle = std::atan2(dx, dy) * 180.f / kPi - ref.yaw_deg;
    while (angle < 0.f) angle += 360.f;
    while (angle >= 360.f) angle -= 360.f;
    return angle;
}

void clear_ground_metrics(std::vector<Target>& targets) {
    for (auto& target : targets) {
        target.distance_m = -1.f;
        target.angle_deg = 0.f;
    }
}

void apply_ground_metrics(const GroundReference& ref, DistanceMode mode,
                          std::vector<Target>& targets) {
    if (!ref.valid) {
        clear_ground_metrics(targets);
        return;
    }

    for (auto& target : targets) {
        if (!target.ground_ok) {
            target.distance_m = -1.f;
            target.angle_deg = 0.f;
            continue;
        }
        const Point2D ground{target.ground_x_m, target.ground_y_m};
        const float dx = ground.x - ref.origin_m.x;
        const float dy = ground.y - ref.origin_m.y;
        target.distance_m = mode == DistanceMode::LegacyForwardY
                                ? dy
                                : std::sqrt(dx * dx + dy * dy);
        target.angle_deg = mode == DistanceMode::LegacyForwardY
                               ? signed_ground_bearing_deg(ref, ground)
                               : ground_bearing_deg(ref, ground);
    }
}

} // namespace evs

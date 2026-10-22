#pragma once

#include <vector>

#include "evs/geom.h"
#include "evs/ground_config.h"
#include "evs/status.h"

namespace evs {

struct GroundReference {
    bool valid = false;
    Point2D origin_m{};
    float yaw_deg = 0.f;
};

// +Y is forward, +X is right. Bearing is clockwise from +Y after applying
// reference yaw, matching the legacy atan2(X, Y) convention.
float ground_bearing_deg(const GroundReference& ref, const Point2D& target);

void apply_ground_metrics(const GroundReference& ref, DistanceMode mode,
                          std::vector<Target>& targets);

void clear_ground_metrics(std::vector<Target>& targets);

} // namespace evs

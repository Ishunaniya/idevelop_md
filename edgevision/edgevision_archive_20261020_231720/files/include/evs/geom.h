#pragma once
#include <cmath>

namespace evs {

struct Point2D { float x = 0.f; float y = 0.f; };

inline float distance_m(const Point2D& a, const Point2D& b) {
    float dx = a.x - b.x, dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

// Clockwise angle (degrees, 0..360) of vector p-origin relative to +x axis.
inline float clockwise_angle_deg(const Point2D& origin, const Point2D& p) {
    float dx = p.x - origin.x, dy = p.y - origin.y;
    // With screen y-down, rotating +x -> +y is clockwise, so the clockwise
    // angle is atan2(dy, dx): straight down becomes 90 deg, matching the
    // occ_axis_deg = 90 convention used by load_under. (atan2(-dy, dx) was wrong.)
    float ang = std::atan2(dy, dx) * 180.f / 3.14159265f;
    if (ang < 0.f) ang += 360.f;
    return ang;
}

inline bool in_circle(const Point2D& p, const Point2D& center, float r) {
    return distance_m(p, center) <= r;
}

// Inside a sector around `origin`: distance <= r AND |angle - axis| <= half.
inline bool in_sector(const Point2D& p, const Point2D& origin,
                      float axis_deg, float half_deg, float r) {
    if (distance_m(p, origin) > r) return false;
    float a = clockwise_angle_deg(origin, p);
    float diff = std::fabs(a - axis_deg);
    if (diff > 180.f) diff = 360.f - diff;
    return diff <= half_deg;
}

} // namespace evs

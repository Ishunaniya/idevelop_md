#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

// Example host policy. This is deliberately outside the library API.
namespace ice_demo {
struct HostWorkingCondition {
    float boom_angle = 0.f;
    int32_t is_working = 0;
    bool valid = false;
    uint32_t timestamp_ms = 0;
};

enum class HostDecision { Detect, AngleNotZero, NotWorking, InvalidContext, StaleContext };

inline const char* host_reason(HostDecision decision) {
    switch (decision) {
    case HostDecision::Detect:
        return "eligible";
    case HostDecision::AngleNotZero:
        return "angle_not_zero";
    case HostDecision::NotWorking:
        return "not_working";
    case HostDecision::InvalidContext:
        return "invalid_context";
    case HostDecision::StaleContext:
        return "stale_context";
    }
    return "invalid_context";
}

inline HostDecision host_decision(const HostWorkingCondition& context, uint32_t image_ms,
                                  uint32_t now_ms, uint32_t max_age_ms = 500) {
    if (!context.valid || !std::isfinite(context.boom_angle) || context.boom_angle < 0.f ||
        context.boom_angle > 90.f || (context.is_working != 0 && context.is_working != 1))
        return HostDecision::InvalidContext;
    const uint32_t delta = image_ms - context.timestamp_ms;
    if (max_age_ms == 0 || max_age_ms >= 0x80000000u || now_ms - image_ms > max_age_ms ||
        now_ms - context.timestamp_ms > max_age_ms ||
        std::min(delta, uint32_t(0) - delta) > max_age_ms)
        return HostDecision::StaleContext;
    if (context.boom_angle != 0.f)
        return HostDecision::AngleNotZero;
    if (context.is_working != 1)
        return HostDecision::NotWorking;
    return HostDecision::Detect;
}
} // namespace ice_demo

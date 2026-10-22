#pragma once

#include "evs/frame.h"
#include "evs/status.h"

namespace evs {

// Judge how the detected switch is mounted.
// 3 correct, 2 wrong, 0 the image cannot be judged yet.
// Only this function may decide 2 or 3. Presence of the switch comes from
// the trained detector, not from here.
int judge_switch_mount(const Frame& frame, const Target& box);

} // namespace evs

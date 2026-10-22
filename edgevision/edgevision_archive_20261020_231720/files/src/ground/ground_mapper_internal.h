#pragma once

#include <memory>

#include "evs/ground_mapper.h"

namespace evs {

std::unique_ptr<IGroundMapper> make_homography_mapper();
std::unique_ptr<IGroundMapper> make_crane_pose_mapper();

} // namespace evs

#pragma once

#include <string>
#include <vector>

#include "evs/ground_config.h"

namespace evs {

bool has_ground_param(const CameraGroundConfig& cfg, const std::string& key);

std::string ground_param_string(const CameraGroundConfig& cfg,
                                const std::string& key,
                                const std::string& fallback = {});

int ground_param_int(const CameraGroundConfig& cfg, const std::string& key,
                     int fallback);

float ground_param_float(const CameraGroundConfig& cfg, const std::string& key,
                         float fallback);

bool ground_param_bool(const CameraGroundConfig& cfg, const std::string& key,
                       bool fallback);

// Accepts "[1, 2, 3]" and "1,2,3". Invalid elements make the result empty.
std::vector<float> ground_param_float_array(const CameraGroundConfig& cfg,
                                            const std::string& key);

} // namespace evs

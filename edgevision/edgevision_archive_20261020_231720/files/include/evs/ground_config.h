#pragma once

#include <map>
#include <string>
#include <vector>

namespace evs {

// How a mapped ground point is converted to Target::distance_m.
enum class DistanceMode {
    LegacyForwardY = 0, // signed forward component; preserves the old Wy/100 behaviour
    Radial = 1,         // Euclidean distance from the task reference point
};

struct CameraMountConfig {
    bool enabled = false;
    float x_m = 0.f;
    float y_m = 0.f;
    float yaw_deg = 0.f;
    bool on_boom = false;
    float offset_m = 0.f;
};

struct CameraGroundConfig {
    int id = -1;
    std::string mapper = "homography";
    std::string matrix_path;
    int calibration_width = 1280;
    int calibration_height = 720;
    CameraMountConfig mount;
    // Mapper-specific values are kept raw and interpreted by configure().
    // This lets new algorithms add parameters without changing TaskConfig.
    std::map<std::string, std::string> params;
};

struct GroundConfig {
    bool enabled = true;
    bool explicitly_configured = false;
    bool use_task_reference = false;
    DistanceMode distance_mode = DistanceMode::LegacyForwardY;
    std::vector<CameraGroundConfig> cameras;
};

} // namespace evs

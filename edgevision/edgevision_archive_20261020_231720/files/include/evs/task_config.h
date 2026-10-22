#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "evs/debug.h"
#include "evs/ground_config.h"

namespace evs {

// Pixel thresholds for snow candidates inside model-detected rectangles.
struct IceConfig {
    int saturation_max = 80; // OpenCV uint8 HSV, S/V in 0..255
    int value_min = 180;
};

// Zone description shared by person functions.
struct ZoneConfig {
    std::string type = "circle"; // circle | sector
    float radius_m = 0.f;
    float consecutive_frames = 3;
    float half_angle_deg = 0.f; // for sector

    // Load-under (0x0005) only: exclude the load-occluded sector (cannot reliably
    // see the ground directly below the load). A person inside this sector is
    // dropped from the alert zone.
    bool has_occlusion = false;
    float occ_axis_deg = 90.f;   // sector central clockwise angle (screen y-down: 90 = straight down)
    float occ_half_deg = 25.f;   // sector half-angle
};

// Per-task configuration (parsed from YAML). Fields mirror the design doc config section.
struct TaskConfig {
    std::string task;
    uint16_t module_id = 0;
    std::vector<int> video_channel; // camera.video_channel; at most 4 ids

    std::string backend;   // mock | rknn | qnn | x86 | onnx
    std::string Model_path;

    // Model-output decoding. Decouples the detection policy from a fixed tensor
    // layout: the backend float tensor is parsed by a pluggable decoder selected
    // by `output_format`, so swapping the model does not touch person_plugin.
    std::string output_format = "flat6"; // flat6 | yolov8 | yolo11 | yolox
    int person_class_id = -1;            // yolov8: keep only this class; -1 = keep all
    int decoder_classes = 80;           // yolov8 row width = 4 + decoder_classes

    int input_width = 0;
    int input_height = 0;
    std::string input_format = "rgb"; // rgb | bgr (model input color order)
    bool letterbox = true;            // preserve aspect ratio (gray pad) vs stretch
    bool warmup = false;
    int init_timeout_s = 60;
    int npu_core_mask = 1;

    int latency_budget_ms = 200;
    int min_fps = 5;

    std::string on_missing_context = "log_only"; // alarm | log_only
    float work_motion_epsilon = 0.1f;

    ZoneConfig zone;
    float confidence_threshold = 0.45f;
    float nms_threshold = 0.5f;

    // Camera image size from camera.image_width / image_height. 0 = unset.
    int image_width = 0;
    int image_height = 0;
    bool canary_enabled = false;

    // Pixel-to-ground mapping. When no ground section is present, the loader
    // supplies the legacy camera 0..3 homography configuration.
    GroundConfig ground;

    // Debug behaviour is hot-updatable (no reload needed).
    DebugConfig debug;

    // height-limit extras (module 0x0007)
    float start_angle_deg = 5.f;
    float stop_angle_deg = 75.f;
    int min_frames = 10;
    int still_timeout_s = 15;
    bool basic_boom_only = true;

    // winch-ice extras (module 0x0008)
    float area_ratio = 0.5f;
    bool exit_on_alarm = true;
    bool inference_enabled = true; // false is rejected; ice always requires its model
    IceConfig ice;
};

// Minimal YAML loader: scalars, one-level nested maps, inline int arrays.
// Overlays fields present in the file; publishes cfg only on success.
// On failure cfg is unchanged and err is a bare EvsStatus (no module stamp).
bool load_task_config(std::string_view path, TaskConfig& cfg, uint32_t& err);

} // namespace evs

#include "evs/task_config.h"
#include "evs/status.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace evs {

namespace {

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

// Strip a trailing inline comment (" # ...").
std::string strip_comment(const std::string& s) {
    bool in_str = false;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '\'' || c == '"') in_str = !in_str;
        else if (c == '#' && !in_str) return s.substr(0, i);
    }
    return s;
}

int indent_of(const std::string& s) {
    int n = 0;
    while (n < static_cast<int>(s.size()) && (s[n] == ' ' || s[n] == '\t')) ++n;
    return n;
}

// Parse "key: value" -> returns true if a key is found, sets key/value.
bool split_kv(const std::string& line, std::string& key, std::string& value) {
    auto colon = line.find(':');
    if (colon == std::string::npos) return false;
    key = trim(line.substr(0, colon));
    value = trim(line.substr(colon + 1));
    if (key.empty()) return false;
    // quoted values
    if (value.size() >= 2 && (value.front() == '"' || value.front() == '\'') &&
        value.back() == value.front()) {
        value = value.substr(1, value.size() - 2);
    }
    return true;
}

std::vector<int> parse_int_array(const std::string& v) {
    std::vector<int> out;
    std::string s = v;
    if (!s.empty() && s.front() == '[') s = s.substr(1);
    if (!s.empty() && s.back() == ']') s = s.substr(0, s.size() - 1);
    std::istringstream iss(s);
    std::string tok;
    while (std::getline(iss, tok, ',')) {
        tok = trim(tok);
        if (tok.empty()) continue;
        out.push_back(static_cast<int>(std::strtol(tok.c_str(), nullptr, 10)));
    }
    return out;
}

float to_float(const std::string& v, float def) {
    if (v.empty()) return def;
    return static_cast<float>(std::strtod(v.c_str(), nullptr));
}

int to_int(const std::string& v, int def) {
    if (v.empty()) return def;
    return static_cast<int>(std::strtol(v.c_str(), nullptr, 10));
}

bool to_bool(const std::string& v, bool def) {
    if (v == "true" || v == "True" || v == "1") return true;
    if (v == "false" || v == "False" || v == "0") return false;
    return def;
}

void add_legacy_ground_defaults(GroundConfig& ground) {
    ground.enabled = true;
    ground.explicitly_configured = false;
    ground.use_task_reference = false;
    ground.distance_mode = DistanceMode::LegacyForwardY;
    ground.cameras.clear();
    for (int id = 0; id < 4; ++id) {
        CameraGroundConfig camera;
        camera.id = id;
        camera.mapper = "homography";
        camera.matrix_path =
            "/userdata/matrix_" + std::to_string(id) + "_1280x720.txt";
        camera.calibration_width = 1280;
        camera.calibration_height = 720;
        ground.cameras.push_back(camera);
    }
}

} // anonymous namespace

bool load_task_config(std::string_view path, TaskConfig& cfg, uint32_t& err) {
    err = 0;
    const std::string cfg_path(path);
    std::ifstream in(cfg_path);
    if (!in) { err = static_cast<uint32_t>(EvsStatus::ConfigInvalid); return false; }

    struct Node {
        int indent;
        std::string key;
        std::string value;
        bool list_start;
    };
    std::vector<Node> nodes;
    std::string raw;
    while (std::getline(in, raw)) {
        std::string line = strip_comment(raw);
        if (trim(line).empty()) continue;
        int ind = indent_of(line);
        std::string text = trim(line);
        const bool list_start = text.size() >= 2 && text[0] == '-' &&
                                std::isspace(static_cast<unsigned char>(text[1]));
        if (list_start) text = trim(text.substr(1));
        std::string k, v;
        if (!split_kv(text, k, v)) continue;
        nodes.push_back({ind, k, v, list_start});
    }

    // Parse the only supported list-of-maps section explicitly. The rest of
    // this loader intentionally remains a small scalar/one-level YAML subset.
    bool saw_ground = false;
    int ground_indent = -1;
    int cameras_indent = -1;
    int mount_indent = -1;
    int params_indent = -1;
    CameraGroundConfig* current_camera = nullptr;
    bool in_cameras = false;
    bool in_mount = false;
    bool in_params = false;
    bool ground_parse_error = false;
    for (const Node& n : nodes) {
        if (!saw_ground) {
            if (!n.list_start && n.key == "ground") {
                saw_ground = true;
                ground_indent = n.indent;
                cfg.ground = GroundConfig{};
                cfg.ground.explicitly_configured = true;
                cfg.ground.cameras.clear();
            }
            continue;
        }
        if (n.indent <= ground_indent) break;

        if (in_cameras && n.indent <= cameras_indent) {
            in_cameras = false;
            current_camera = nullptr;
            in_mount = false;
            in_params = false;
        }
        if (!in_cameras) {
            if (n.key == "enabled")
                cfg.ground.enabled = to_bool(n.value, true);
            else if (n.key == "reference") {
                if (n.value == "task" || n.value == "task_reference")
                    cfg.ground.use_task_reference = true;
                else if (n.value == "camera_origin")
                    cfg.ground.use_task_reference = false;
                else
                    ground_parse_error = true;
            } else if (n.key == "distance_mode") {
                if (n.value == "radial")
                    cfg.ground.distance_mode = DistanceMode::Radial;
                else if (n.value == "legacy_forward_y" || n.value == "legacy")
                    cfg.ground.distance_mode = DistanceMode::LegacyForwardY;
                else
                    ground_parse_error = true;
            }
            else if (n.key == "cameras") {
                in_cameras = true;
                cameras_indent = n.indent;
            }
            continue;
        }

        if (n.list_start && n.key == "id") {
            cfg.ground.cameras.emplace_back();
            current_camera = &cfg.ground.cameras.back();
            current_camera->id = to_int(n.value, -1);
            in_mount = false;
            in_params = false;
            continue;
        }
        if (!current_camera) continue;
        if (in_mount && n.indent <= mount_indent) in_mount = false;
        if (in_params && n.indent <= params_indent) in_params = false;
        if (n.key == "mount") {
            current_camera->mount.enabled = true;
            in_mount = true;
            mount_indent = n.indent;
        } else if (n.key == "params") {
            in_params = true;
            params_indent = n.indent;
        } else if (in_mount) {
            if (n.key == "x_m") current_camera->mount.x_m = to_float(n.value, 0.f);
            else if (n.key == "y_m") current_camera->mount.y_m = to_float(n.value, 0.f);
            else if (n.key == "yaw_deg")
                current_camera->mount.yaw_deg = to_float(n.value, 0.f);
            else if (n.key == "on_boom")
                current_camera->mount.on_boom = to_bool(n.value, false);
            else if (n.key == "offset_m")
                current_camera->mount.offset_m = to_float(n.value, 0.f);
        } else if (in_params) {
            current_camera->params[n.key] = n.value;
        } else if (n.key == "mapper") {
            current_camera->mapper = n.value;
        } else if (n.key == "matrix" || n.key == "matrix_path") {
            current_camera->matrix_path = n.value;
            current_camera->params["matrix"] = n.value; // marks an explicit path
        } else if (n.key == "calibration_width") {
            current_camera->calibration_width = to_int(n.value, 1280);
        } else if (n.key == "calibration_height") {
            current_camera->calibration_height = to_int(n.value, 720);
        }
    }
    if (!saw_ground) add_legacy_ground_defaults(cfg.ground);
    if (cfg.ground.use_task_reference &&
        cfg.ground.distance_mode != DistanceMode::Radial) {
        ground_parse_error = true;
    }

    const std::string* cur_nested = nullptr; // "zone" or "thresholds"
    int nested_indent = 0;
    for (size_t i = 0; i < nodes.size(); ++i) {
        const Node& n = nodes[i];
        if (n.list_start) continue;
        // Leave the nested block only when we drop back to the parent indent.
        // Comparing against a fixed 2 spaces wrongly evicted 2-space children.
        if (cur_nested && n.indent <= nested_indent) cur_nested = nullptr;

        if (n.key == "task") cfg.task = n.value;
        else if (n.key == "module_id") {
            // accept "0x0004" or decimal
            if (n.value.size() > 2 && n.value.substr(0, 2) == "0x")
                cfg.module_id = static_cast<uint16_t>(std::strtoul(n.value.c_str(), nullptr, 16));
            else
                cfg.module_id = static_cast<uint16_t>(to_int(n.value, 0));
        }
        else if (n.key == "camera_ids" || n.key == "video_channel")
            cfg.video_channel = parse_int_array(n.value);
        else if (n.key == "backend") cfg.backend = n.value;
        else if (n.key == "model" || n.key == "Model_path") cfg.Model_path = n.value;
        else if (n.key == "confidence_threshold")
            cfg.confidence_threshold = to_float(n.value, 0.45f);
        else if (n.key == "score_threshold")
            cfg.confidence_threshold = to_float(n.value, 0.45f);
        else if (n.key == "nms_threshold") cfg.nms_threshold = to_float(n.value, 0.5f);
        else if (n.key == "canary_enabled") cfg.canary_enabled = to_bool(n.value, false);
        else if (n.key == "output_format") cfg.output_format = n.value;
        else if (n.key == "person_class_id") cfg.person_class_id = to_int(n.value, -1);
        else if (n.key == "decoder_classes") cfg.decoder_classes = to_int(n.value, 80);
        else if (n.key == "input_width") cfg.input_width = to_int(n.value, 0);
        else if (n.key == "input_height") cfg.input_height = to_int(n.value, 0);
        else if (n.key == "input_format") cfg.input_format = n.value; // rgb | bgr
        else if (n.key == "letterbox") cfg.letterbox = to_bool(n.value, true);
        else if (n.key == "warmup") cfg.warmup = to_bool(n.value, false);
        else if (n.key == "init_timeout_s") cfg.init_timeout_s = to_int(n.value, 60);
        else if (n.key == "npu_core_mask") cfg.npu_core_mask = to_int(n.value, 1);
        else if (n.key == "latency_budget_ms") cfg.latency_budget_ms = to_int(n.value, 200);
        else if (n.key == "min_fps") cfg.min_fps = to_int(n.value, 5);
        else if (n.key == "on_missing_context") cfg.on_missing_context = n.value;
        else if (n.key == "work_motion_epsilon") cfg.work_motion_epsilon = to_float(n.value, 0.1f);
        else if (n.key == "zone") { cur_nested = &n.key; nested_indent = n.indent; }
        else if (n.key == "thresholds") { cur_nested = &n.key; nested_indent = n.indent; }
        else if (n.key == "debug") { cur_nested = &n.key; nested_indent = n.indent; }
        else if (n.key == "camera") { cur_nested = &n.key; nested_indent = n.indent; }
        else if (n.key == "canary") { cur_nested = &n.key; nested_indent = n.indent; }
        else if (cur_nested && *cur_nested == "debug") {
            if (n.key == "enabled") cfg.debug.enabled = to_bool(n.value, false);
            else if (n.key == "dump_input") cfg.debug.dump_input = to_bool(n.value, false);
            else if (n.key == "dump_meta") cfg.debug.dump_meta = to_bool(n.value, true);
            else if (n.key == "dump_on_alarm_only") cfg.debug.dump_on_alarm_only = to_bool(n.value, true);
            else if (n.key == "dump_every_n") cfg.debug.dump_every_n = to_int(n.value, 0);
            else if (n.key == "dump_dir") cfg.debug.dump_dir = n.value;
            else if (n.key == "max_dumps") cfg.debug.max_dumps = to_int(n.value, 100);
        }
        else if (cur_nested && *cur_nested == "zone") {
            if (n.key == "type") cfg.zone.type = n.value;
            else if (n.key == "radius_m") cfg.zone.radius_m = to_float(n.value, 0.f);
            else if (n.key == "consecutive_frames") cfg.zone.consecutive_frames = to_float(n.value, 3.f);
            else if (n.key == "half_angle_deg") cfg.zone.half_angle_deg = to_float(n.value, 0.f);
            else if (n.key == "has_occlusion") cfg.zone.has_occlusion = to_bool(n.value, false);
            else if (n.key == "occ_axis_deg") cfg.zone.occ_axis_deg = to_float(n.value, 90.f);
            else if (n.key == "occ_half_deg") cfg.zone.occ_half_deg = to_float(n.value, 25.f);
        }
        else if (cur_nested && *cur_nested == "camera") {
            if (n.key == "video_channel") cfg.video_channel = parse_int_array(n.value);
            else if (n.key == "image_width") cfg.image_width = to_int(n.value, 0);
            else if (n.key == "image_height") cfg.image_height = to_int(n.value, 0);
        }
        else if (cur_nested && *cur_nested == "canary") {
            if (n.key == "canary_enabled") cfg.canary_enabled = to_bool(n.value, false);
        }
        else if (cur_nested && *cur_nested == "thresholds") {
            if (n.key == "score" || n.key == "confidence_threshold")
                cfg.confidence_threshold = to_float(n.value, 0.45f);
            else if (n.key == "nms_threshold")
                cfg.nms_threshold = to_float(n.value, 0.5f);
        }
        // height-limit / winch-ice extras (flattened for scaffold convenience)
        else if (n.key == "start_angle_deg") cfg.start_angle_deg = to_float(n.value, 5.f);
        else if (n.key == "stop_angle_deg") cfg.stop_angle_deg = to_float(n.value, 75.f);
        else if (n.key == "min_frames") cfg.min_frames = to_int(n.value, 10);
        else if (n.key == "still_timeout_s") cfg.still_timeout_s = to_int(n.value, 15);
        else if (n.key == "basic_boom_only") cfg.basic_boom_only = to_bool(n.value, true);
        else if (n.key == "area_ratio") cfg.area_ratio = to_float(n.value, 0.3f);
        else if (n.key == "exit_on_alarm") cfg.exit_on_alarm = to_bool(n.value, true);
    }
    if (ground_parse_error) {
        err = static_cast<uint32_t>(EvsStatus::ConfigParse);
        return false;
    }
    return true;
}

} // namespace evs

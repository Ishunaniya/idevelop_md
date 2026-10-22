#include "config_internal.h"
#include "evs/status.h"
#include "evs/task_config.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <map>
#include <set>
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

bool parse_ice_integer(const std::string& value, int& out) {
    if (value.empty())
        return false;
    char* end = nullptr;
    errno = 0;
    const long n = std::strtol(value.c_str(), &end, 10);
    if (errno || end == value.c_str() || *end != '\0' || n < std::numeric_limits<int>::min() ||
        n > std::numeric_limits<int>::max())
        return false;
    out = static_cast<int>(n);
    return true;
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

static bool parse_task_config(std::string_view path, TaskConfig& cfg, uint32_t& err) {
    err = 0;
    const std::string cfg_path(path);
    std::ifstream in(cfg_path);
    if (!in) { err = static_cast<uint32_t>(EvsStatus::ConfigInvalid); return false; }

    struct Node {
        int indent;
        std::string key;
        std::string value;
        bool list_start;
        bool quoted;
    };
    std::vector<Node> nodes;
    bool syntax_error = false;
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
        if (!split_kv(text, k, v)) {
            syntax_error = true;
            continue;
        }
        const std::string raw_value = trim(text.substr(text.find(':') + 1));
        const bool quoted =
            !raw_value.empty() && (raw_value.front() == '\'' || raw_value.front() == '"');
        nodes.push_back({ind, k, v, list_start, quoted});
    }

    // Treat a root model map as a section, never as an empty path. Keep the
    // existing scalar model/Model_path spellings and flat model parameters.
    std::vector<bool> model_node(nodes.size(), false);
    std::vector<std::string> model_key(nodes.size());
    bool model_parse_error = false;
    for (size_t i = 0; i < nodes.size(); ++i) {
        const auto& root = nodes[i];
        if (root.indent != 0 || root.list_start || root.key != "model")
            continue;
        size_t end = i + 1;
        while (end < nodes.size() && nodes[end].indent > root.indent)
            ++end;
        if (end == i + 1)
            continue; // legacy scalar, including an explicitly empty path
        model_node[i] = true;
        if (!root.value.empty())
            model_parse_error = true;
        const int child_indent = nodes[i + 1].indent;
        for (size_t j = i + 1; j < end; ++j) {
            model_node[j] = true;
            const auto& child = nodes[j];
            if (child.list_start || child.indent != child_indent ||
                (child.key != "path" && child.key != "input_width" && child.key != "input_height" &&
                 child.key != "warmup")) {
                model_parse_error = true;
                continue;
            }
            model_key[j] = child.key;
        }
        i = end - 1;
    }

    // Compare definitions in this file, not values inherited from the caller.
    // Ice rejects conflicting aliases; other modules keep legacy last-value
    // behaviour. All modules can read the newly supported model map.
    std::map<std::string, std::string> model_definitions;
    auto read_model_value = [&](const std::string& key, const std::string& value, bool quoted) {
        std::string canonical = value;
        if (key == "path") {
            if (!quoted && !value.empty() &&
                (value.front() == '[' || value.front() == '{' || value == "null" || value == "~"))
                model_parse_error = true;
            cfg.Model_path = value;
        } else if (key == "warmup") {
            if (value != "true" && value != "True" && value != "1" && value != "false" &&
                value != "False" && value != "0")
                model_parse_error = true;
            cfg.warmup = to_bool(value, false);
            canonical = cfg.warmup ? "true" : "false";
        } else {
            int parsed = 0;
            if (!parse_ice_integer(value, parsed) || parsed < 0)
                model_parse_error = true;
            const int legacy = to_int(value, 0);
            if (key == "input_width")
                cfg.input_width = legacy;
            else
                cfg.input_height = legacy;
            canonical = std::to_string(legacy);
        }
        const auto existing = model_definitions.emplace(key, canonical);
        if (!existing.second && existing.first->second != canonical)
            model_parse_error = true;
    };

    // Ignore children of retired ice inventories rather than flattening them
    // into unrelated task settings. Ice rejects the root key below.
    std::vector<bool> retired_ice_child(nodes.size(), false);
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (nodes[i].key != "ice_objects") continue;
        for (size_t j = i + 1; j < nodes.size() && nodes[j].indent > nodes[i].indent; ++j)
            retired_ice_child[j] = true;
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
    bool ice_parse_error = false;
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
        if (n.list_start || retired_ice_child[i])
            continue;
        if (model_node[i]) {
            if (!model_key[i].empty())
                read_model_value(model_key[i], n.value, n.quoted);
            continue;
        }
        // Leave the nested block only when we drop back to the parent indent.
        // Comparing against a fixed 2 spaces wrongly evicted 2-space children.
        if (cur_nested && n.indent <= nested_indent) cur_nested = nullptr;

        if (n.key == "task") cfg.task = n.value;
        else if (n.key == "module_id") {
            // Preserve legacy module parsing; ice validates the complete token below.
            if (n.value.size() > 2 && n.value.substr(0, 2) == "0x")
                cfg.module_id = static_cast<uint16_t>(std::strtoul(n.value.c_str(), nullptr, 16));
            else
                cfg.module_id = static_cast<uint16_t>(to_int(n.value, 0));
        }
        else if (n.key == "camera_ids" || n.key == "video_channel")
            cfg.video_channel = parse_int_array(n.value);
        else if (n.key == "inference_enabled") {
            if (n.value != "true" && n.value != "false")
                ice_parse_error = true;
            else
                cfg.inference_enabled = n.value == "true";
        } else if (n.key == "ice_mode") {
            // Read-only compatibility with the previously shipped model YAML.
            if (n.value != "model") ice_parse_error = true;
        }
        else if (n.key == "ice_saturation_max")
            ice_parse_error |= !parse_ice_integer(n.value, cfg.ice.saturation_max);
        else if (n.key == "ice_value_min")
            ice_parse_error |= !parse_ice_integer(n.value, cfg.ice.value_min);
        else if (n.key == "backend") cfg.backend = n.value;
        else if (n.key == "model" || n.key == "Model_path")
            read_model_value("path", n.value, n.quoted);
        else if (n.key == "confidence_threshold")
            cfg.confidence_threshold = to_float(n.value, 0.45f);
        else if (n.key == "score_threshold")
            cfg.confidence_threshold = to_float(n.value, 0.45f);
        else if (n.key == "nms_threshold") cfg.nms_threshold = to_float(n.value, 0.5f);
        else if (n.key == "canary_enabled") cfg.canary_enabled = to_bool(n.value, false);
        else if (n.key == "output_format") cfg.output_format = n.value;
        else if (n.key == "person_class_id" || n.key == "winch_class_id") cfg.person_class_id = to_int(n.value, -1);
        else if (n.key == "decoder_classes") cfg.decoder_classes = to_int(n.value, 80);
        else if (n.key == "input_width")
            read_model_value("input_width", n.value, n.quoted);
        else if (n.key == "input_height")
            read_model_value("input_height", n.value, n.quoted);
        else if (n.key == "input_format") cfg.input_format = n.value; // rgb | bgr
        else if (n.key == "letterbox") cfg.letterbox = to_bool(n.value, true);
        else if (n.key == "warmup")
            read_model_value("warmup", n.value, n.quoted);
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
    // Keep legacy camera parsing for other modules. Ice tasks must never turn
    // a misspelled channel into camera zero through strtol's permissive fallback.
    if (cfg.module_id == 0x0008) {
        for (const Node& n : nodes) {
            if (n.key != "video_channel" && n.key != "camera_ids")
                continue;
            if (n.value.size() < 3 || n.value.front() != '[' || n.value.back() != ']') {
                ice_parse_error = true;
                continue;
            }
            const std::string body = trim(n.value.substr(1, n.value.size() - 2));
            if (body.empty() || body.back() == ',')
                ice_parse_error = true;
            std::istringstream items(body);
            std::string item;
            while (std::getline(items, item, ',')) {
                int channel = -1;
                if (!parse_ice_integer(trim(item), channel))
                    ice_parse_error = true;
            }
        }
    }
    if (cfg.module_id == 8) {
        const std::set<std::string> allowed = {"ice_mode", "ice_saturation_max", "ice_value_min"};
        std::set<std::string> seen;
        for (size_t i = 0; i < nodes.size(); ++i) {
            if (retired_ice_child[i])
                continue;
            const auto& key = nodes[i].key;
            if (key.rfind("ice_", 0) == 0 && !allowed.count(key))
                ice_parse_error = true;
            if ((allowed.count(key) || key == "area_ratio" || key == "exit_on_alarm" ||
                 key == "inference_enabled") &&
                !seen.insert(key).second)
                ice_parse_error = true;
        }
        for (const auto& node : nodes) {
            if (node.key == "module_id") {
                char* end = nullptr;
                errno = 0;
                const int base = node.value.rfind("0x", 0) == 0 ? 16 : 10;
                const auto module = std::strtoul(node.value.c_str(), &end, base);
                if (errno || end == node.value.c_str() || *end != '\0' ||
                    node.value.front() == '-' || module > std::numeric_limits<uint16_t>::max())
                    ice_parse_error = true;
            }
            if (node.key == "winch_class_id" || node.key == "decoder_classes") {
                int value = 0;
                if (!parse_ice_integer(node.value, value)) ice_parse_error = true;
            }
            if (node.key == "exit_on_alarm" && node.value != "true" && node.value != "false")
                ice_parse_error = true;
            if (node.key != "area_ratio" && node.key != "confidence_threshold" &&
                node.key != "score_threshold" && node.key != "score" && node.key != "nms_threshold")
                continue;
            char* end = nullptr;
            errno = 0;
            const float value = std::strtof(node.value.c_str(), &end);
            if (errno || end == node.value.c_str() || *end != '\0' || !std::isfinite(value))
                ice_parse_error = true;
        }
        ice_parse_error |= syntax_error || model_parse_error || !cfg.inference_enabled;
    }
    if (ground_parse_error || (cfg.module_id == 8 && ice_parse_error)) {
        err = static_cast<uint32_t>(EvsStatus::ConfigParse);
        return false;
    }
    return true;
}

namespace detail {
bool load_task_config_with_module(std::string_view path, TaskConfig& cfg, uint32_t& err,
                                  uint16_t& parsed_module) {
    // Preserve overlay semantics, but publish only a fully parsed configuration.
    TaskConfig next = cfg;
    const bool ok = parse_task_config(path, next, err);
    parsed_module = next.module_id;
    if (!ok)
        return false;
    cfg = std::move(next);
    return true;
}
} // namespace detail

bool load_task_config(std::string_view path, TaskConfig& cfg, uint32_t& err) {
    uint16_t parsed_module = cfg.module_id;
    return detail::load_task_config_with_module(path, cfg, err, parsed_module);
}

} // namespace evs

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#include "evs/status.h"
#include "evs/geom.h"
#include "evs/vision_task.h"
#include "evs/task_config.h"
#include "evs/model_registry.h"
#include "evs/person_plugin.h"
#include "evs/ground_mapper.h"
#include "evs/ground_metrics.h"
#include "evs/ground_params.h"
#include "evs/detection_decoder.h"
#include "evs/preprocess.h"
#include "evs/log.h"
#include "evs/debug.h"
#include "evs/x86_backend.h"
#include "evs/offline_eval.h"
#include "../src/backend/yolo_decode.h"

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg)                                                        \
    do {                                                                        \
        if (cond) {                                                             \
            ++g_pass;                                                           \
        } else {                                                                \
            ++g_fail;                                                           \
            std::cout << "FAIL: " << (msg) << " @line " << __LINE__ << "\n";    \
        }                                                                       \
    } while (0)

using namespace evs;

static bool write_sample(const std::string& path, const std::string& content) {
    std::ofstream o(path);
    if (!o) return false;
    o << content;
    return true;
}

int main() {
    // Keep the earlier sections quiet; the logging tests install their own sink.
    Logger::instance().set_sink([](LogLevel, const std::string&) {});

    // 1. module-specific error code construction
    CHECK(make_module_error(0x0004, 0x0001) == 0x0004C001u, "make_module_error");

    // 2. geometry: circle
    CHECK(in_circle({1.f, 1.f}, {0.f, 0.f}, 1.5f), "in_circle true");
    CHECK(!in_circle({2.f, 2.f}, {0.f, 0.f}, 1.5f), "in_circle false");

    // 3. geometry: sector (axis 0 deg, half 40 deg, radius 50 m)
    CHECK(in_sector({40.f, 0.f}, {0.f, 0.f}, 0.f, 40.f, 50.f), "in_sector along axis");
    CHECK(!in_sector({-40.f, 0.f}, {0.f, 0.f}, 0.f, 40.f, 50.f), "in_sector opposite");
    CHECK(in_sector({30.f, 30.f}, {0.f, 0.f}, 45.f, 40.f, 50.f), "in_sector 45deg within 40 half");

    // 4. config parsing
    const std::string sample =
        "task: load_near\n"
        "module_id: 0x0004\n"
        "camera_ids: [0]\n"
        "backend: mock\n"
        "model: models/person.rknn\n"
        "input_width: 640\n"
        "input_height: 640\n"
        "zone:\n"
        "  type: circle\n"
        "  radius_m: 10\n"
        "  consecutive_frames: 3\n"
        "ground:\n"
        "  enabled: true\n"
        "  reference: camera_origin\n"
        "  distance_mode: legacy_forward_y\n"
        "  cameras:\n"
        "    - id: 0\n"
        "      mapper: homography\n"
        "      params:\n"
        "        matrix: /tmp/test_matrix.txt\n"
        "        calibration_width: 1280\n"
        "        calibration_height: 720\n"
        "        strict_matrix: false\n"
        "        required_feature: true\n"
        "        distortion: [-0.21, 0.08, 0.001, -0.002]\n"
        "      mount:\n"
        "        x_m: 1.5\n"
        "        y_m: 2.5\n"
        "        yaw_deg: 10\n"
        "thresholds:\n"
        "  score: 0.45\n";
    const std::string path = "tmp_sample_load_near.yaml";
    CHECK(write_sample(path, sample), "write sample yaml");
    TaskConfig cfg;
    uint32_t err = 0;
    CHECK(load_task_config(path, cfg, err), "load_task_config ok");
    CHECK(cfg.module_id == 0x0004, "module_id parsed");
    CHECK(cfg.video_channel.size() == 1 && cfg.video_channel[0] == 0, "video_channel parsed");
    CHECK(cfg.backend == "mock", "backend parsed");
    CHECK(cfg.zone.type == "circle" && cfg.zone.radius_m == 10.f, "zone parsed");
    CHECK(cfg.confidence_threshold == 0.45f, "threshold parsed");
    CHECK(cfg.Model_path == "models/person.rknn", "Model_path parsed");
    CHECK(cfg.ground.explicitly_configured && cfg.ground.cameras.size() == 1,
          "ground cameras parsed");
    CHECK(cfg.ground.cameras[0].id == 0 &&
              cfg.ground.cameras[0].mapper == "homography",
          "ground camera fields parsed");
    CHECK(cfg.ground.cameras[0].mount.enabled &&
              cfg.ground.cameras[0].mount.x_m == 1.5f &&
              cfg.ground.cameras[0].mount.yaw_deg == 10.f,
          "ground mount parsed");
    CHECK(ground_param_string(cfg.ground.cameras[0], "matrix") ==
              "/tmp/test_matrix.txt" &&
              ground_param_int(cfg.ground.cameras[0], "calibration_width", 0) == 1280 &&
              ground_param_bool(cfg.ground.cameras[0], "required_feature", false),
          "ground generic scalar params parsed");
    const std::vector<float> distortion =
        ground_param_float_array(cfg.ground.cameras[0], "distortion");
    CHECK(distortion.size() == 4 && std::fabs(distortion[0] + 0.21f) < 1e-6f,
          "ground generic float-array param parsed");
    CHECK(pack_camera_channel(1, 2, 3) == 0xFF030201u, "camera_channel one byte per camera");

    const std::string spec_yaml =
        "Model_path: models/spec.rknn\n"
        "confidence_threshold: 0.5\n"
        "nms_threshold: 0.4\n"
        "camera:\n"
        "  video_channel: [1, 2, 3]\n"
        "  image_width: 1280\n"
        "  image_height: 720\n"
        "canary:\n"
        "  canary_enabled: true\n";
    const std::string spec_path = "tmp_spec_names.yaml";
    CHECK(write_sample(spec_path, spec_yaml), "write spec-name yaml");
    TaskConfig spec{};
    CHECK(load_task_config(spec_path, spec, err), "load spec-name yaml");
    CHECK(spec.Model_path == "models/spec.rknn", "spec Model_path");
    CHECK(spec.confidence_threshold == 0.5f, "spec confidence_threshold");
    CHECK(spec.nms_threshold == 0.4f, "spec nms_threshold");
    CHECK(spec.video_channel.size() == 3 && spec.video_channel[2] == 3, "spec video_channel");
    CHECK(spec.image_width == 1280 && spec.image_height == 720, "spec image size");
    CHECK(spec.canary_enabled, "spec canary_enabled");
    CHECK(!spec.ground.explicitly_configured && spec.ground.cameras.size() == 4 &&
              spec.ground.distance_mode == DistanceMode::LegacyForwardY,
          "missing ground section gets legacy camera 0..3 defaults");
    std::remove(spec_path.c_str());

    const std::string invalid_ground_path = "tmp_invalid_ground.yaml";
    CHECK(write_sample(invalid_ground_path,
                       "ground:\n  reference: typo\n  distance_mode: radical\n"),
          "write invalid ground yaml");
    TaskConfig invalid_ground{};
    CHECK(!load_task_config(invalid_ground_path, invalid_ground, err) &&
              err == static_cast<uint32_t>(EvsStatus::ConfigParse),
          "unknown ground reference/mode is rejected");
    CHECK(write_sample(invalid_ground_path,
                       "ground:\n  reference: task\n"
                       "  distance_mode: legacy_forward_y\n"),
          "write task legacy ground yaml");
    CHECK(!load_task_config(invalid_ground_path, invalid_ground, err),
          "task reference with legacy forward distance is rejected");
    std::remove(invalid_ground_path.c_str());

    // Shared loader compatibility: overlay on success, no partial state on failure.
    const std::string compatibility_path = "tmp_config_compatibility.yaml";
    TaskConfig overlay;
    overlay.module_id = 4;
    overlay.confidence_threshold = .91f;
    CHECK(write_sample(compatibility_path, "module_id: 0x0004\n"), "write overlay config");
    CHECK(load_task_config(compatibility_path, overlay, err) &&
              overlay.confidence_threshold == .91f,
          "missing scalar preserves caller value");
    CHECK(write_sample(compatibility_path, "module_id: 0x0004\nconfidence_threshold: 0.1\n"
                                           "ground:\n  reference: typo\n"),
          "write rejected overlay config");
    CHECK(!load_task_config(compatibility_path, overlay, err) && err == 0x4002u &&
              overlay.module_id == 4 && overlay.confidence_threshold == .91f,
          "parser returns bare code and preserves the full previous config");
    VisionTask parse_rejected;
    CHECK(parse_rejected.initialize(compatibility_path) == 0x00044002u,
          "task stamps module identity without publishing a partial config");
    CHECK(write_sample(compatibility_path,
                       "module_id: 0x0004oops\narea_ratio: invalid\nexit_on_alarm: unknown\n"),
          "write legacy permissive config");
    CHECK(load_task_config(compatibility_path, overlay, err) && overlay.module_id == 4 &&
              overlay.area_ratio == 0.f && overlay.exit_on_alarm,
          "strict ice numbers do not alter other modules' legacy parsing");
    std::remove(compatibility_path.c_str());
    TaskConfig conflicting_height;
    conflicting_height.module_id = 7;
    conflicting_height.backend = "unavailable_for_compatibility_test";
    conflicting_height.start_angle_deg = 75.f;
    conflicting_height.stop_angle_deg = 5.f;
    VisionTask height_rejected;
    CHECK(height_rejected.initialize(conflicting_height) == 0x00071009u,
          "inference initialization retains backend-before-policy error precedence");

    // 5. VisionTask lifecycle & failure paths
    VisionTask vt;
    Result out{};
    // Preprocessing really reads frame pixels, so use a real buffer (the previous
    // non-null dummy pointer would be dereferenced by Preprocessor::run).
    std::vector<uint8_t> dummy_buf(1920 * 1080 * 3, 128);
    Frame f{};
    f.camera_id = 0;
    f.width = 1920;
    f.height = 1080;
    f.format = PixelFormat::RGB;
    f.data = dummy_buf.data();
    MachineContext ctx{};

    CHECK(vt.detect(f, ctx, out) == static_cast<uint32_t>(EvsStatus::NotInitialized),
          "detect before init -> NotInitialized");

    Frame bad{};
    bad.camera_id = 0;
    bad.width = 1920;
    bad.height = 1080;
    bad.data = nullptr; // null -> NullPointer
    CHECK(vt.detect(bad, ctx, out) == static_cast<uint32_t>(EvsStatus::NotInitialized),
          "bad frame still rejected before init");

    CHECK(vt.initialize(path) == 0u, "initialize ok");
    CHECK(vt.detect(f, ctx, out) == 0u, "detect after init");
    CHECK(out.camera_channel == pack_camera_channel(0), "camera_channel byte0 is camera 0");
    CHECK(global_registry().refcount("mock", "models/person.rknn") == 1,
          "refcount 1 after init");

    Frame wrong_cam = f;
    wrong_cam.camera_id = 9; // not in camera_ids
    CHECK(vt.detect(wrong_cam, ctx, out) == with_module(0x0004, static_cast<uint32_t>(EvsStatus::InvalidCameraIndex)),
          "wrong camera_id -> module 0x0004 InvalidCameraIndex");

    CHECK(vt.initialize(path) == 0u, "re-init idempotent");
    CHECK(global_registry().refcount("mock", "models/person.rknn") == 1,
          "refcount unchanged on re-init");

    // updateConfig must reject backend / model change
    const std::string rej =
        "task: load_near\n"
        "module_id: 0x0004\n"
        "camera_ids: [0]\n"
        "backend: qnn\n"
        "model: models/person.rknn\n";
    const std::string rpath = "tmp_reject.yaml";
    write_sample(rpath, rej);
    CHECK(vt.updateConfig(rpath) == with_module(0x0004, static_cast<uint32_t>(EvsStatus::NotSupported)),
          "updateConfig rejects backend change");

    vt.release();
    CHECK(global_registry().refcount("mock", "models/person.rknn") == 0,
          "refcount 0 after release");

    std::remove(path.c_str());
    std::remove(rpath.c_str());

    // 6. person plugin configuration builder (make_person_config)
    TaskConfig pc{};
    pc.zone.radius_m = 10.f;
    pc.zone.consecutive_frames = 3.f;
    pc.on_missing_context = "alarm";
    PersonPolicyConfig c4;
    CHECK(make_person_config(0x0004, pc, c4), "make_person_config 0x0004");
    CHECK(c4.zone == PersonPolicyConfig::Zone::Circle, "0x0004 circle");
    CHECK(c4.radius_m == 10.f, "0x0004 radius 10");
    CHECK(c4.missing_context == AlarmState::Alarm, "0x0004 missing->alarm");
    CHECK(c4.require_work == true, "0x0004 requires work");

    pc.on_missing_context = "log_only";
    pc.zone.has_occlusion = true;
    PersonPolicyConfig c5;
    CHECK(make_person_config(0x0005, pc, c5), "make_person_config 0x0005");
    CHECK(c5.missing_context == AlarmState::LogOnly, "0x0005 missing->log_only");
    CHECK(c5.has_occlusion == true, "0x0005 has occlusion");

    pc.zone.has_occlusion = false;
    pc.zone.half_angle_deg = 40.f;
    PersonPolicyConfig c6;
    CHECK(make_person_config(0x0006, pc, c6), "make_person_config 0x0006");
    CHECK(c6.zone == PersonPolicyConfig::Zone::Sector, "0x0006 sector");
    CHECK(c6.dynamic_radius == true, "0x0006 dynamic radius");
    CHECK(c6.sector_half_deg == 40.f, "0x0006 half 40");
    CHECK(c6.missing_context == AlarmState::LogOnly, "0x0006 missing->log_only");

    CHECK(!make_person_config(0x0007, pc, c4), "0x0007 not a person plugin");

    // 6b. function plugins
    CHECK(create_function_plugin(0x0004) != nullptr, "plugin 0x0004");
    CHECK(create_function_plugin(0x0005) != nullptr, "plugin 0x0005");
    CHECK(create_function_plugin(0x0006) != nullptr, "plugin 0x0006");
    CHECK(create_function_plugin(0x0007) != nullptr, "plugin 0x0007");
    CHECK(create_function_plugin(0x0008) != nullptr, "plugin 0x0008");
    CHECK(create_function_plugin(0x0001) == nullptr, "unknown module has no plugin");

    // 7. person_decide: scenarios (working / idle / missing / occlusion / sector)
    auto mk = [](float d, float a) {
        Target t{};
        t.distance_m = d;
        t.angle_deg = a;
        t.confidence = 0.9f;
        return t;
    };
    MachineContext working{};
    working.context_valid = true;
    working.hoist_moving = true; // moving -> working
    MachineContext idle{};
    idle.context_valid = true; // valid but no motion -> still
    MachineContext invalid{};
    invalid.context_valid = false;

    // 7a. load_near, working, 3 consecutive frames -> Alarm
    {
        PersonPolicyConfig cfg = c4; // circle r=10, consec 3, require_work, missing alarm
        PersonPolicyState st{};
        std::vector<Target> p = {mk(5.f, 0.f)};
        CHECK(person_decide(p, working, cfg, st) == AlarmState::LogOnly, "load_near f1 LogOnly");
        CHECK(person_decide(p, working, cfg, st) == AlarmState::LogOnly, "load_near f2 LogOnly");
        CHECK(person_decide(p, working, cfg, st) == AlarmState::Alarm, "load_near f3 Alarm");
        CHECK(person_decide({}, working, cfg, st) == AlarmState::None, "load_near reset None");
    }
    // 7a1. Unmapped distance is recorded and does not count toward Alarm.
    {
        PersonPolicyConfig cfg = c4;
        PersonPolicyState st{};
        std::vector<Target> p = {mk(kDistanceUnknown, 0.f)};
        CHECK(person_decide(p, working, cfg, st) == AlarmState::LogOnly, "unknown dist f1 LogOnly");
        CHECK(person_decide(p, working, cfg, st) == AlarmState::LogOnly, "unknown dist f2 LogOnly");
        CHECK(person_decide(p, working, cfg, st) == AlarmState::LogOnly, "unknown dist f3 stays LogOnly");
        CHECK(st.consecutive == 0, "unknown dist does not count");
        CHECK(person_decide(p, invalid, cfg, st) == AlarmState::LogOnly,
              "unknown dist with missing context is LogOnly");

        Target box{};
        box.x = 0.5f;
        box.y = 500.f / 720.f - 0.1f;
        box.w = 0.2f;
        box.h = 0.2f;
        CameraGroundConfig gc;
        gc.id = 0;
        gc.mapper = "homography";
        auto homography = create_ground_mapper(gc);
        CHECK(homography != nullptr, "camera 0 homography mapper created");
        Frame gf{};
        gf.camera_id = 0;
        gf.width = 1920;  // mapper rescales normalized target to calibration pixels
        gf.height = 1080;
        const GroundPoint hom = homography->map(box, gf, MachineContext{});
        CHECK(hom.ok && std::fabs(hom.y_m - 0.9236935f) < 1e-3f,
              "camera 0 maps to ground metres");

        GroundReference origin;
        origin.valid = true;
        std::vector<Target> ts = {box};
        ts[0].ground_ok = hom.ok;
        ts[0].ground_x_m = hom.x_m;
        ts[0].ground_y_m = hom.y_m;
        apply_ground_metrics(origin, DistanceMode::LegacyForwardY, ts);
        CHECK(std::fabs(ts[0].distance_m - 0.9236935f) < 1e-3f,
              "legacy metric preserves Wy distance");
        apply_ground_metrics(origin, DistanceMode::Radial, ts);
        CHECK(ts[0].distance_m >= std::fabs(hom.y_m), "radial metric uses XY");
        Target left{};
        left.ground_ok = true;
        left.ground_x_m = -1.f;
        left.ground_y_m = 1.f;
        std::vector<Target> left_targets = {left};
        apply_ground_metrics(origin, DistanceMode::LegacyForwardY, left_targets);
        CHECK(std::fabs(left_targets[0].angle_deg + 45.f) < 1e-4f,
              "legacy bearing preserves signed -180..180 range");
        apply_ground_metrics(origin, DistanceMode::Radial, left_targets);
        CHECK(std::fabs(left_targets[0].angle_deg - 315.f) < 1e-4f,
              "radial bearing uses normalized 0..360 range");

        CameraGroundConfig bad_gc;
        bad_gc.id = 9;
        bad_gc.mapper = "homography";
        CHECK(create_ground_mapper(bad_gc) == nullptr,
              "unknown camera without matrix is rejected");
        CameraGroundConfig strict_gc;
        strict_gc.id = 0;
        strict_gc.mapper = "homography";
        strict_gc.params["matrix"] = "/tmp/evs_missing_matrix.txt";
        strict_gc.params["strict_matrix"] = "true";
        CHECK(create_ground_mapper(strict_gc) == nullptr,
              "strict explicit matrix failure does not silently use builtin");
        CameraGroundConfig crane_gc;
        crane_gc.id = 4;
        crane_gc.mapper = "crane_pose";
        auto crane = create_ground_mapper(crane_gc);
        CHECK(crane && !crane->map(box, gf, MachineContext{}).ok,
              "crane pose mapper is still a stub");
    }
    // 7a2. PersonPlugin::decide matches person_decide; configure keeps the streak
    {
        TaskConfig tc;
        tc.module_id = 0x0004;
        tc.zone.radius_m = 10.f;
        tc.zone.consecutive_frames = 3.f;
        tc.on_missing_context = "alarm";
        PersonPolicyConfig built;
        CHECK(make_person_config(0x0004, tc, built), "plugin cfg 0x0004");
        auto plugin = create_function_plugin(0x0004);
        CHECK(plugin && plugin->configure(tc) == 0u, "PersonPlugin configure");
        GroundReference legacy_ref;
        CHECK(plugin->ground_reference(working, legacy_ref) &&
                  legacy_ref.valid && legacy_ref.origin_m.y == 0.f,
              "legacy plugin reference is homography origin");
        tc.ground.use_task_reference = true;
        CHECK(plugin->configure(tc) == 0u, "enable task ground reference");
        MachineContext hook_ctx = working;
        hook_ctx.hoist_radius = 12.f;
        GroundReference hook_ref;
        CHECK(plugin->ground_reference(hook_ctx, hook_ref) &&
                  hook_ref.valid && hook_ref.origin_m.y == 12.f,
              "load-near task reference uses hook radius on local +Y");
        GroundReference missing_ctx_ref;
        MachineContext missing_ctx{};
        missing_ctx.context_valid = false;
        CHECK(plugin->ground_reference(missing_ctx, missing_ctx_ref) &&
                  missing_ctx_ref.valid && missing_ctx_ref.origin_m.y == 0.f,
              "missing context falls back to homography origin");
        tc.ground.use_task_reference = false;
        CHECK(plugin->configure(tc) == 0u, "restore legacy ground reference");
        PersonPolicyState st{};
        std::vector<Target> p = {mk(5.f, 0.f)};
        Frame unused{};
        Result r{};
        CHECK(plugin->decide(unused, working, p, r) == 0u, "plugin decide f1");
        CHECK(r.alarm == person_decide(p, working, built, st), "plugin f1 matches person_decide");
        CHECK(r.targets.size() == 1, "plugin copies targets");
        CHECK(r.limit_state == 0 && r.ice_ratio == 0.f, "person leaves height and ice at 0");
        CHECK(plugin->decide(unused, working, p, r) == 0u, "plugin decide f2");
        CHECK(r.alarm == person_decide(p, working, built, st), "plugin f2 matches");
        tc.zone.consecutive_frames = 100.f;
        CHECK(plugin->configure(tc) == 0u, "hot-update consecutive frames");
        built.consecutive_frames = 100;
        CHECK(plugin->decide(unused, working, p, r) == 0u, "plugin decide f3");
        CHECK(r.alarm == person_decide(p, working, built, st), "hot-update reaches decide");
        CHECK(r.alarm == AlarmState::LogOnly, "streak kept, threshold 100 stays LogOnly");
        tc.module_id = 0x0007;
        CHECK(plugin->configure(tc) == static_cast<uint32_t>(EvsStatus::NotSupported),
              "bad configure rejected");
        tc.module_id = 0x0004;
        CHECK(plugin->decide(unused, working, p, r) == 0u, "decide after rejected configure");
        CHECK(r.alarm == AlarmState::LogOnly, "rejected configure keeps old policy");
    }
    // 7b. load_near, idle (still), person present -> only record
    {
        PersonPolicyConfig cfg = c4;
        PersonPolicyState st{};
        std::vector<Target> p = {mk(5.f, 0.f)};
        CHECK(person_decide(p, idle, cfg, st) == AlarmState::LogOnly, "load_near idle LogOnly");
        CHECK(person_decide(p, idle, cfg, st) == AlarmState::LogOnly, "load_near idle stays LogOnly");
    }
    // 7b2. is_working=1 with no motion rates counts as working
    {
        PersonPolicyConfig cfg = c4;
        PersonPolicyState st{};
        std::vector<Target> p = {mk(5.f, 0.f)};
        MachineContext flagged{};
        flagged.context_valid = true;
        flagged.is_working = 1;
        CHECK(person_decide(p, flagged, cfg, st) == AlarmState::LogOnly, "is_working f1 LogOnly");
        CHECK(person_decide(p, flagged, cfg, st) == AlarmState::LogOnly, "is_working f2 LogOnly");
        CHECK(person_decide(p, flagged, cfg, st) == AlarmState::Alarm, "is_working f3 Alarm");
    }
    // 7c. load_near, missing context, person -> immediate Alarm
    {
        PersonPolicyConfig cfg = c4;
        PersonPolicyState st{};
        std::vector<Target> p = {mk(5.f, 0.f)};
        CHECK(person_decide(p, invalid, cfg, st) == AlarmState::Alarm, "load_near missing->Alarm");
        std::vector<Target> none;
        CHECK(person_decide(none, invalid, cfg, st) == AlarmState::None, "load_near missing no person");
    }
    // 7d. load_under, missing context, person -> LogOnly (NOT alarm)
    {
        PersonPolicyConfig cfg = c5; // missing log_only, has_occlusion
        PersonPolicyState st{};
        std::vector<Target> p = {mk(5.f, 0.f)};
        CHECK(person_decide(p, invalid, cfg, st) == AlarmState::LogOnly, "load_under missing->LogOnly");
    }
    // 7e. load_under occlusion: person directly below load excluded
    {
        PersonPolicyConfig cfg = c5; // has_occlusion true, axis 90, half 25
        PersonPolicyState st{};
        std::vector<Target> below = {mk(5.f, 90.f)}; // inside occlusion sector
        CHECK(person_decide(below, working, cfg, st) == AlarmState::None, "load_under occluded excluded");
        std::vector<Target> side = {mk(5.f, 0.f)}; // outside occlusion sector
        CHECK(person_decide(side, working, cfg, st) == AlarmState::LogOnly, "load_under side LogOnly");
    }
    // 7f. boom_under sector with dynamic radius + clamp
    {
        PersonPolicyConfig cfg = c6; // sector, dynamic r>=50, axis 0, half 40
        PersonPolicyState st{};
        MachineContext ctx = working;
        ctx.hoist_radius = 60.f;
        std::vector<Target> along = {mk(40.f, 0.f)};   // within radius & axis
        CHECK(person_decide(along, ctx, cfg, st) == AlarmState::LogOnly, "boom_under along axis LogOnly");
        std::vector<Target> off = {mk(40.f, 70.f)};     // outside half-angle
        CHECK(person_decide(off, ctx, cfg, st) == AlarmState::None, "boom_under off-angle None");
        std::vector<Target> far = {mk(80.f, 0.f)};      // beyond radius
        CHECK(person_decide(far, ctx, cfg, st) == AlarmState::None, "boom_under beyond radius None");
        // clamp: hoist_radius=30 -> min 50
        MachineContext ctx2 = working;
        ctx2.hoist_radius = 30.f;
        std::vector<Target> near = {mk(45.f, 0.f)};     // 45 <= 50 clamp
        CHECK(person_decide(near, ctx2, cfg, st) == AlarmState::LogOnly, "boom_under clamp to 50");
    }
    // 7g. decode_person convention [class, conf, x, y, w, h] x6, score filter
    {
        std::vector<float> out = {0.f, 0.9f, 0.1f, 0.2f, 0.3f, 0.4f,
                                  0.f, 0.2f, 0.5f, 0.5f, 0.1f, 0.1f};
        std::vector<Target> ps;
        decode_person(out, 0.45f, ps);
        CHECK(ps.size() == 1, "decode_person keeps 1 above threshold");
        CHECK(ps[0].confidence == 0.9f && ps[0].x == 0.1f, "decode_person fields");
        std::vector<Target> empty;
        decode_person({}, 0.45f, empty);
        CHECK(empty.empty(), "decode_person empty output");
    }

    // 8. detection decoders (output-coupling removal)
    CHECK(create_decoder("nope") == nullptr, "unknown format -> nullptr");
    CHECK(create_decoder("") != nullptr, "empty format defaults to flat6");
    {
        auto d = create_decoder("flat6");
        CHECK(std::string(d->format()) == "flat6", "flat6 decoder tag");
        std::vector<float> out = {0.f, 0.9f, 0.1f, 0.2f, 0.3f, 0.4f,
                                  0.f, 0.2f, 0.5f, 0.5f, 0.1f, 0.1f};
        std::vector<Target> ps;
        d->decode(out, 0.45f, ps);
        CHECK(ps.size() == 1, "flat6 decoder keeps 1 above threshold");
        CHECK(ps[0].confidence == 0.9f && ps[0].x == 0.1f, "flat6 fields");
    }
    {
        // yolov8: row = [x,y,w,h, s0, s1]; num_classes=2; keep only class 1
        auto d = create_decoder("yolov8", /*person_class_id=*/1, /*num_classes=*/2);
        CHECK(std::string(d->format()) == "yolov8", "yolov8 decoder tag");
        std::vector<float> out = {
            0.1f,0.2f,0.3f,0.4f, 0.1f,0.9f,  // class1 high -> keep
            0.5f,0.5f,0.1f,0.1f, 0.8f,0.2f   // class0 high -> filtered
        };
        std::vector<Target> ps;
        d->decode(out, 0.45f, ps);
        CHECK(ps.size() == 1, "yolov8 filters to person class");
        CHECK(ps[0].class_id == 1 && ps[0].confidence == 0.9f, "yolov8 class/conf");
        CHECK(ps[0].x == 0.1f && ps[0].w == 0.3f, "yolov8 box fields");
    }
    {
        // yolov8: raise threshold above all scores -> none kept
        auto d = create_decoder("yolov8", -1, 2);
        std::vector<float> out = {0.1f,0.2f,0.3f,0.4f, 0.1f,0.9f};
        std::vector<Target> ps;
        d->decode(out, 0.95f, ps);
        CHECK(ps.empty(), "yolov8 threshold drops all");
    }
    {
        // yolov8: malformed length (not a multiple of row width) -> empty
        auto d = create_decoder("yolov8", -1, 2);
        std::vector<float> out = {0.1f,0.2f,0.3f}; // incomplete row
        std::vector<Target> ps;
        d->decode(out, 0.45f, ps);
        CHECK(ps.empty(), "yolov8 malformed -> empty");
    }
    {
        auto d = create_decoder("yolox", /*person_class_id=*/0, /*num_classes=*/80);
        CHECK(d && std::string(d->format()) == "yolox", "yolox decoder tag");
        std::vector<float> row(4 + 80, 0.f);
        row[0] = 0.2f; row[1] = 0.3f; row[2] = 0.1f; row[3] = 0.1f;
        row[4] = 0.8f; // class 0
        std::vector<Target> ps;
        d->decode(row, 0.25f, ps);
        CHECK(ps.size() == 1 && ps[0].class_id == 0 && ps[0].confidence == 0.8f,
              "yolox row keeps person");
    }
    {
        // One 2x2 NCHW YOLOX map. Only cell (x=0,y=0) is a detection.
        // stride = 640/2 = 320. cx = (0.25+0)*320/640 = 0.125.
        // w = exp(log(0.2))*320/640 = 0.1.
        constexpr int S = 2;
        constexpr int CH = 5 + 80;
        std::vector<float> map(static_cast<size_t>(CH) * S * S, 0.f);
        auto put = [&](int c, int y, int x, float v) {
            map[static_cast<size_t>(c) * S * S + static_cast<size_t>(y) * S + x] = v;
        };
        put(0, 0, 0, 0.25f);
        put(1, 0, 0, 0.5f);
        put(2, 0, 0, std::log(0.2f));
        put(3, 0, 0, std::log(0.2f));
        put(4, 0, 0, 0.9f);
        put(5, 0, 0, 0.8f);
        evs::yolo::Head h;
        h.data = map.data();
        h.n_dims = 4;
        h.d0 = 1;
        h.d1 = CH;
        h.d2 = S;
        h.d3 = S;
        h.elems = map.size();
        h.mode = evs::yolo::HeadMode::kClsAct;
        std::vector<float> rows;
        evs::yolo::decode_heads({h}, rows);
        auto d = create_decoder("yolox", 0, 80);
        std::vector<Target> ps;
        d->decode(rows, 0.25f, ps);
        CHECK(ps.size() == 1, "yolox head yields one box");
        CHECK(ps[0].class_id == 0, "yolox head class 0");
        CHECK(std::fabs(ps[0].confidence - 0.72f) < 1e-4f, "yolox obj*cls");
        CHECK(std::fabs(ps[0].x - 0.125f) < 1e-4f, "yolox cx");
        CHECK(std::fabs(ps[0].y - 0.25f) < 1e-4f, "yolox cy");
        CHECK(std::fabs(ps[0].w - 0.1f) < 1e-4f, "yolox width");
        CHECK(std::fabs(ps[0].h - 0.1f) < 1e-4f, "yolox height");
    }
    {
        // Demo blob: one [anchors, 85], stride 8 then 16 then 32, y outer, x inner.
        // Anchor 0 is stride 8, grid (0, 0).
        constexpr int CH = 85;
        constexpr int N = 80 * 80 + 40 * 40 + 20 * 20;
        std::vector<float> blob(static_cast<size_t>(N) * CH, 0.f);
        blob[0] = 0.25f;
        blob[1] = 0.5f;
        blob[2] = std::log(0.2f);
        blob[3] = std::log(0.2f);
        blob[4] = 0.9f;
        blob[5] = 0.8f;
        evs::yolo::Head h;
        h.data = blob.data();
        h.n_dims = 3;
        h.d0 = 1;
        h.d1 = N;
        h.d2 = CH;
        h.elems = blob.size();
        h.mode = evs::yolo::HeadMode::kClsAct;
        std::vector<float> rows;
        evs::yolo::decode_heads({h}, rows);
        auto d = create_decoder("yolox", 0, 80);
        std::vector<Target> ps;
        d->decode(rows, 0.25f, ps);
        CHECK(ps.size() == 1, "yolox packed blob yields one box");
        CHECK(std::fabs(ps[0].confidence - 0.72f) < 1e-4f, "yolox packed score");
        CHECK(std::fabs(ps[0].x - 0.25f * 8.f / 640.f) < 1e-4f, "yolox packed cx");
        CHECK(std::fabs(ps[0].y - 0.5f * 8.f / 640.f) < 1e-4f, "yolox packed cy");
        CHECK(std::fabs(ps[0].w - 0.2f * 8.f / 640.f) < 1e-4f, "yolox packed w");
        CHECK(std::fabs(ps[0].h - 0.2f * 8.f / 640.f) < 1e-4f, "yolox packed h");
    }

    // 9. preprocessing: frame -> model input tensor, and letterbox inverse map
    {
        Preprocessor pre;
        PreprocessConfig pconf;
        pconf.input_width = 640;
        pconf.input_height = 640;
        pconf.letterbox = true;
        pconf.target_format = PixelFormat::RGB;
        CHECK(pre.configure(pconf), "preprocessor configured");

        // 200x100 RGB source -> 640x640 letterbox input
        std::vector<uint8_t> src(200 * 100 * 3, 200);
        Frame sf{};
        sf.camera_id = 0;
        sf.width = 200;
        sf.height = 100;
        sf.format = PixelFormat::RGB;
        sf.data = src.data();

        std::vector<uint8_t> pout;
        PreprocessMap pmap{};
        CHECK(pre.run(sf, pout, pmap) == 0u, "preprocess run ok");
        CHECK(pout.size() == 640u * 640u * 3u, "preprocess output size");
        // scale = min(640/200, 640/100) = 3.2; content 640x320 -> pad_y = 160
        CHECK(pmap.scale_x == 3.2f && pmap.scale_y == 3.2f, "letterbox scale");
        CHECK(pmap.pad_x == 0.f && pmap.pad_y == 160.f, "letterbox pad_y centered");
        CHECK(pout[0] == 114 && pout[1] == 114 && pout[2] == 114,
              "letterbox padding is gray 114");

        // Undo the mapping: a target at the input center must land on the
        // original frame center with size preserved.
        Target t{};
        t.x = 0.5f; t.y = 0.5f; t.w = 0.2f; t.h = 0.2f;
        map_target_to_original(pmap, t, 200, 100);
        CHECK(std::fabs(t.x - 0.5f) < 1e-3f, "map center x -> 0.5");
        CHECK(std::fabs(t.y - 0.5f) < 1e-3f, "map center y -> 0.5");
        CHECK(std::fabs(t.w - 0.2f) < 1e-3f, "map width preserved");
        // Vertical extent doubles in relative terms: the source is 2:1 while
        // the input is square, so a square box is 20% of width but 40% of height.
        CHECK(std::fabs(t.h - 0.4f) < 1e-3f, "map height scales with aspect");

        // Null frame pointer -> NullPointer, never dereferenced.
        Frame no_data = sf;
        no_data.data = nullptr;
        CHECK(pre.run(no_data, pout, pmap) == static_cast<uint32_t>(EvsStatus::NullPointer),
              "preprocess null data -> NullPointer");

        // Invalid input dims -> unconfigured; run() reports NotInitialized.
        Preprocessor nc;
        PreprocessConfig bad;
        bad.input_width = 0;
        bad.input_height = 640;
        CHECK(!nc.configure(bad), "configure rejects zero dims");
        CHECK(!nc.configured(), "unconfigured stays unconfigured");
        CHECK(nc.run(sf, pout, pmap) == static_cast<uint32_t>(EvsStatus::NotInitialized),
              "run before configure -> NotInitialized");
    }

    // 10. logging: level filtering, custom sink, ring buffer
    {
        std::vector<std::string> lines;
        Logger::instance().set_sink([&](LogLevel, const std::string& l) { lines.push_back(l); });
        Logger::instance().clear_ring();
        Logger::instance().set_level(LogLevel::Warn);
        EVS_LOGE("e%d", 1);
        EVS_LOGW("w%d", 2);
        EVS_LOGI("i%d", 3);
        EVS_LOGD("d%d", 4);
        CHECK(lines.size() == 2, "level=Warn keeps error+warn only");
        CHECK(lines[0].find("[ERROR]") != std::string::npos, "error line tagged");
        CHECK(lines[0].find("e1") != std::string::npos, "printf-style formatting");
        CHECK(lines[0].find("test_core") != std::string::npos, "source name in line");

        const std::size_t before = lines.size();
        Logger::instance().set_level(LogLevel::Off);
        EVS_LOGE("silenced");
        CHECK(lines.size() == before, "level Off emits nothing");

        // Ring buffer. Use Info, not Trace: -DEVS_LOG_LEVEL strips trace/debug
        // at compile time in release builds, so a trace-based ring would be
        // empty here and would make the assertions below build-dependent.
        Logger::instance().set_ring_capacity(2);
        Logger::instance().clear_ring();
        Logger::instance().set_level(LogLevel::Info);
        EVS_LOGI("t1");
        EVS_LOGI("t2");
        EVS_LOGI("t3");
        const std::vector<std::string> ring = Logger::instance().tail(10);
        CHECK(ring.size() == 2, "ring capacity honored");
        CHECK(ring.size() == 2 && ring[1].find("t3") != std::string::npos,
              "ring keeps the newest lines");
        CHECK(Logger::instance().tail(1).size() == 1, "tail(n) limited");

        CHECK(log_level_from_string("warn") == LogLevel::Warn, "parse warn");
        CHECK(log_level_from_string("DEBUG") == LogLevel::Debug, "parse DEBUG");
        CHECK(log_level_from_string("nonsense", LogLevel::Info) == LogLevel::Info,
              "unknown level falls back");
        CHECK(std::string(log_level_name(LogLevel::Trace)) == "TRACE", "level name");
        Logger::instance().set_ring_capacity(64);
        // The sink above captured a local vector: it MUST be detached before
        // that vector dies, otherwise every later log line uses a dangling
        // reference (ASan: stack-use-after-scope).
        Logger::instance().set_sink([](LogLevel, const std::string&) {});
    }

    // 11. TaskDebug: counters, latency stats, snapshot JSON
    {
        TaskDebug dbg;
        DebugConfig dc;
        dc.enabled = true;
        dc.dump_input = false; // IO is tested separately below
        dbg.configure(dc);
        for (int i = 0; i < 10; ++i)
            dbg.note(1.0, 2.0, 0.5, 3.5 + i * 0.1, 0u, AlarmState::None, 0, 1000 + i);

        CHECK(dbg.stats().frames == 10, "frames counted");
        CHECK(dbg.stats().errors == 0, "no errors so far");
        CHECK(dbg.stats().total.count == 10, "timing samples collected");
        CHECK(dbg.stats().infer.avg_ms == 2.0, "infer average");
        CHECK(dbg.stats().total.min_ms == 3.5, "min latency tracked");
        CHECK(dbg.stats().total.max_ms > 4.3, "max latency tracked");
        CHECK(dbg.stats().total.p95_ms > 0.0, "p95 estimated");

        dbg.note(0.0, 0.0, 0.0, 1.0, 0x00001001u, AlarmState::Alarm, 2, 7);
        CHECK(dbg.stats().errors == 1, "failed frame counted as error");
        CHECK(dbg.stats().alarms == 1, "alarm frame counted");
        CHECK(dbg.stats().last_status == 0x00001001u, "last status retained");
        CHECK(dbg.stats().last_targets == 2, "target count retained");

        dbg.note_budget(true);
        CHECK(dbg.stats().over_budget == 1, "budget miss counted");

        const std::string snap = dbg.snapshot_json();
        CHECK(snap.find("\"frames\":11") != std::string::npos, "snapshot frames");
        CHECK(snap.find("\"errors\":1") != std::string::npos, "snapshot errors");
        CHECK(snap.find("\"p95\"") != std::string::npos, "snapshot latency block");

        dbg.reset();
        CHECK(dbg.stats().frames == 0, "reset clears counters");
        TaskDebug fresh;
        CHECK(fresh.stats().frames == 0, "fresh TaskDebug is empty");
    }

    // 12. dump policy: PPM + JSON sidecar, bounded by max_dumps, alarm-only mode
    {
        const std::string dir = "tmp_evs_dump";
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);

        TaskDebug dbg;
        DebugConfig dc;
        dc.enabled = true;
        dc.dump_input = true;
        dc.dump_meta = true;
        dc.dump_every_n = 1;
        dc.dump_on_alarm_only = false;
        dc.dump_dir = dir;
        dc.max_dumps = 2;
        dbg.configure(dc);

        std::vector<uint8_t> in(64 * 64 * 3, 7);
        PreprocessMap pm{};
        pm.in_w = 64;
        pm.in_h = 64;
        pm.scale_x = 1.f;
        pm.scale_y = 1.f;
        std::vector<Target> ts(1, Target{});

        bool wrote = false;
        for (int i = 0; i < 5; ++i)
            wrote |= dbg.maybe_dump(320, 240, pm, in, ts, 0u, AlarmState::None, i);
        CHECK(wrote, "periodic dump wrote files");

        int files = 0;
        for (auto& p : std::filesystem::directory_iterator(dir, ec)) { (void)p; ++files; }
        CHECK(files == 4, "max_dumps=2 -> 2 ppm + 2 json");

        // alarm-only mode skips neutral frames, keeps alarm frames
        TaskDebug alarm_dbg;
        DebugConfig ac;
        ac.enabled = true;
        ac.dump_input = true;
        ac.dump_on_alarm_only = true;
        ac.dump_every_n = 0;
        ac.dump_dir = dir;
        alarm_dbg.configure(ac);
        std::vector<uint8_t> input2(32 * 32 * 3, 3);
        PreprocessMap pm2{};
        pm2.in_w = 32;
        pm2.in_h = 32;
        pm2.scale_x = 1.f;
        pm2.scale_y = 1.f;
        const int before = files;
        CHECK(!alarm_dbg.maybe_dump(320, 240, pm2, input2, {}, 0u, AlarmState::None, 1),
              "alarm-only skips neutral frames");
        CHECK(alarm_dbg.maybe_dump(320, 240, pm2, input2, {}, 0u, AlarmState::Alarm, 2),
              "alarm-only stores alarm frames");
        (void)before;

        DebugConfig off;
        alarm_dbg.configure(off);
        CHECK(!alarm_dbg.maybe_dump(320, 240, pm2, input2, {}, 0u, AlarmState::Alarm, 3),
              "debug disabled -> no dump");

        std::filesystem::remove_all(dir, ec);
    }

    // 13. VisionTask debug API + config-driven debug block
    {
        const std::string dbg_yaml =
            sample +
            "debug:\n"
            "  enabled: true\n"
            "  dump_dir: tmp_task_dump\n"
            "  dump_every_n: 1\n"
            "  dump_on_alarm_only: false\n"
            "  dump_input: true\n";
        const std::string dpath = "tmp_debug.yaml";
        CHECK(write_sample(dpath, dbg_yaml), "write debug-enabled yaml");

        TaskConfig dcfg;
        uint32_t derr = 0;
        CHECK(load_task_config(dpath, dcfg, derr), "parse debug block");
        CHECK(dcfg.debug.enabled, "debug enabled parsed");
        CHECK(dcfg.debug.dump_dir == "tmp_task_dump", "dump_dir parsed");
        CHECK(dcfg.debug.dump_every_n == 1, "dump_every_n parsed");

        std::error_code ec;
        std::filesystem::remove_all("tmp_task_dump", ec);

        VisionTask vt;
        CHECK(vt.initialize(dpath) == 0u, "init with debug enabled");
        Result r{};
        for (int i = 0; i < 3; ++i) CHECK(vt.detect(f, ctx, r) == 0u, "detect (debug run)");

        CHECK(vt.debugStats().frames == 3, "3 frames counted by the task");
        CHECK(vt.debugStats().errors == 0, "no errors during those frames");

        const std::string snap = vt.debugSnapshot();
        CHECK(snap.find("\"task\":\"load_near\"") != std::string::npos, "snapshot task name");
        CHECK(snap.find("\"backend\":\"mock\"") != std::string::npos, "snapshot backend");
        CHECK(snap.find("\"frames\":3") != std::string::npos, "snapshot frame count");
        CHECK(snap.find("log") != std::string::npos, "snapshot carries recent logs");

        CHECK(vt.saveLastInput("tmp_last_input.ppm"), "cached tensor saved");
        {
            std::ifstream ppm("tmp_last_input.ppm", std::ios::binary | std::ios::ate);
            CHECK(ppm.good(), "ppm exists");
            // header "P6\n640 640\n255\n" = 15 bytes + 640*640*3 payload
            CHECK(static_cast<std::size_t>(ppm.tellg()) == 15u + 640u * 640u * 3u,
                  "ppm size matches the model input");
        }
        std::remove("tmp_last_input.ppm");

        int dumped = 0;
        for (auto& p : std::filesystem::directory_iterator("tmp_task_dump", ec)) {
            (void)p;
            ++dumped;
        }
        CHECK(dumped == 6, "3 frames -> ppm + json each");

        const std::string lib = library_snapshot(5);
        CHECK(lib.find("\"version\"") != std::string::npos, "library snapshot version");
        CHECK(lib.find("\"backend\":\"mock\"") != std::string::npos, "library snapshot backend");
        CHECK(lib.find("models") != std::string::npos, "library snapshot registry");

        vt.release();
        std::filesystem::remove_all("tmp_task_dump", ec);
        std::remove(dpath.c_str());
    }

    // 14. X86 backend + offline evaluation (precision / recall / mAP).
    //     Only meaningful when an x86-class backend is compiled in; the mock /
    //     rknn builds skip it (matching how rknn-only tests are gated).
#if defined(EVS_BACKEND_X86) || defined(EVS_BACKEND_ONNX)
    {
        // 14a. X86 reference detector finds a red blob on a synthetic input.
        evs::X86Backend xb;
        xb.configure_input(64, 48);
        CHECK(xb.load("", 0) == 0u, "x86 reference load ok");
        CHECK(xb.loaded(), "x86 backend loaded");
        CHECK(!xb.using_onnx(), "x86 running in reference (no ORT) mode");

        std::vector<uint8_t> in(64 * 48 * 3, 128); // gray background
        for (int y = 10; y < 38; ++y)
            for (int x = 16; x < 48; ++x) {
                const size_t o = (static_cast<size_t>(y) * 64 + x) * 3;
                in[o] = 230; in[o + 1] = 40; in[o + 2] = 40; // red rectangle
            }
        std::vector<float> out;
        CHECK(xb.infer(in, out) == 0u, "x86 infer ok");
        CHECK(out.size() >= 6, "x86 produced >=1 detection (flat6 = 6 floats)");
        CHECK(out[0] == 0.f, "x86 detection class 0 (red)");
        CHECK(std::fabs(out[1] - 0.9f) < 1e-3f, "x86 detection confidence 0.9");
    }

    // 14b. OfflineEvaluator on a self-contained synthetic dataset.
    {
        const std::string dset = "tmp_eval_ds";
        std::error_code ec;
        std::filesystem::remove_all(dset, ec);
        std::filesystem::create_directories(dset + "/images", ec);

        auto write_ppm = [](const std::string& p, int W, int H,
                            const std::vector<uint8_t>& px) {
            std::ofstream o(p, std::ios::binary);
            o << "P6\n" << W << " " << H << "\n255\n";
            o.write(reinterpret_cast<const char*>(px.data()),
                    static_cast<std::streamsize>(px.size()));
            return o.good();
        };
        const int W = 64, H = 48;
        std::vector<uint8_t> img1(W * H * 3, 128);
        for (int y = 10; y < 38; ++y)
            for (int x = 16; x < 48; ++x) {
                const size_t o = (static_cast<size_t>(y) * W + x) * 3;
                img1[o] = 230; img1[o + 1] = 40; img1[o + 2] = 40;
            }
        CHECK(write_ppm(dset + "/images/i1.ppm", W, H, img1), "write i1.ppm");
        std::vector<uint8_t> img2(W * H * 3, 128); // empty / negative
        CHECK(write_ppm(dset + "/images/i2.ppm", W, H, img2), "write i2.ppm");

        std::ofstream anno(dset + "/annotations.json");
        anno << "{\"class_names\":[\"person\",\"helmet\"],\"images\":["
                "{\"file\":\"i1.ppm\",\"boxes\":[{\"class\":0,\"x\":0.5,\"y\":0.5,"
                "\"w\":0.5,\"h\":0.583}]},"
                "{\"file\":\"i2.ppm\",\"boxes\":[]}]}";
        CHECK(anno.good(), "write annotations.json");
        anno.close();

        const std::string eyaml =
            "task: eval_x86\n"
            "module_id: 0x0004\n"
            "camera_ids: [0]\n"
            "backend: x86\n"
            "model: \"\"\n"
            "output_format: flat6\n"
            "input_width: 64\n"
            "input_height: 48\n"
            "input_format: rgb\n"
            "letterbox: true\n"
            "score_threshold: 0.45\n";
        const std::string eyp = dset + "/task.yaml";
        CHECK(write_sample(eyp, eyaml), "write eval task yaml");

        evs::EvalConfig ecfg;
        ecfg.task_yaml = eyp;
        ecfg.dataset_dir = dset;
        ecfg.iou_threshold = 0.5f;
        ecfg.class_names = "person,helmet";

        evs::OfflineEvaluator eval;
        CHECK(eval.load(ecfg) == 0u, "evaluator load");
        evs::EvalReport rep;
        CHECK(eval.run(rep) == 0u, "evaluator run");
        CHECK(rep.n_images == 2, "evaluator processed 2 images");
        // i1 -> 1 TP (red rect matches GT), i2 -> nothing; class 0 perfect.
        auto it = rep.per_class.find(0);
        CHECK(it != rep.per_class.end(), "class 0 present in report");
        CHECK(it->second.tp == 1, "class0 tp == 1");
        CHECK(it->second.fp == 0, "class0 fp == 0");
        CHECK(it->second.fn == 0, "class0 fn == 0");
        CHECK(std::fabs(it->second.precision - 1.0) < 1e-6, "class0 precision == 1");
        CHECK(std::fabs(it->second.recall - 1.0) < 1e-6, "class0 recall == 1");
        CHECK(std::fabs(it->second.ap - 1.0) < 1e-6, "class0 AP == 1");
        CHECK(std::fabs(rep.map - 1.0) < 1e-6, "mAP == 1 (only class 0 has GT)");
        CHECK(std::fabs(rep.micro_precision - 1.0) < 1e-6, "micro precision == 1");
        CHECK(std::fabs(rep.micro_recall - 1.0) < 1e-6, "micro recall == 1");
        // JSON report must serialize the per-class block.
        CHECK(rep.to_json().find("\"precision\":1") != std::string::npos,
              "report.json carries precision");

        std::filesystem::remove_all(dset, ec);
    }
#else
    CHECK(true, "x86 eval tests gated out for non-x86 build");
#endif

    // 15. height-limit and winch-ice skeletons, plus VisionTask wiring
    {
        auto height = create_function_plugin(0x0007);
        TaskConfig hc;
        hc.module_id = 0x0007;
        hc.start_angle_deg = 5.f;
        hc.stop_angle_deg = 75.f;
        hc.min_frames = 10;
        CHECK(height && height->configure(hc) == 0u, "height configure");
        hc.stop_angle_deg = 5.f;
        CHECK(height->configure(hc) == static_cast<uint32_t>(EvsStatus::OutOfRange),
              "height rejects inverted window");
        Result hr{};
        CHECK(height->decide(Frame{}, MachineContext{}, {}, hr) == 0u, "height decide");
        CHECK(hr.limit_state == 1, "no model box means switch not installed");
        Target seen;
        seen.class_id = 0;
        seen.confidence = 0.9f;
        seen.w = 0.2f;
        seen.h = 0.2f;
        CHECK(height->decide(Frame{}, MachineContext{}, {seen}, hr) == 0u,
              "height decide with a switch box");
        CHECK(hr.limit_state == 0, "mount stays unknown until OpenCV judges it");
        CHECK(hr.targets.size() == 1, "detected switch box is kept");
        CHECK(hr.ice_ratio == 0.f, "height does not set ice_ratio");

        auto ice = create_function_plugin(0x0008);
        TaskConfig ic;
        ic.module_id = 0x0008;
        ic.inference_enabled = false;
        ic.area_ratio = 0.3f;
        ic.exit_on_alarm = true;
#if defined(EVS_WITH_OPENCV)
        CHECK(ice && ice->configure(ic) == static_cast<uint32_t>(EvsStatus::ConfigInvalid),
              "ice requires measured ROI");
#else
        CHECK(ice && ice->configure(ic) == static_cast<uint32_t>(EvsStatus::NotSupported),
              "ice requires OpenCV");
#endif
        ic.area_ratio = 1.5f;
        CHECK(ice->configure(ic) == static_cast<uint32_t>(EvsStatus::OutOfRange),
              "ice rejects area_ratio > 1");
        Result ir{};
        ir.limit_state = 3;
        CHECK(ice->decide(Frame{}, MachineContext{}, {}, ir) != 0u && ir.status != 0u,
              "unconfigured ice returns failure");
        CHECK(ir.ice_ratio == kIceRatioUnknown, "failed ice is unknown");
        CHECK(ir.limit_state == 3, "ice does not change limit_state");

        const std::string hy =
            "task: height_limit\n"
            "module_id: 0x0007\n"
            "camera_ids: [0]\n"
            "backend: mock\n"
            "model: models/height_switch.rknn\n"
            "output_format: yolov8\n"
            "input_width: 64\n"
            "input_height: 64\n"
            "start_angle_deg: 5\n"
            "stop_angle_deg: 75\n";
        const std::string hp = "tmp_height_limit.yaml";
        CHECK(write_sample(hp, hy), "write height yaml");
        VisionTask ht;
        CHECK(ht.initialize(hp) == 0u, "height VisionTask initialize");
        std::vector<uint8_t> buf(64 * 64 * 3, 128);
        Frame hf{};
        hf.camera_id = 0;
        hf.width = 64;
        hf.height = 64;
        hf.format = PixelFormat::RGB;
        hf.data = buf.data();
        Result ho{};
        CHECK(ht.detect(hf, MachineContext{}, ho) == 0u, "height detect status 0");
        CHECK(ho.status == 0u && ho.limit_state == 1 && ho.ice_ratio == 0.f,
              "mock model has no switch box");
        const std::string bad =
            "task: height_limit\n"
            "module_id: 0x0007\n"
            "backend: mock\n"
            "model: models/height_switch.rknn\n"
            "output_format: yolov8\n"
            "start_angle_deg: 80\n"
            "stop_angle_deg: 10\n";
        const std::string bp = "tmp_height_bad.yaml";
        write_sample(bp, bad);
        CHECK(ht.updateConfig(bp) == with_module(0x0007, static_cast<uint32_t>(EvsStatus::OutOfRange)),
              "updateConfig keeps task when plugin configure fails");
        CHECK(ht.detect(hf, MachineContext{}, ho) == 0u, "detect still works after rejected update");
        ht.release();
        std::remove(hp.c_str());
        std::remove(bp.c_str());
    }

    std::cout << "\nPASS=" << g_pass << " FAIL=" << g_fail << "\n";
    return g_fail == 0 ? 0 : 1;
}

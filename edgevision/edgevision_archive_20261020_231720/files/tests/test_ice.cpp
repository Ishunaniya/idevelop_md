#include "../tools/ice_replay/host_gate.h"
#include "backend/rknn/detection_rows.h"
#include "evs/detection_decoder.h"
#include "evs/function_plugin.h"
#include "evs/log.h"
#include "evs/preprocess.h"
#include "evs/testing/ice_replay.h"
#include "evs/vision_task.h"
#include "evs/winch_ice.h"
#if defined(EVS_WITH_OPENCV)
#include "../tools/ice_replay/replay_io.h"
#include "ice_test_support.h"
#endif
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
using namespace evs;
using namespace evs::winch_ice;
static int passed = 0, failed = 0;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (x)                                                                                     \
            ++passed;                                                                              \
        else {                                                                                     \
            ++failed;                                                                              \
            std::cerr << "FAIL " << __LINE__ << ": " #x "\n";                                      \
        }                                                                                          \
    } while (0)

static bool near(float a, float b) {
    return std::abs(a - b) < .00001f;
}

static uint32_t code(EvsStatus s) {
    return with_module(8, uint32_t(s));
}

struct Image {
    int w = 200, h = 100, stride = 616;
    std::vector<uint8_t> data = std::vector<uint8_t>(stride * h, 60);

    void fill(int left, int right = 0) {
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                for (int c = 0; c < 3; ++c)
                    data[y * stride + x * 3 + c] =
                        (x < 100 ? x < left : x - 100 < right) ? 230 : 60;
    }

    ImageData image() const {
        ImageData r;
        r.data = data.data();
        r.width = w;
        r.height = h;
        r.stride = stride;
        r.size_bytes = data.size();
        r.timestamp_ms = 0;
        return r;
    }

    Frame frame() const {
        Frame f;
        f.data = data.data();
        f.width = w;
        f.height = h;
        f.stride = stride;
        f.format = PixelFormat::RGB;
        f.camera_id = 0;
        return f;
    }
};

static void host_policy() {
    using namespace ice_demo;
    HostWorkingCondition c;
    CHECK(host_decision(c, 10, 10) == HostDecision::InvalidContext);
    c.valid = true;
    c.is_working = 1;
    c.timestamp_ms = 10;
    CHECK(host_decision(c, 10, 10) == HostDecision::Detect);
    c.boom_angle = 2;
    CHECK(host_decision(c, 10, 10) == HostDecision::AngleNotZero);
    c.boom_angle = 0;
    c.is_working = 0;
    CHECK(host_decision(c, 10, 10) == HostDecision::NotWorking);
    c.is_working = 1;
    CHECK(host_decision(c, 10, 1000) == HostDecision::StaleContext);
}

#if defined(EVS_WITH_OPENCV)
// Keep artifact regression in the existing test binary, without another replay CLI.
static void artifacts_and_files() {
    using namespace ice_demo;
    const auto folder =
        std::filesystem::temp_directory_path() /
        ("evs_ice_artifacts_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto stem = (folder / "run").string();
    Image im;
    im.fill(60);
    ice_test::PixelDetector detector;
    CHECK(detector.initialize(WinchSnowConfig{}) == 0);
    WinchSnowResult result;
    CHECK(detector.detectWithBoxes(im.image(), {{0, 0, 0, 100, 100}, {1, 100, 0, 100, 100}},
                                   result) == 0);
    std::vector<testing::IceMaskSnapshot> masks;
    CHECK(detector.copyMasks(masks) && masks.size() == 2);
    artifacts(im.image(), result, stem, HostDecision::Detect, masks);
    CHECK(std::filesystem::exists(stem + "_object_0_snow.pgm") &&
          std::filesystem::exists(stem + "_object_1_snow.pgm"));
    PpmImage saved(stem + "_original.ppm");
    CHECK(saved.frame().width == 200 && saved.frame().height == 100);
    CHECK(std::filesystem::exists(stem + "_overlay.ppm"));
    {
        cv::FileStorage json(stem + "_result.json", cv::FileStorage::READ);
        CHECK(json.isOpened() && int(json["alarm"]) == 1 && json["objects"].size() == 2);
        CHECK(near(float(double(json["objects"][0]["ratio"])), .6f));
    }
    // Reusing a prefix for an empty observation must remove all old masks.
    CHECK(detector.detectWithBoxes(im.image(), {}, result) == 0);
    CHECK(detector.copyMasks(masks) && masks.empty());
    artifacts(im.image(), result, stem, HostDecision::Detect, masks);
    CHECK(!std::filesystem::exists(stem + "_object_0_snow.pgm") &&
          !std::filesystem::exists(stem + "_object_1_snow.pgm"));
    {
        cv::FileStorage json(stem + "_result.json", cv::FileStorage::READ);
        CHECK(int(json["alarm"]) == 0 && json["objects"].size() == 0 &&
              std::string(json["reason"]) == "object_not_found");
    }
    artifacts(im.image(), WinchSnowResult{}, stem, HostDecision::AngleNotZero, {});
    CHECK(!std::filesystem::exists(stem + "_overlay.ppm"));
    bool rejected = false;
    try {
        PpmImage missing((folder / "missing.ppm").string());
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    CHECK(rejected);
    std::filesystem::remove_all(folder);
}
#endif

static void output_contract() {
    std::vector<float> out;
    // A custom single-class model has five columns, not the COCO row width 84.
    std::vector<float> rows{160, 320, 320, 640, .9f, 480, 320, 320, 640, .8f};
    CHECK(rknn_detection_rows(rows.data(), rows.size(), {1, 2, 5}, "yolov8", 1, 640, 640, out));
    CHECK(out.size() == 10 && near(out[0], .25f) && near(out[2], .5f));
    std::vector<float> transposed{160, 480, 320, 320, 320, 320, 640, 640, .9f, .8f};
    std::vector<float> other;
    CHECK(rknn_detection_rows(transposed.data(), 10, {1, 5, 2}, "yolov8", 1, 640, 640, other));
    CHECK(out == other);
    CHECK(!rknn_detection_rows(rows.data(), 10, {1, 2, 5}, "yolov8", 80, 640, 640, out));
    CHECK(!rknn_detection_rows(rows.data(), 10, {1, 18, 80, 80}, "yolov8", 1, 640, 640, out));
    CHECK(!rknn_detection_rows(rows.data(), 10, {1, 2, 5}, "raw_heads", 1, 640, 640, out));
    rows[4] = 5; // logits are not probabilities
    CHECK(!rknn_detection_rows(rows.data(), 10, {1, 2, 5}, "yolov8", 1, 640, 640, out));
    rows[4] = std::numeric_limits<float>::quiet_NaN();
    CHECK(!rknn_detection_rows(rows.data(), 10, {1, 2, 5}, "yolov8", 1, 640, 640, out));
    std::vector<float> flat{0, .9f, .5f, .5f, 1, 1};
    CHECK(rknn_detection_rows(flat.data(), 6, {1, 1, 6}, "flat6", 1, 640, 640, out));
    CHECK(out == flat);
    flat[0] = .5f;
    CHECK(!rknn_detection_rows(flat.data(), 6, {1, 1, 6}, "flat6", 1, 640, 640, out));
}

static void yolox_contract() {
    std::vector<float> raw(8400 * 6, 0), out;
    const size_t anchors[] = {40 * 80 + 20, 6400 + 7 * 40 + 12, 8000 + 10 * 20 + 15};
    for (size_t row : anchors) {
        float* p = raw.data() + row * 6;
        p[0] = p[1] = .5f;
        p[2] = std::log(2.f);
        p[3] = std::log(4.f);
        p[4] = .8f;
        p[5] = .9f;
    }
    CHECK(rknn_detection_rows(raw.data(), raw.size(), {1, 8400, 6}, "yolox", 1, 640, 640, out));
    CHECK(out.size() == 8400 * 5);
    const float centers[][2] = {{164, 324}, {200, 120}, {496, 336}};
    for (size_t k = 0; k < 3; ++k) {
        const size_t base = anchors[k] * 5;
        CHECK(near(out[base], centers[k][0] / 640) && near(out[base + 1], centers[k][1] / 640));
        CHECK(near(out[base + 2], float(16 << k) / 640) &&
              near(out[base + 3], float(32 << k) / 640));
        CHECK(near(out[base + 4], .72f)); // objectness * class, no second sigmoid
    }
    CHECK(rknn_detection_rows(raw.data(), raw.size(), {8400, 6}, "yolox", 1, 640, 640, out));
    CHECK(!rknn_detection_rows(raw.data(), raw.size(), {1, 6, 8400}, "yolox", 1, 640, 640, out));
    CHECK(!rknn_detection_rows(raw.data(), raw.size(), {1, 8400, 5}, "yolox", 1, 640, 640, out));
    CHECK(!rknn_detection_rows(raw.data(), raw.size(), {1, 8399, 6}, "yolox", 1, 640, 640, out));
    CHECK(!rknn_detection_rows(raw.data(), raw.size(), {1, 8400, 6}, "yolox", 80, 640, 640, out));
    CHECK(!rknn_detection_rows(raw.data(), raw.size(), {1, 8400, 6}, "yolox", 1, 641, 640, out));
    CHECK(
        !rknn_detection_rows(raw.data(), raw.size() - 1, {1, 8400, 6}, "yolox", 1, 640, 640, out));
    const size_t base = anchors[0] * 6;
    for (size_t col : {size_t(4), size_t(5)}) {
        const float saved = raw[base + col];
        for (float bad : {-1.f, 2.f, std::numeric_limits<float>::quiet_NaN()}) {
            raw[base + col] = bad;
            CHECK(!rknn_detection_rows(raw.data(), raw.size(), {1, 8400, 6}, "yolox", 1, 640, 640,
                                       out) &&
                  out.empty());
        }
        raw[base + col] = saved;
    }
    raw[base + 2] = 1000; // malformed width must not leak inf downstream
    CHECK(!rknn_detection_rows(raw.data(), raw.size(), {1, 8400, 6}, "yolox", 1, 640, 640, out) &&
          out.empty());
    raw[base + 2] = std::numeric_limits<float>::quiet_NaN();
    CHECK(!rknn_detection_rows(raw.data(), raw.size(), {1, 8400, 6}, "yolox", 1, 640, 640, out));
}

static void config_and_errors() {
    WinchSnowConfig c;
    CHECK(load_config(EVS_SOURCE_DIR "/configs/winch_ice.yaml", c) == 0);
    CHECK(c.model_config.backend == "rknn" && c.model_config.num_classes == 1 &&
          c.model_config.winch_class_id == 0 && c.model_config.Model_path == "ice_model.rknn" &&
          c.model_config.output_format == "yolox" && c.model_config.input_width == 640 &&
          c.model_config.input_height == 640);
    Detector d;
    c.model_config.backend = "mock";
    CHECK(d.initialize(c) == code(EvsStatus::NotSupported)); // never fake a model
    Image im;
    WinchSnowResult r;
    CHECK(d.detect(im.image(), r) == code(EvsStatus::NotInitialized));
    CHECK(r.objects.empty() && !r.processed && r.alert_level == AlertLevel::ALERT_NONE);
    c.model_config.backend = "rknn";
    c.model_config.output_format = "unknown";
    CHECK(d.initialize(c) == code(EvsStatus::NotSupported));
    c.model_config.output_format = "yolox";
    c.model_config.input_width = 0;
    CHECK(d.initialize(c) == code(EvsStatus::ConfigInvalid));
    c.model_config.input_width = 640;
    c.model_config.Model_path = "/no/such/winch.rknn";
    CHECK(d.initialize(c) != 0); // SDK absent or model cannot load
    // Retired geometry/modes must fail without partially publishing a config.
    const auto temp = std::filesystem::temp_directory_path() / "evs_ice_config_slim.yaml";
    const std::vector<std::string> retired{"ice_mode: manual_boxes",
                                           "ice_mode: fixed_surface",
                                           "inference_enabled: false",
                                           "ice_surface_polygon: [0,0,1,0,1,1]",
                                           "ice_reference_width: 200",
                                           "ice_reference_height: 100",
                                           "ice_objects:\n  - id: 0\n    name: old"};
    for (const auto& extra : retired) {
        std::ofstream file(temp);
        file << "module_id: 8\nvideo_channel: [0]\n" << extra << "\n";
        file.close();
        c.camera_channel = 17;
        CHECK(load_config(temp.string(), c) != 0 && c.camera_channel == 17);
        TaskConfig before;
        before.module_id = 4;
        before.Model_path = "keep";
        uint32_t error = 0;
        CHECK(!load_task_config(temp.string(), before, error) && before.module_id == 4 &&
              before.Model_path == "keep");
    }
    {
        std::ofstream file(temp);
        file << "module_id: 8\nvideo_channel: [0]\nground:\n  enabled: false\n"
                "ice_mode: model\ninference_enabled: true\n";
    }
    CHECK(load_config(temp.string(), c) == 0); // prior model YAML remains readable
    std::filesystem::remove(temp);
    TaskConfig disabled;
    disabled.module_id = 8;
    disabled.inference_enabled = false;
    VisionTask task;
    CHECK(task.initialize(disabled) == code(EvsStatus::NotSupported));
#if defined(EVS_WITH_OPENCV)
    ice_test::PixelDetector pixels;
    CHECK(pixels.initialize(c) == 0);
    auto image = im.image();
    image.size_bytes = 1;
    CHECK(pixels.detectWithBoxes(image, {}, r) == code(EvsStatus::InvalidImageData) ||
          r.status != 0);
    image = im.image();
    image.format = ImageFormat::Gray;
    image.channels = 1;
    CHECK(pixels.detectWithBoxes(image, {}, r) == code(EvsStatus::NotSupported));
    image = im.image();
    image.data = nullptr;
    CHECK(pixels.detectWithBoxes(image, {}, r) == code(EvsStatus::NullPointer));
    CHECK(pixels.detectWithBoxes(im.image(), {}, r) == 0);
    auto cfg = c.detect_config;
    cfg.area_ratio = std::numeric_limits<float>::quiet_NaN();
    CHECK(pixels.updateConfig(cfg) == code(EvsStatus::OutOfRange));
    pixels.release();
    CHECK(pixels.detectWithBoxes(im.image(), {}, r) == code(EvsStatus::NotInitialized));
#endif
}
#if defined(EVS_WITH_OPENCV)
static void per_call() {
    Image im;
    WinchSnowConfig c;
    ice_test::PixelDetector d;
    CHECK(d.initialize(c) == 0);
    const std::vector<ice_test::Box> left{{0, 0, 0, 100, 100}};
    const std::vector<ice_test::Box> both{{1, 100, 0, 100, 100}, {0, 0, 0, 100, 100}};
    WinchSnowResult r;
    for (int n : {20, 60, 20, 50, 100, 0}) {
        im.fill(n);
        CHECK(d.detectWithBoxes(im.image(), left, r) == 0);
        CHECK(r.processed && r.objects.size() == 1 && r.objects[0].valid);
        CHECK(near(r.objects[0].ice_ratio, float(n) / 100));
        CHECK(r.objects[0].surface_pixels == 10000 &&
              r.objects[0].snow_pixels == uint64_t(n * 100));
        CHECK((r.alert_level == AlertLevel::ALERT_WARN) == (n >= 50));
        CHECK(r.objects[0].box.width == 100 && r.objects[0].box.height == 100);
    }
    im.fill(20, 80);
    CHECK(d.detectWithBoxes(im.image(), both, r) == 0);
    CHECK(r.objects.size() == 2 && near(r.objects[0].ice_ratio, .2f) &&
          near(r.objects[1].ice_ratio, .8f));
    CHECK(r.objects[0].winch_id == 0 && r.objects[1].winch_id == 1 &&
          r.alert_level == AlertLevel::ALERT_WARN);
    im.fill(80, 20);
    auto reversed = both;
    std::reverse(reversed.begin(), reversed.end());
    CHECK(d.detectWithBoxes(im.image(), reversed, r) == 0);
    CHECK(near(r.objects[0].ice_ratio, .8f) && near(r.objects[1].ice_ratio, .2f));
    CHECK(d.detectWithBoxes(im.image(), {}, r) == 0);
    CHECK(r.processed && r.objects.empty() && r.reason == Reason::ObjectNotFound &&
          r.result_code == ResultCode::RESULT_NO_RESULT && r.alert_level == AlertLevel::ALERT_NONE);
    std::vector<testing::IceMaskSnapshot> masks;
    CHECK(d.copyMasks(masks) && masks.empty());
    CHECK(d.detectWithBoxes(im.image(), both, r) == 0);
    auto bad = both;
    bad[1].x = 180;
    CHECK(d.detectWithBoxes(im.image(), bad, r) != 0);
    CHECK(r.objects.empty() && !r.processed && r.alert_level == AlertLevel::ALERT_NONE);
    CHECK(d.copyMasks(masks) && masks.empty());
    auto duplicates = left;
    duplicates.push_back(left[0]);
    CHECK(d.detectWithBoxes(im.image(), duplicates, r) == 0 && r.objects.size() == 1);
    std::vector<ice_test::Box> overlapping{{0, 0, 0, 120, 100}, {1, 100, 0, 100, 100}};
    CHECK(d.detectWithBoxes(im.image(), overlapping, r) == 0);
    CHECK(!r.objects[0].valid && !r.objects[1].valid &&
          r.objects[0].reason == Reason::OverlappingBoxes);
    auto three = both;
    three.push_back({2, 10, 10, 30, 30});
    CHECK(d.detectWithBoxes(im.image(), three, r) != 0);
    CHECK(r.reason == Reason::UnexpectedObjectCount && r.objects.empty());
    im.fill(60);
    CHECK(d.detectWithBoxes(im.image(), left, r) == 0);
    auto update = c.detect_config;
    update.area_ratio = .7f;
    CHECK(d.updateConfig(update) == 0);
    CHECK(d.detectWithBoxes(im.image(), left, r) == 0);
    CHECK(near(r.objects[0].ice_ratio, .6f) && r.alert_level == AlertLevel::ALERT_NONE);
    std::fill(im.data.begin(), im.data.end(), 0);
    CHECK(d.detectWithBoxes(im.image(), left, r) == 0);
    CHECK(!r.objects[0].valid && r.objects[0].ice_ratio == -1 &&
          r.objects[0].reason == Reason::Unobservable);
    std::fill(im.data.begin(), im.data.end(), 255);
    CHECK(d.detectWithBoxes(im.image(), left, r) == 0 && !r.objects[0].valid &&
          r.objects[0].reason == Reason::Unobservable);
    im.fill(60);
    CHECK(d.detectWithBoxes(im.image(), {{0, 0, 0, 2, 100}}, r) == 0 && !r.objects[0].valid &&
          r.objects[0].reason == Reason::InvalidSurface);
    // RGB/BGR and NV12 use the same denominator, including padded row strides.
    im.fill(60);
    auto bgr = im.image();
    bgr.format = ImageFormat::BGR;
    CHECK(d.detectWithBoxes(bgr, left, r) == 0 && near(r.objects[0].ice_ratio, .6f));
    std::vector<uint8_t> nv12(200 * 150, 128);
    for (int y = 0; y < 100; ++y)
        for (int x = 0; x < 200; ++x)
            nv12[y * 200 + x] = x < 60 ? 214 : 68;
    auto nv = im.image();
    nv.data = nv12.data();
    nv.size_bytes = nv12.size();
    nv.stride = 200;
    nv.format = ImageFormat::NV12;
    nv.channels = 1;
    CHECK(d.detectWithBoxes(nv, left, r) == 0 && near(r.objects[0].ice_ratio, .6f));
}

static void model_policy() {
    auto p = create_function_plugin(8);
    TaskConfig c;
    c.module_id = 8;
    c.video_channel = {0};
    c.ground.enabled = false;
    c.inference_enabled = true;
    c.person_class_id = 0;
    CHECK(p && p->configure(c) == 0);
    Image im;
    im.fill(60, 20);
    Result out;
    Target a;
    a.class_id = 0;
    a.confidence = .9f;
    a.x = .25f;
    a.y = .5f;
    a.w = .5f;
    a.h = 1;
    auto b = a;
    b.x = .75f;
    b.confidence = .8f;
    auto duplicate = a;
    duplicate.confidence = .7f;
    auto other = a;
    other.class_id = 1;
    MachineContext ctx; // No PLC is required in the library.
    CHECK(p->decide(im.frame(), ctx, {b, duplicate, other, a}, out) == 0);
    auto r = p->winch_result();
    CHECK(r->objects.size() == 2 && r->objects[0].winch_id == 0 && r->objects[1].winch_id == 1);
    CHECK(near(r->objects[0].ice_ratio, .6f) && near(r->objects[1].ice_ratio, .2f));
    CHECK(r->objects[0].box.confidence == .9f && out.alarm == AlarmState::Alarm);
    im.fill(20, 0);
    CHECK(p->decide(im.frame(), ctx, {a, b}, out) == 0 && out.alarm == AlarmState::None);
    CHECK(near(p->winch_result()->objects[0].ice_ratio, .2f));
    CHECK(p->decide(im.frame(), ctx, {}, out) == 0 && p->winch_result()->objects.empty());
    p->reject_call(code(EvsStatus::InferFailed), out);
    CHECK(out.status != 0 && p->winch_result()->objects.empty() && !p->winch_result()->processed);
    a.confidence = std::numeric_limits<float>::quiet_NaN();
    CHECK(p->decide(im.frame(), ctx, {a}, out) != 0 && p->winch_result()->objects.empty());
}

static void yolox_to_ice() {
    std::vector<float> raw(8400 * 6, 0), rows;
    // Two winches and one duplicate, across all three scales. The original
    // 200x100 frame becomes 640x320 plus centered vertical padding of 160.
    const size_t anchors[] = {40 * 80 + 20, 6400 + 20 * 40 + 30, 8000 + 10 * 20 + 5};
    const int gx[] = {20, 30, 5}, gy[] = {40, 20, 10}, stride[] = {8, 16, 32};
    for (size_t i = 0; i < 3; ++i) {
        float* p = raw.data() + anchors[i] * 6;
        p[0] = (i == 1 ? 480.f : 160.f) / stride[i] - gx[i];
        p[1] = 320.f / stride[i] - gy[i];
        p[2] = p[3] = std::log(320.f / stride[i]);
        p[4] = .8f;
        p[5] = i == 2 ? .7f : .9f;
    }
    // High class probability alone must not pass the confidence threshold.
    raw[4] = .1f;
    raw[5] = .99f;
    CHECK(rknn_detection_rows(raw.data(), raw.size(), {1, 8400, 6}, "yolox", 1, 640, 640, rows));
    auto decoder = create_decoder("yolox", 0, 1, 640, 640);
    CHECK(bool(decoder));
    if (!decoder)
        return;
    std::vector<Target> targets;
    decoder->decode(rows, .45f, targets);
    CHECK(targets.size() == 3);
    Image im;
    Preprocessor pre;
    PreprocessConfig pc;
    pc.input_width = pc.input_height = 640;
    CHECK(pre.configure(pc));
    std::vector<uint8_t> input;
    PreprocessMap map;
    CHECK(pre.run(im.frame(), input, map) == 0);
    for (auto& target : targets)
        map_target_to_original(map, target, im.w, im.h);
    auto plugin = create_function_plugin(8);
    TaskConfig cfg;
    cfg.module_id = 8;
    cfg.video_channel = {0};
    cfg.ground.enabled = false;
    cfg.inference_enabled = true;
    cfg.person_class_id = 0;
    CHECK(plugin && plugin->configure(cfg) == 0);
    if (!plugin)
        return;
    MachineContext ctx;
    Result result;
    for (int snow : {20, 60, 20}) {
        im.fill(snow);
        CHECK(plugin->decide(im.frame(), ctx, targets, result) == 0);
        const auto* r = plugin->winch_result();
        CHECK(r && r->objects.size() == 2);
        if (!r || r->objects.size() != 2)
            continue;
        CHECK(r->objects[0].valid && r->objects[1].valid && r->objects[0].surface_pixels == 10000 &&
              r->objects[1].surface_pixels == 10000);
        CHECK(near(r->objects[0].ice_ratio, float(snow) / 100) &&
              near(r->objects[1].ice_ratio, 0) && near(r->objects[0].box.confidence, .72f));
        CHECK((result.alarm == AlarmState::Alarm) == (snow >= 50));
    }
}
#endif
int main() {
    Logger::instance().set_level(LogLevel::Off);
    host_policy();
    output_contract();
    yolox_contract();
    config_and_errors();
#if defined(EVS_WITH_OPENCV)
    per_call();
    model_policy();
    yolox_to_ice();
    artifacts_and_files();
#endif
    std::cout << "ICE PASS=" << passed << " FAIL=" << failed << '\n';
    return failed ? 1 : 0;
}

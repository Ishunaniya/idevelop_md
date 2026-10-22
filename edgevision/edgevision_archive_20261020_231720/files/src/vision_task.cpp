#include "evs/vision_task.h"
#include "core/config_internal.h"
#include "evs/testing/ice_replay.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <map>
#include <sstream>
#include <utility>
#include <vector>

#include "evs/backend_factory.h"
#if defined(EVS_BACKEND_RKNN)
#include "evs/rknn_backend.h"
#endif
#include <cmath>
#include "evs/debug.h"
#include "evs/detection_decoder.h"
#include "evs/function_plugin.h"
#include "evs/geom.h"
#include "evs/ground_mapper.h"
#include "evs/ground_metrics.h"
#include "evs/log.h"
#include "evs/model_registry.h"
#include "evs/preprocess.h"
#include "evs/task_config.h"

namespace evs {

namespace {
using Clock = std::chrono::steady_clock;

double ms_since(const Clock::time_point& t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

using GroundMapperMap = std::map<int, std::unique_ptr<IGroundMapper>>;

uint32_t build_ground_mappers(const GroundConfig& cfg, GroundMapperMap& out) {
    out.clear();
    if (!cfg.enabled) return 0u;
    for (const CameraGroundConfig& camera : cfg.cameras) {
        if (camera.id < 0 || out.find(camera.id) != out.end()) {
            EVS_LOGE("invalid/duplicate ground camera id=%d", camera.id);
            return static_cast<uint32_t>(EvsStatus::InvalidParameter);
        }
        uint32_t map_err = 0;
        auto mapper = create_ground_mapper(camera, &map_err);
        if (!mapper) {
            EVS_LOGE("ground mapper configure failed: camera=%d type=%s status=0x%08X",
                     camera.id, camera.mapper.c_str(), map_err);
            return map_err ? map_err
                           : static_cast<uint32_t>(EvsStatus::ConfigInvalid);
        }
        out.emplace(camera.id, std::move(mapper));
    }
    return 0u;
}

void warn_missing_ground_mappers(const TaskConfig& cfg,
                                 const GroundMapperMap& mappers) {
    if (!cfg.ground.enabled) return;
    for (int camera_id : cfg.video_channel) {
        if (mappers.find(camera_id) == mappers.end()) {
            EVS_LOGW("camera %d is accepted by video_channel but has no ground mapper; "
                     "distance will stay unknown", camera_id);
        }
    }
}

std::string jstr(const std::string& v) {
    std::string out = "\"";
    for (char c : v) {
        if (c == '"' || c == '\\') { out += '\\'; out += c; }
        else out += c;
    }
    out += '"';
    return out;
}
} // anonymous namespace

struct VisionTask::Impl {
    TaskConfig cfg;
    bool initialized = false;
    std::shared_ptr<IInferBackend> backend;
    std::unique_ptr<IDetectionDecoder> decoder; // model-output parser
    std::unique_ptr<IFunctionPlugin> plugin;    // person / height / ice
    GroundMapperMap ground_mappers;             // camera id -> image/ground projection
    Preprocessor pre;                           // frame -> model input
    TaskDebug dbg;                              // counters + dump policy

    // Last preprocessed tensor; cached only while debugging is enabled.
    std::vector<uint8_t> last_input;
    int last_in_w = 0;
    int last_in_h = 0;
};

VisionTask::VisionTask() : impl_(std::make_unique<Impl>()) {}
VisionTask::~VisionTask() { release(); }

uint32_t VisionTask::initialize(std::string_view config_path) {
    if (impl_->initialized) return 0u; // idempotent: never reload / leak

    const std::string cfp(config_path);
    TaskConfig cfg;
    uint32_t err = 0;
    EVS_LOGI("initializing from %s", cfp.c_str());
    uint16_t parsed_module = cfg.module_id;
    if (!detail::load_task_config_with_module(config_path, cfg, err, parsed_module)) {
        EVS_LOGE("config parse failed: %s", cfp.c_str());
        // rollback: nothing allocated yet
        const uint32_t code = err ? err : static_cast<uint32_t>(EvsStatus::ConfigParse);
        return with_module(parsed_module, code);
    }

    return initialize(cfg);
}

uint32_t VisionTask::initialize(const TaskConfig& config) {
    if (impl_->initialized)
        return 0u;
    TaskConfig cfg = config;
    if (!cfg.inference_enabled)
        return with_module(cfg.module_id, static_cast<uint32_t>(EvsStatus::NotSupported));

    if (cfg.module_id == 8) {
        if (cfg.ground.enabled ||
            cfg.Model_path.empty() || cfg.input_width <= 0 || cfg.input_height <= 0 ||
            cfg.input_width > 8192 || cfg.input_height > 8192 ||
            cfg.decoder_classes < 1 || cfg.decoder_classes > 4096 ||
            cfg.person_class_id < 0 || cfg.person_class_id >= cfg.decoder_classes ||
            (cfg.input_format != "rgb" && cfg.input_format != "bgr"))
            return with_module(8, static_cast<uint32_t>(EvsStatus::ConfigInvalid));
        if (cfg.backend != "rknn" ||
            (cfg.output_format != "flat6" && cfg.output_format != "yolov8" &&
             cfg.output_format != "yolox"))
            return with_module(8, static_cast<uint32_t>(EvsStatus::NotSupported));
    }

    auto& reg = global_registry();
    std::shared_ptr<IInferBackend> be;
    std::unique_ptr<IDetectionDecoder> dec;
    if (!create_backend(cfg.backend)) {
        EVS_LOGE("backend '%s' is not compiled into this build", cfg.backend.c_str());
        return with_module(cfg.module_id, static_cast<uint32_t>(EvsStatus::NotSupported));
    }
    be = reg.acquire(cfg.backend, cfg.Model_path, cfg.init_timeout_s);
    if (!be) {
        EVS_LOGE("model load failed: %s", cfg.Model_path.c_str());
        return with_module(cfg.module_id, static_cast<uint32_t>(EvsStatus::ModelLoadFailed));
    }
#if defined(EVS_BACKEND_RKNN)
    if (cfg.module_id == 8) {
        const auto rc = static_cast<RknnBackend*>(be.get())->validateDetectionModel(
            cfg.output_format, cfg.decoder_classes, cfg.input_width, cfg.input_height);
        if (rc) {
            reg.release(cfg.backend, cfg.Model_path);
            return with_module(8, rc);
        }
    }
#endif
    be->configure_input(cfg.input_width, cfg.input_height);
    dec = create_decoder(cfg.output_format, cfg.person_class_id, cfg.decoder_classes,
                         cfg.input_width, cfg.input_height);
    if (!dec) {
        EVS_LOGE("unknown output_format: %s", cfg.output_format.c_str());
        reg.release(cfg.backend, cfg.Model_path);
        return with_module(cfg.module_id, static_cast<uint32_t>(EvsStatus::ModelInvalidFormat));
    }
    auto plugin = create_function_plugin(cfg.module_id);
    if (!plugin) {
        EVS_LOGE("no function plugin for module_id=0x%04X", cfg.module_id);
        if (be)
            reg.release(cfg.backend, cfg.Model_path);
        return with_module(cfg.module_id, static_cast<uint32_t>(EvsStatus::NotSupported));
    }
    if (const uint32_t pc = plugin->configure(cfg)) {
        EVS_LOGE("plugin configure failed: module_id=0x%04X status=0x%08X", cfg.module_id, pc);
        if (be)
            reg.release(cfg.backend, cfg.Model_path);
        return with_module(cfg.module_id, pc);
    }
    GroundMapperMap ground_mappers;
    if (const uint32_t ge = build_ground_mappers(cfg.ground, ground_mappers)) {
        if (be)
            reg.release(cfg.backend, cfg.Model_path);
        return with_module(cfg.module_id, ge);
    }
    warn_missing_ground_mappers(cfg, ground_mappers);

    impl_->cfg = std::move(cfg);
    impl_->backend = std::move(be);
    impl_->decoder = std::move(dec);
    impl_->plugin = std::move(plugin);
    impl_->ground_mappers = std::move(ground_mappers);

    // Configure preprocessing (frame -> model input). Invalid input dims leave
    // it unconfigured, which detect() treats as a pass-through (legacy behaviour).
    PreprocessConfig pc;
    pc.input_width = impl_->cfg.input_width;
    pc.input_height = impl_->cfg.input_height;
    pc.letterbox = impl_->cfg.letterbox;
    pc.target_format = (impl_->cfg.input_format == "bgr") ? PixelFormat::BGR
                                                           : PixelFormat::RGB;
    impl_->pre.configure(pc);
    const bool pre_ok = impl_->pre.configured();

    // Optional warmup: one dummy inference so the first real frame stays within
    // the latency budget. Failures here are non-fatal in the scaffold.
    if (impl_->cfg.warmup && pre_ok) {
        const Clock::time_point w0 = Clock::now();
        std::vector<uint8_t> dummy(
            static_cast<size_t>(impl_->cfg.input_width) *
                impl_->cfg.input_height * 3,
            0);
        std::vector<float> warm_out;
        uint32_t wr;
#if defined(EVS_BACKEND_RKNN)
        if (impl_->cfg.module_id == 8)
            wr = static_cast<RknnBackend*>(impl_->backend.get())->inferDetectionRows(
                dummy, warm_out, impl_->cfg.output_format, impl_->cfg.decoder_classes,
                impl_->cfg.input_width, impl_->cfg.input_height);
        else
#endif
            wr = impl_->backend->infer(dummy, warm_out);
        if (wr != 0u) {
            EVS_LOGW("warmup inference failed (status=0x%08X)", wr);
        } else {
            EVS_LOGI("warmup done in %.1f ms", ms_since(w0));
        }
    }

    impl_->dbg.configure(impl_->cfg.debug);
    impl_->initialized = true;
    EVS_LOGI("task ready: module=0x%04X backend=%s model=%s input=%dx%d "
             "letterbox=%d debug=%d",
             impl_->cfg.module_id, impl_->cfg.backend.c_str(),
             impl_->cfg.Model_path.c_str(), impl_->cfg.input_width,
             impl_->cfg.input_height, impl_->cfg.letterbox ? 1 : 0,
             impl_->cfg.debug.enabled ? 1 : 0);
    return 0u;
}

uint32_t VisionTask::acceptFrame(const Frame& frame) const {
    if (!impl_->initialized) return static_cast<uint32_t>(EvsStatus::NotInitialized);
    const uint16_t module = impl_->cfg.module_id;
    if (frame.data == nullptr) {
        EVS_LOGW("detect: null frame data");
        return with_module(module, static_cast<uint32_t>(EvsStatus::NullPointer));
    }
    if (frame.width <= 0 || frame.height <= 0)
        return with_module(module, static_cast<uint32_t>(EvsStatus::InvalidParameter));
    for (int id : impl_->cfg.video_channel)
        if (id == frame.camera_id) return 0u;
    EVS_LOGW("frame from camera %d ignored: not in video_channel", frame.camera_id);
    return with_module(module, static_cast<uint32_t>(EvsStatus::InvalidCameraIndex));
}

uint32_t VisionTask::detect(const Frame& frame, const MachineContext& ctx, Result& out) {
    if (impl_->cfg.module_id == 0x0008) {
        out = Result{};
        out.ice_ratio = kIceRatioUnknown;
        if (frame.camera_id >= 0 && frame.camera_id < kCameraChannelUnused)
            out.camera_channel = pack_camera_channel(static_cast<uint8_t>(frame.camera_id));
    }
    if (const uint32_t ck = acceptFrame(frame)) {
        if (impl_->cfg.module_id == 0x0008)
            return rejectWinchSnow(with_module(8, ck), out);
        return ck;
    }

    bool run = true;
    if (const auto rc = impl_->plugin->preflight(frame, ctx, out, run))
        return rc;
    if (!run)
        return 0u;

    // Optical blind is judged after preprocessing, before inference.
    // (placeholder: never blind in the scaffold build)
    bool camera_blind = false;
    if (camera_blind) {
        EVS_LOGW("camera %d blind: frame skipped", frame.camera_id);
        out = Result{};
        out.status = with_module(impl_->cfg.module_id,
                                 static_cast<uint32_t>(EvsStatus::HardwareFault));
        out.camera_blind = true;
        return out.status;
    }

    const Clock::time_point t0 = Clock::now();
    std::vector<uint8_t> input;
    PreprocessMap pre_map{};
    if (impl_->pre.configured()) {
        uint32_t pr = impl_->pre.run(frame, input, pre_map);
        if (pr != 0u) {
            pr = with_module(impl_->cfg.module_id, pr);
            EVS_LOGE("preprocess failed (status=0x%08X)", pr);
            impl_->dbg.note(0.0, 0.0, 0.0, ms_since(t0), pr, AlarmState::None,
                            0, frame.timestamp_us);
            if (impl_->cfg.module_id == 0x0008) return rejectWinchSnow(pr, out);
            return pr;
        }
    } else {
        // Legacy pass-through: identity map (no resize happened).
        pre_map.in_w = frame.width;
        pre_map.in_h = frame.height;
        pre_map.scale_x = 1.f;
        pre_map.scale_y = 1.f;
    }
    return finishDetect(frame, ctx, input, pre_map, ms_since(t0), out);
}

uint32_t VisionTask::detectPrepared(const Frame& frame, const MachineContext& ctx,
                                    std::vector<uint8_t> tensor, const PreprocessMap& map,
                                    double preprocess_ms, Result& out) {
    if (impl_->cfg.module_id == 0x0008) {
        out = Result{};
        out.ice_ratio = kIceRatioUnknown;
        if (frame.camera_id >= 0 && frame.camera_id < kCameraChannelUnused)
            out.camera_channel = pack_camera_channel(static_cast<uint8_t>(frame.camera_id));
    }
    if (const uint32_t ck = acceptFrame(frame)) {
        if (impl_->cfg.module_id == 0x0008)
            return rejectWinchSnow(with_module(8, ck), out);
        return ck;
    }
    bool run = true;
    if (const auto rc = impl_->plugin->preflight(frame, ctx, out, run))
        return rc;
    if (!run)
        return 0u;
    const std::size_t need = static_cast<std::size_t>(impl_->cfg.input_width) *
                             static_cast<std::size_t>(impl_->cfg.input_height) * 3u;
    if (tensor.size() != need || map.in_w != impl_->cfg.input_width ||
        map.in_h != impl_->cfg.input_height) {
        EVS_LOGW("detectPrepared: tensor %zu bytes, map %dx%d, model %dx%d",
                 tensor.size(), map.in_w, map.in_h, impl_->cfg.input_width,
                 impl_->cfg.input_height);
        const uint32_t rc =
            with_module(impl_->cfg.module_id, static_cast<uint32_t>(EvsStatus::InvalidImageSize));
        if (impl_->cfg.module_id == 0x0008) return rejectWinchSnow(rc, out);
        return rc;
    }
    return finishDetect(frame, ctx, tensor, map, preprocess_ms, out);
}

bool VisionTask::lastWinchSnowResult(winch_ice::WinchSnowResult& out) const {
    const auto* result = winchSnowResultView();
    if (!result)
        return false;
    out = *result;
    return true;
}

const winch_ice::WinchSnowResult* VisionTask::winchSnowResultView() const noexcept {
    return impl_->initialized && impl_->plugin ? impl_->plugin->winch_result() : nullptr;
}

uint32_t VisionTask::rejectWinchSnow(uint32_t status, Result& out) noexcept {
    if (impl_->initialized && impl_->plugin && impl_->cfg.module_id == 0x0008)
        impl_->plugin->reject_call(status, out);
    else {
        out = Result{};
        out.ice_ratio = kIceRatioUnknown;
        out.status = status;
    }
    return status;
}

bool VisionTask::copyIceMasks(std::vector<testing::IceMaskSnapshot>& out) const {
    return impl_->initialized && impl_->plugin && impl_->plugin->copy_ice_masks(out);
}

int VisionTask::inputWidth() const {
    return impl_->initialized ? impl_->cfg.input_width : 0;
}
int VisionTask::inputHeight() const {
    return impl_->initialized ? impl_->cfg.input_height : 0;
}
bool VisionTask::letterbox() const {
    return impl_->initialized && impl_->cfg.letterbox;
}

uint32_t VisionTask::finishDetect(const Frame& frame, const MachineContext& ctx,
                                  std::vector<uint8_t>& input, const PreprocessMap& pre_map,
                                  double pre_ms, Result& out) {
    const Clock::time_point t_start = Clock::now();

    // Keep the tensor only when debugging is on; copying every frame otherwise
    // would double memory traffic on the slow path.
    if (impl_->cfg.debug.enabled) {
        impl_->last_input = input;
        impl_->last_in_w = pre_map.in_w;
        impl_->last_in_h = pre_map.in_h;
    }

    const Clock::time_point t1 = Clock::now();
    std::vector<float> output;
    uint32_t r;
#if defined(EVS_BACKEND_RKNN)
    if (impl_->cfg.module_id == 8)
        r = static_cast<RknnBackend*>(impl_->backend.get())->inferDetectionRows(
            input, output, impl_->cfg.output_format, impl_->cfg.decoder_classes,
            impl_->cfg.input_width, impl_->cfg.input_height);
    else
#endif
        r = impl_->backend->infer(input, output);
    const double infer_ms = ms_since(t1);
    if (r != 0u) {
        r = with_module(impl_->cfg.module_id, r);
        EVS_LOGE("inference failed (status=0x%08X, input=%zu B)", r, input.size());
        impl_->dbg.note(pre_ms, infer_ms, 0.0, pre_ms + ms_since(t_start), r,
                        AlarmState::None, 0, frame.timestamp_us);
        if (impl_->cfg.module_id == 0x0008) return rejectWinchSnow(r, out);
        return r;
    }

    const Clock::time_point t2 = Clock::now();
    std::vector<Target> targets;
    impl_->decoder->decode(output, impl_->cfg.confidence_threshold, targets);
    // Decode targets are in 0..1 of the model input; undo letterbox/stretch
    // so the policy works in original-frame coordinates.
    for (auto& t : targets)
        map_target_to_original(pre_map, t, frame.width, frame.height);
    if (impl_->cfg.module_id == 8) {
        // Clip boxes after undoing letterbox. Padding itself is not image area.
        for (auto& t : targets) {
            if (!std::isfinite(t.x) || !std::isfinite(t.y) || !std::isfinite(t.w) ||
                !std::isfinite(t.h) || t.w < 0 || t.h < 0)
                return rejectWinchSnow(with_module(8, uint32_t(EvsStatus::ModelInvalidFormat)), out);
            const float left = std::clamp(t.x - t.w / 2, 0.f, 1.f);
            const float top = std::clamp(t.y - t.h / 2, 0.f, 1.f);
            const float right = std::clamp(t.x + t.w / 2, 0.f, 1.f);
            const float bottom = std::clamp(t.y + t.h / 2, 0.f, 1.f);
            t.x = (left + right) / 2; t.y = (top + bottom) / 2;
            t.w = right - left; t.h = bottom - top;
        }
        targets.erase(std::remove_if(targets.begin(), targets.end(), [](const auto& t) {
            return t.w <= 0 || t.h <= 0;
        }), targets.end());
    }
    for (auto& target : targets) {
        target.ground_ok = false;
        target.ground_x_m = 0.f;
        target.ground_y_m = 0.f;
    }
    const auto mapper_it = impl_->ground_mappers.find(frame.camera_id);
    if (mapper_it != impl_->ground_mappers.end()) {
        for (auto& target : targets) {
            const GroundPoint point = mapper_it->second->map(target, frame, ctx);
            target.ground_ok = point.ok;
            target.ground_x_m = point.x_m;
            target.ground_y_m = point.y_m;
        }
    }
    GroundReference reference;
    if (impl_->cfg.ground.enabled &&
        impl_->plugin && impl_->plugin->ground_reference(ctx, reference)) {
        apply_ground_metrics(reference, impl_->cfg.ground.distance_mode, targets);
    } else {
        clear_ground_metrics(targets);
    }
    const double decode_ms = ms_since(t2);
    out = Result{};
    if (impl_->cfg.module_id == 0x0008)
        out.ice_ratio = kIceRatioUnknown;
    out.status = 0u;
    out.camera_blind = false;
    if (!impl_->plugin)
        return with_module(impl_->cfg.module_id,
                           static_cast<uint32_t>(EvsStatus::NotInitialized));
    if (const uint32_t dr = impl_->plugin->decide(frame, ctx, targets, out)) {
        const uint32_t stamped = with_module(impl_->cfg.module_id, dr);
        out.status = stamped;
        return stamped;
    }
    const AlarmState alarm = out.alarm;

    out.camera_channel = pack_camera_channel(
        static_cast<uint8_t>(frame.camera_id & 0xFF));

    // Latency watchdog: the operator needs to know when we stop meeting the
    // frame budget before callbacks pile up.
    const double total_ms = pre_ms + ms_since(t_start);
    if (impl_->cfg.latency_budget_ms > 0 &&
        total_ms > static_cast<double>(impl_->cfg.latency_budget_ms)) {
        impl_->dbg.note_budget(true);
        EVS_LOGW("frame %.1f ms > budget %d ms (pre=%.1f infer=%.1f decode=%.1f)",
                 total_ms, impl_->cfg.latency_budget_ms, pre_ms, infer_ms, decode_ms);
    }

    impl_->dbg.note(pre_ms, infer_ms, decode_ms, total_ms, 0u, alarm,
                    out.targets.size(), frame.timestamp_us);
    // Dumping is bounded by the policy in DebugConfig (interval / alarm only).
    impl_->dbg.maybe_dump(frame.width, frame.height, pre_map, input, targets,
                          0u, alarm, frame.timestamp_us);
    EVS_LOGT("frame cam=%d %dx%d -> %zu targets, state=%s, %.1f ms",
             frame.camera_id, frame.width, frame.height, out.targets.size(),
             out.alarm == AlarmState::Alarm ? "alarm" : "ok", total_ms);
    return 0u;
}

uint32_t VisionTask::updateConfig(std::string_view config_path) {
    if (!impl_->initialized) return static_cast<uint32_t>(EvsStatus::NotInitialized);
    const uint16_t module = impl_->cfg.module_id;

    TaskConfig newcfg;
    uint32_t err = 0;
    if (!load_task_config(config_path, newcfg, err)) {
        EVS_LOGE("updateConfig: parse failed");
        const uint32_t code = err ? err : static_cast<uint32_t>(EvsStatus::ConfigParse);
        return with_module(module, code);
    }
    return updateConfig(newcfg);
}

uint32_t VisionTask::updateConfig(const TaskConfig& config) {
    if (!impl_->initialized)
        return static_cast<uint32_t>(EvsStatus::NotInitialized);
    const uint16_t module = impl_->cfg.module_id;
    TaskConfig newcfg = config;
    if (module == 8 && (newcfg.input_width != impl_->cfg.input_width ||
        newcfg.input_height != impl_->cfg.input_height ||
        newcfg.input_format != impl_->cfg.input_format || newcfg.letterbox != impl_->cfg.letterbox))
        return with_module(8, static_cast<uint32_t>(EvsStatus::NotSupported));
    if (newcfg.inference_enabled != impl_->cfg.inference_enabled)
        return with_module(module, static_cast<uint32_t>(EvsStatus::NotSupported));
    // Backend / model are immutable at runtime.
    if (newcfg.backend != impl_->cfg.backend || newcfg.Model_path != impl_->cfg.Model_path) {
        EVS_LOGW("updateConfig rejected: backend/model change %s -> %s",
                 impl_->cfg.backend.c_str(), newcfg.backend.c_str());
        return with_module(module, static_cast<uint32_t>(EvsStatus::NotSupported));
    }

    // Output layout is bound to the model; changing how its tensor is parsed
    // also requires a re-init, so reject it here.
    if (newcfg.output_format != impl_->cfg.output_format ||
        newcfg.decoder_classes != impl_->cfg.decoder_classes ||
        newcfg.person_class_id != impl_->cfg.person_class_id ||
        newcfg.module_id != impl_->cfg.module_id) {
        EVS_LOGW("updateConfig rejected: decoder bound to the loaded model");
        return with_module(module, static_cast<uint32_t>(EvsStatus::NotSupported));
    }

    // Build ground mapping transactionally. A bad hot update must leave the
    // current camera calibration and mapper instances untouched.
    GroundMapperMap next_ground_mappers;
    if (const uint32_t ge = build_ground_mappers(newcfg.ground, next_ground_mappers)) {
        EVS_LOGW("updateConfig rejected: ground mapper configure failed 0x%08X", ge);
        return with_module(module, ge);
    }
    warn_missing_ground_mappers(newcfg, next_ground_mappers);

    // Policy hot-update. configure() must keep the previous policy if it fails.
    if (!impl_->plugin)
        return with_module(module, static_cast<uint32_t>(EvsStatus::NotInitialized));
    if (const uint32_t pc = impl_->plugin->configure(newcfg)) {
        EVS_LOGW("updateConfig rejected: plugin configure failed 0x%08X", pc);
        return with_module(module, pc);
    }

    // Apply only the allowed, hot-updatable fields.
    impl_->cfg.on_missing_context = newcfg.on_missing_context;
    impl_->cfg.work_motion_epsilon = newcfg.work_motion_epsilon;
    impl_->cfg.zone = newcfg.zone;
    impl_->cfg.confidence_threshold = newcfg.confidence_threshold;
    impl_->cfg.nms_threshold = newcfg.nms_threshold;
    impl_->cfg.ground = newcfg.ground;
    impl_->ground_mappers.swap(next_ground_mappers);
    impl_->cfg.latency_budget_ms = newcfg.latency_budget_ms;
    impl_->cfg.start_angle_deg = newcfg.start_angle_deg;
    impl_->cfg.stop_angle_deg = newcfg.stop_angle_deg;
    impl_->cfg.min_frames = newcfg.min_frames;
    impl_->cfg.still_timeout_s = newcfg.still_timeout_s;
    impl_->cfg.basic_boom_only = newcfg.basic_boom_only;
    impl_->cfg.area_ratio = newcfg.area_ratio;
    impl_->cfg.exit_on_alarm = newcfg.exit_on_alarm;
    impl_->cfg.ice = newcfg.ice;

    // Debug settings stay hot even when everything else is frozen: a field
    // technician must be able to turn dumping on without a restart.
    impl_->cfg.debug = newcfg.debug;
    impl_->dbg.configure(impl_->cfg.debug);
    if (impl_->cfg.debug.enabled)
        EVS_LOGI("debug updated: enabled=1 dir=%s every_n=%d (frames=%llu)",
                 impl_->cfg.debug.dump_dir.c_str(), impl_->cfg.debug.dump_every_n,
                 static_cast<unsigned long long>(impl_->dbg.stats().frames));

    EVS_LOGI("updateConfig applied: score=%.2f radius=%.1f budget=%d ms",
             impl_->cfg.confidence_threshold, impl_->cfg.zone.radius_m,
             impl_->cfg.latency_budget_ms);
    return 0u;
}

void VisionTask::release() {
    if (!impl_) return;
    if (impl_->initialized && impl_->backend) {
        EVS_LOGI("releasing %s:%s (%llu frames processed)",
                 impl_->cfg.backend.c_str(), impl_->cfg.Model_path.c_str(),
                 static_cast<unsigned long long>(impl_->dbg.stats().frames));
        global_registry().release(impl_->cfg.backend, impl_->cfg.Model_path);
    }
    if (impl_->plugin) impl_->plugin->reset();
    impl_->plugin.reset();
    impl_->ground_mappers.clear();
    impl_->decoder.reset();
    impl_->initialized = false;
    impl_->backend.reset();
}

const DebugStats& VisionTask::debugStats() const { return impl_->dbg.stats(); }

std::string VisionTask::debugSnapshot() const {
    const TaskConfig& c = impl_->cfg;
    const DebugStats& s = impl_->dbg.stats();
    std::ostringstream js;
    js << "{\"initialized\":" << (impl_->initialized ? "true" : "false")
       << ",\"inference_enabled\":" << (c.inference_enabled ? "true" : "false")
       << ",\"task\":" << jstr(c.task) << ",\"backend\":" << jstr(c.backend)
       << ",\"Model_path\":" << jstr(c.Model_path) << ",\"output_format\":" << jstr(c.output_format)
       << ",\"input\":{\"w\":" << c.input_width << ",\"h\":" << c.input_height
       << ",\"format\":" << jstr(c.input_format)
       << ",\"letterbox\":" << (c.letterbox ? "true" : "false") << "}"
       << ",\"confidence_threshold\":" << c.confidence_threshold
       << ",\"latency_budget_ms\":" << c.latency_budget_ms << ",\"frames\":" << s.frames
       << ",\"stats\":" << impl_->dbg.snapshot_json();
    // Append the last few log lines so a remote snapshot carries context.
    js << ",\"log\":[";
    const auto lines = Logger::instance().tail(10);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i) js << ',';
        js << jstr(lines[i]);
    }
    js << "]}";
    return js.str();
}

void VisionTask::setDebugConfig(const DebugConfig& cfg) {
    impl_->cfg.debug = cfg;
    impl_->dbg.configure(cfg);
    EVS_LOGI("debug set: enabled=%d dir=%s every_n=%d alarm_only=%d",
             cfg.enabled ? 1 : 0, cfg.dump_dir.c_str(), cfg.dump_every_n,
             cfg.dump_on_alarm_only ? 1 : 0);
}

bool VisionTask::saveLastInput(const std::string& path) const {
    if (impl_->last_input.empty() || impl_->last_in_w <= 0 || impl_->last_in_h <= 0) {
        EVS_LOGW("saveLastInput: no tensor cached (debug is %s)",
                 impl_->cfg.debug.enabled ? "on" : "off");
        return false;
    }
    return save_rgb_ppm(impl_->last_input.data(), impl_->last_in_w,
                        impl_->last_in_h, path);
}

} // namespace evs

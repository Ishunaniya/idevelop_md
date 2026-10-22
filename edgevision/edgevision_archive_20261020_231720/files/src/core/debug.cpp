#include "evs/debug.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

#include "evs/model_registry.h"

namespace evs {

const char* library_version() { return "0.3.0-scaffold"; }

namespace {

constexpr std::size_t kHistMax = 200; // samples kept for the p95 estimate

void update_stats(TimingStats& s, double ms) {
    if (s.count == 0) {
        s.min_ms = ms;
        s.max_ms = ms;
    } else {
        if (ms < s.min_ms) s.min_ms = ms;
        if (ms > s.max_ms) s.max_ms = ms;
    }
    s.last_ms = ms;
    ++s.count;
    s.avg_ms += (ms - s.avg_ms) / static_cast<double>(s.count);
}

std::string json_string(const std::string& v) {
    std::string out = "\"";
    for (char c : v) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char b[8];
                    std::snprintf(b, sizeof(b), "\\u%04x", c);
                    out += b;
                } else {
                    out += c;
                }
        }
    }
    out += '"';
    return out;
}

std::string json_double(double v) {
    char b[32];
    std::snprintf(b, sizeof(b), "%.3f", v);
    // Trim trailing zeros for readability (still valid JSON).
    return std::string(b);
}

std::string alarm_name(AlarmState a) {
    switch (a) {
        case AlarmState::None:    return "None";
        case AlarmState::LogOnly: return "LogOnly";
        case AlarmState::Alarm:   return "Alarm";
    }
    return "?";
}

bool write_ppm(const uint8_t* rgb, int width, int height, const std::string& path) {
    if (!rgb || width <= 0 || height <= 0) return false;
    std::ofstream o(path, std::ios::binary | std::ios::trunc);
    if (!o) return false;
    char head[64];
    std::snprintf(head, sizeof(head), "P6\n%d %d\n255\n", width, height);
    o.write(head, static_cast<std::streamsize>(std::strlen(head)));
    o.write(reinterpret_cast<const char*>(rgb),
            static_cast<std::streamsize>(static_cast<size_t>(width) * height * 3));
    return static_cast<bool>(o);
}

// Minimal NV12 -> RGB (BT.601); mirrors Preprocessor's conversion so a dumped
// frame looks the same as what the model saw.
std::vector<uint8_t> nv12_to_rgb(const uint8_t* y_plane, const uint8_t* uv_plane,
                                 int stride, int width, int height) {
    std::vector<uint8_t> out(static_cast<size_t>(width) * height * 3);
    auto clip = [](int v) -> uint8_t {
        return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
    };
    for (int y = 0; y < height; ++y) {
        const uint8_t* yrow = y_plane + static_cast<size_t>(y) * stride;
        for (int x = 0; x < width; ++x) {
            const size_t yi = (static_cast<size_t>(y) / 2) * stride + (static_cast<size_t>(x) & ~1u);
            const int yy = yrow[x];
            const int u = static_cast<int>(uv_plane[yi]) - 128;
            const int v = static_cast<int>(uv_plane[yi + 1]) - 128;
            const int r = yy + ((360 * v) >> 8);
            const int g = yy - ((88 * u + 183 * v) >> 8);
            const int b = yy + ((455 * u) >> 8);
            const size_t o = (static_cast<size_t>(y) * width + x) * 3;
            out[o] = clip(r);
            out[o + 1] = clip(g);
            out[o + 2] = clip(b);
        }
    }
    return out;
}

#if defined(EVS_BACKEND_MOCK)
const char* g_backend_name = "mock";
#elif defined(EVS_BACKEND_RKNN)
const char* g_backend_name = "rknn";
#elif defined(EVS_BACKEND_QNN)
const char* g_backend_name = "qnn";
#else
const char* g_backend_name = "none";
#endif

} // anonymous namespace

void TaskDebug::configure(const DebugConfig& cfg) { cfg_ = cfg; }

void TaskDebug::reset() {
    st_ = DebugStats{};
    hist_.clear();
    seq_ = 0;
    dumps_ = 0;
}

void TaskDebug::note(double pre_ms, double infer_ms, double decode_ms, double total_ms,
                     uint32_t status, AlarmState alarm, std::size_t n_targets,
                     int64_t timestamp_us) {
    ++seq_;
    st_.frames = seq_;
    if (status != 0u) ++st_.errors;
    if (alarm == AlarmState::Alarm) ++st_.alarms;
    st_.last_status = status;
    st_.last_alarm = alarm;
    st_.last_targets = n_targets;
    st_.last_timestamp_us = timestamp_us;

    update_stats(st_.pre, pre_ms);
    update_stats(st_.infer, infer_ms);
    update_stats(st_.decode, decode_ms);
    update_stats(st_.total, total_ms);

    hist_.push_back(total_ms);
    if (hist_.size() > kHistMax) hist_.erase(hist_.begin());
    // Recompute p95 periodically; sorting every frame is wasteful on a device.
    if (hist_.size() >= 8 && (hist_.size() % 8 == 0)) {
        std::vector<double> sorted(hist_);
        std::sort(sorted.begin(), sorted.end());
        const std::size_t idx = static_cast<std::size_t>(0.95 * (sorted.size() - 1));
        st_.total.p95_ms = sorted[idx];
    }
}

bool TaskDebug::maybe_dump(int orig_w, int orig_h, const PreprocessMap& map,
                           const std::vector<uint8_t>& input,
                           const std::vector<Target>& targets, uint32_t status,
                           AlarmState alarm, int64_t timestamp_us) {
    if (!cfg_.enabled || !cfg_.dump_input) return false;
    if (cfg_.max_dumps > 0 && dumps_ >= static_cast<uint64_t>(cfg_.max_dumps)) return false;

    const bool periodic = cfg_.dump_every_n > 0 && (seq_ % static_cast<uint64_t>(cfg_.dump_every_n) == 0);
    const bool on_alarm = cfg_.dump_on_alarm_only && alarm != AlarmState::None;
    if (!periodic && !on_alarm) return false;

    const int in_w = map.in_w > 0 ? map.in_w : 0;
    const int in_h = map.in_h > 0 ? map.in_h : 0;
    if (input.empty() || in_w <= 0 || in_h <= 0) return false;

    std::error_code ec;
    std::filesystem::create_directories(cfg_.dump_dir, ec);
    if (ec) {
        EVS_LOGW("cannot create dump dir %s: %s", cfg_.dump_dir.c_str(), ec.message().c_str());
        return false;
    }

    char name[128];
    std::snprintf(name, sizeof(name), "%s/seq%06llu_%lld_%s",
                  cfg_.dump_dir.c_str(), static_cast<unsigned long long>(seq_),
                  static_cast<long long>(timestamp_us), alarm_name(alarm).c_str());
    const std::string stem(name);
    const std::string ppm = stem + ".ppm";

    if (!write_ppm(input.data(), in_w, in_h, ppm)) {
        EVS_LOGW("dump failed: %s", ppm.c_str());
        return false;
    }
    ++dumps_;

    if (cfg_.dump_meta) {
        std::ostringstream js;
        js << "{\"seq\":" << seq_
           << ",\"timestamp_us\":" << timestamp_us
           << ",\"status\":" << status
           << ",\"alarm\":" << json_string(alarm_name(alarm))
           << ",\"original\":{\"w\":" << orig_w << ",\"h\":" << orig_h << "}"
           << ",\"input\":{\"w\":" << in_w << ",\"h\":" << in_h << "}"
           << ",\"map\":{\"scale_x\":" << json_double(map.scale_x)
           << ",\"scale_y\":" << json_double(map.scale_y)
           << ",\"pad_x\":" << json_double(map.pad_x)
           << ",\"pad_y\":" << json_double(map.pad_y) << "}"
           << ",\"targets\":[";
        for (std::size_t i = 0; i < targets.size(); ++i) {
            const Target& t = targets[i];
            if (i) js << ',';
            js << "{\"class\":" << t.class_id
               << ",\"conf\":" << json_double(t.confidence)
               << ",\"x\":" << json_double(t.x) << ",\"y\":" << json_double(t.y)
               << ",\"w\":" << json_double(t.w) << ",\"h\":" << json_double(t.h)
               << ",\"ground_ok\":" << (t.ground_ok ? "true" : "false")
               << ",\"ground_x_m\":" << json_double(t.ground_x_m)
               << ",\"ground_y_m\":" << json_double(t.ground_y_m)
               << ",\"distance_m\":" << json_double(t.distance_m)
               << ",\"angle_deg\":" << json_double(t.angle_deg)
               << ",\"track\":" << t.track_id << "}";
        }
        js << "]}";
        std::ofstream m(stem + ".json", std::ios::trunc);
        if (m) m << js.str();
    }
    return true;
}

std::string TaskDebug::snapshot_json() const {
    std::ostringstream js;
    js << "{\"seq\":" << seq_
       << ",\"frames\":" << st_.frames
       << ",\"errors\":" << st_.errors
       << ",\"alarms\":" << st_.alarms
       << ",\"over_budget\":" << st_.over_budget
       << ",\"last_status\":" << st_.last_status
       << ",\"last_alarm\":" << json_string(alarm_name(st_.last_alarm))
       << ",\"last_targets\":" << st_.last_targets
       << ",\"ms\":{\"last\":" << json_double(st_.total.last_ms)
       << ",\"avg\":" << json_double(st_.total.avg_ms)
       << ",\"p95\":" << json_double(st_.total.p95_ms)
       << ",\"min\":" << json_double(st_.total.min_ms)
       << ",\"max\":" << json_double(st_.total.max_ms) << "}"
       << ",\"stages\":{\"pre\":" << json_double(st_.pre.avg_ms)
       << ",\"infer\":" << json_double(st_.infer.avg_ms)
       << ",\"decode\":" << json_double(st_.decode.avg_ms) << "}"
       << ",\"dump\":{\"enabled\":" << (cfg_.enabled ? "true" : "false")
       << ",\"written\":" << dumps_
       << ",\"dir\":" << json_string(cfg_.dump_dir) << "}}";
    return js.str();
}

bool save_rgb_ppm(const uint8_t* data, int width, int height, const std::string& path) {
    return write_ppm(data, width, height, path);
}

bool save_frame_ppm(const Frame& frame, const std::string& path) {
    if (!frame.data || frame.width <= 0 || frame.height <= 0) return false;
    const int stride = frame.stride > 0 ? frame.stride : frame.width;

    if (frame.format == PixelFormat::NV12) {
        const uint8_t* y = frame.data;
        const uint8_t* uv = frame.data + static_cast<size_t>(stride) * frame.height;
        std::vector<uint8_t> rgb = nv12_to_rgb(y, uv, stride, frame.width, frame.height);
        return write_ppm(rgb.data(), frame.width, frame.height, path);
    }
    const bool src_is_bgr = frame.format == PixelFormat::BGR;
    std::vector<uint8_t> rgb(static_cast<size_t>(frame.width) * frame.height * 3);
    for (int row = 0; row < frame.height; ++row) {
        const uint8_t* s = frame.data + static_cast<size_t>(row) * stride * 3;
        uint8_t* d = rgb.data() + static_cast<size_t>(row) * frame.width * 3;
        for (int x = 0; x < frame.width * 3; x += 3) {
            d[x] = src_is_bgr ? s[x + 2] : s[x];
            d[x + 1] = s[x + 1];
            d[x + 2] = src_is_bgr ? s[x] : s[x + 2];
        }
    }
    return write_ppm(rgb.data(), frame.width, frame.height, path);
}

std::string library_snapshot(std::size_t log_lines) {
    std::ostringstream js;
    js << "{\"version\":" << json_string(library_version())
       << ",\"backend\":" << json_string(g_backend_name)
       << ",\"log_level\":" << json_string(log_level_name(Logger::instance().level()))
       << ",\"models\":[";
    const auto entries = global_registry().snapshot();
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        if (i) js << ',';
        js << "{\"backend\":" << json_string(e.backend_type)
           << ",\"model\":" << json_string(e.model_path)
           << ",\"ref\":" << e.ref
           << ",\"loaded\":" << (e.loaded ? "true" : "false") << "}";
    }
    js << "],\"log\":[";
    const auto lines = Logger::instance().tail(log_lines);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i) js << ',';
        js << json_string(lines[i]);
    }
    js << "]}";
    return js.str();
}

} // namespace evs

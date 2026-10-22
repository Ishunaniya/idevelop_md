#include "evs/offline_eval.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "evs/backend_factory.h"
#include "evs/debug.h"   // save_rgb_ppm
#include "evs/log.h"
#include "evs/model_registry.h"
#include "evs/task_config.h"

namespace evs {

namespace {

// --- tiny JSON parser (subset: object/array/string/number/bool/null) ----------
struct JsonValue {
    enum Type { Null, Bool, Num, Str, Arr, Obj } type = Null;
    bool b = false;
    double num = 0.0;
    std::string str;
    std::vector<JsonValue> arr;
    std::vector<std::pair<std::string, JsonValue>> obj;

    const JsonValue* find(const std::string& k) const {
        if (type != Obj) return nullptr;
        for (const auto& p : obj)
            if (p.first == k) return &p.second;
        return nullptr;
    }
};

class JsonParser {
public:
    explicit JsonParser(const std::string& s) : s_(s), i_(0) {}
    bool parse(JsonValue& out) {
        skip_ws();
        return parse_value(out) && i_ >= s_.size();
    }

private:
    const std::string& s_;
    size_t i_;

    void skip_ws() {
        while (i_ < s_.size() &&
               (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\n' || s_[i_] == '\r'))
            ++i_;
    }
    bool parse_value(JsonValue& v) {
        skip_ws();
        if (i_ >= s_.size()) return false;
        char c = s_[i_];
        if (c == '{') return parse_object(v);
        if (c == '[') return parse_array(v);
        if (c == '"') { v.type = JsonValue::Str; return parse_string(v.str); }
        if (c == 't' || c == 'f') return parse_bool(v);
        if (c == 'n') { v.type = JsonValue::Null; i_ += 4; return true; }
        return parse_number(v);
    }
    bool parse_object(JsonValue& v) {
        v.type = JsonValue::Obj;
        ++i_; // '{'
        skip_ws();
        if (i_ < s_.size() && s_[i_] == '}') { ++i_; return true; }
        while (true) {
            skip_ws();
            if (i_ >= s_.size() || s_[i_] != '"') return false;
            std::string key;
            if (!parse_string(key)) return false;
            skip_ws();
            if (i_ >= s_.size() || s_[i_] != ':') return false;
            ++i_;
            JsonValue val;
            if (!parse_value(val)) return false;
            v.obj.emplace_back(std::move(key), std::move(val));
            skip_ws();
            if (i_ >= s_.size()) return false;
            if (s_[i_] == ',') { ++i_; continue; }
            if (s_[i_] == '}') { ++i_; return true; }
            return false;
        }
    }
    bool parse_array(JsonValue& v) {
        v.type = JsonValue::Arr;
        ++i_; // '['
        skip_ws();
        if (i_ < s_.size() && s_[i_] == ']') { ++i_; return true; }
        while (true) {
            JsonValue val;
            if (!parse_value(val)) return false;
            v.arr.push_back(std::move(val));
            skip_ws();
            if (i_ >= s_.size()) return false;
            if (s_[i_] == ',') { ++i_; continue; }
            if (s_[i_] == ']') { ++i_; return true; }
            return false;
        }
    }
    bool parse_string(std::string& out) {
        ++i_; // opening quote
        std::string tmp;
        while (i_ < s_.size()) {
            char c = s_[i_++];
            if (c == '"') { out = std::move(tmp); return true; }
            if (c == '\\') {
                if (i_ >= s_.size()) return false;
                char e = s_[i_++];
                switch (e) {
                    case 'n': tmp += '\n'; break;
                    case 't': tmp += '\t'; break;
                    case 'r': tmp += '\r'; break;
                    case '"': tmp += '"'; break;
                    case '\\': tmp += '\\'; break;
                    case '/': tmp += '/'; break;
                    default: tmp += e; break;
                }
            } else {
                tmp += c;
            }
        }
        return false;
    }
    bool parse_number(JsonValue& v) {
        size_t start = i_;
        while (i_ < s_.size()) {
            char c = s_[i_];
            if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' ||
                c == 'e' || c == 'E')
                ++i_;
            else break;
        }
        if (i_ == start) return false;
        v.type = JsonValue::Num;
        v.num = std::strtod(s_.substr(start, i_ - start).c_str(), nullptr);
        return true;
    }
    bool parse_bool(JsonValue& v) {
        if (s_.compare(i_, 4, "true") == 0) {
            v.type = JsonValue::Bool; v.b = true; i_ += 4; return true;
        }
        if (s_.compare(i_, 5, "false") == 0) {
            v.type = JsonValue::Bool; v.b = false; i_ += 5; return true;
        }
        return false;
    }
};

// --- PPM (P6 binary) loader ----------------------------------------------------
bool load_ppm(const std::string& path, int& w, int& h, std::vector<uint8_t>& rgb) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::string magic;
    in >> magic;
    if (magic != "P6") return false;
    in >> w >> h;
    int maxval = 255;
    in >> maxval;
    if (w <= 0 || h <= 0 || maxval <= 0) return false;
    in.ignore(1); // whitespace after maxval
    const size_t n = static_cast<size_t>(w) * h * 3;
    rgb.resize(n);
    in.read(reinterpret_cast<char*>(rgb.data()), static_cast<std::streamsize>(n));
    if (static_cast<size_t>(in.gcount()) != n) return false;
    if (maxval != 255) { // scale to 8-bit
        const double s = 255.0 / maxval;
        for (size_t k = 0; k < n; ++k) rgb[k] = static_cast<uint8_t>(rgb[k] * s);
    }
    return true;
}

// --- box IoU in 0..1 coordinates ----------------------------------------------
float box_iou(const GtBox& a, const Target& b) {
    const float ax1 = a.x - a.w / 2.f, ay1 = a.y - a.h / 2.f;
    const float ax2 = a.x + a.w / 2.f, ay2 = a.y + a.h / 2.f;
    const float bx1 = b.x - b.w / 2.f, by1 = b.y - b.h / 2.f;
    const float bx2 = b.x + b.w / 2.f, by2 = b.y + b.h / 2.f;
    const float ix1 = std::max(ax1, bx1), iy1 = std::max(ay1, by1);
    const float ix2 = std::min(ax2, bx2), iy2 = std::min(ay2, by2);
    const float iw = std::max(0.f, ix2 - ix1), ih = std::max(0.f, iy2 - iy1);
    const float inter = iw * ih;
    const float area_a = a.w * a.h, area_b = b.w * b.h;
    const float uni = area_a + area_b - inter;
    return uni > 0.f ? inter / uni : 0.f;
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

void draw_box(std::vector<uint8_t>& rgb, int w, int h, float cx, float cy,
              float bw, float bh, uint8_t r, uint8_t g, uint8_t b) {
    const int x1 = static_cast<int>((cx - bw / 2.f) * w);
    const int y1 = static_cast<int>((cy - bh / 2.f) * h);
    const int x2 = static_cast<int>((cx + bw / 2.f) * w);
    const int y2 = static_cast<int>((cy + bh / 2.f) * h);
    auto set = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= w || y >= h) return;
        const size_t o = (static_cast<size_t>(y) * w + x) * 3;
        rgb[o] = r; rgb[o + 1] = g; rgb[o + 2] = b;
    };
    for (int x = x1; x <= x2; ++x) { set(x, y1); set(x, y2); }
    for (int y = y1; y <= y2; ++y) { set(x1, y); set(x2, y); }
}

std::vector<std::string> split_names(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == ',') { out.push_back(cur); cur.clear(); }
        else if (c != ' ') cur += c;
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

} // anonymous namespace

// --- OfflineEvaluator ----------------------------------------------------------
uint32_t OfflineEvaluator::load(const EvalConfig& cfg) {
    cfg_ = cfg;
    uint32_t err = 0;
    if (!load_task_config(cfg.task_yaml, task_, err)) {
        EVS_LOGE("eval: cannot parse task yaml %s", cfg.task_yaml.c_str());
        return err ? err : static_cast<uint32_t>(EvsStatus::InvalidParameter);
    }
    // Resolve the model path relative to the dataset dir if it is not absolute.
    if (!task_.Model_path.empty() && task_.Model_path.find(':') == std::string::npos) {
        std::error_code ec;
        if (!std::filesystem::exists(task_.Model_path, ec)) {
            const auto rel =
                std::filesystem::path(cfg.dataset_dir) / task_.Model_path;
            if (std::filesystem::exists(rel, ec)) task_.Model_path = rel.string();
        }
    }

    backend_ = create_backend(task_.backend);
    if (!backend_) {
        EVS_LOGE("eval: backend '%s' not compiled into this build",
                 task_.backend.c_str());
        return static_cast<uint32_t>(EvsStatus::NotSupported);
    }
    if (backend_->load(task_.Model_path, task_.init_timeout_s) != 0u) {
        EVS_LOGE("eval: backend load failed (%s)", task_.backend.c_str());
        return static_cast<uint32_t>(EvsStatus::NotInitialized);
    }
    backend_->configure_input(task_.input_width, task_.input_height);

    dec_ = create_decoder(task_.output_format, task_.person_class_id,
                          task_.decoder_classes, task_.input_width, task_.input_height);
    if (!dec_) {
        EVS_LOGE("eval: unknown output_format '%s'", task_.output_format.c_str());
        return static_cast<uint32_t>(EvsStatus::NotSupported);
    }

    PreprocessConfig pc;
    pc.input_width = task_.input_width;
    pc.input_height = task_.input_height;
    pc.letterbox = task_.letterbox;
    pc.target_format =
        (task_.input_format == "bgr") ? PixelFormat::BGR : PixelFormat::RGB;
    pre_.configure(pc);
    return 0u;
}

uint32_t OfflineEvaluator::run(EvalReport& out) {
    out = EvalReport{};
    out.backend = task_.backend;
    out.Model_path = task_.Model_path;
    out.iou_threshold = cfg_.iou_threshold;
    out.confidence_threshold = (cfg_.confidence_threshold >= 0.f)
                                  ? cfg_.confidence_threshold
                                  : task_.confidence_threshold;
    out.class_names_ = cfg_.class_names;

    // Load annotations.
    const auto anno_path =
        std::filesystem::path(cfg_.dataset_dir) / cfg_.anno_file;
    std::ifstream ain(anno_path);
    if (!ain) {
        EVS_LOGE("eval: annotation file not found: %s", anno_path.string().c_str());
        return static_cast<uint32_t>(EvsStatus::InvalidParameter);
    }
    std::stringstream ss;
    ss << ain.rdbuf();
    JsonValue root;
    if (!JsonParser(ss.str()).parse(root) || root.type != JsonValue::Obj) {
        EVS_LOGE("eval: failed to parse annotations JSON");
        return static_cast<uint32_t>(EvsStatus::InvalidParameter);
    }

    std::vector<EvalImageEntry> images;
    const JsonValue* imgs = root.find("images");
    if (imgs && imgs->type == JsonValue::Arr) {
        for (const auto& it : imgs->arr) {
            if (it.type != JsonValue::Obj) continue;
            const JsonValue* f = it.find("file");
            if (!f || f->type != JsonValue::Str) continue;
            EvalImageEntry e;
            e.file = f->str;
            const JsonValue* bx = it.find("boxes");
            if (bx && bx->type == JsonValue::Arr) {
                for (const auto& b : bx->arr) {
                    if (b.type != JsonValue::Obj) continue;
                    GtBox g;
                    if (auto* p = b.find("class")) g.cls = static_cast<int>(p->num);
                    if (auto* p = b.find("x")) g.x = static_cast<float>(p->num);
                    if (auto* p = b.find("y")) g.y = static_cast<float>(p->num);
                    if (auto* p = b.find("w")) g.w = static_cast<float>(p->num);
                    if (auto* p = b.find("h")) g.h = static_cast<float>(p->num);
                    e.boxes.push_back(g);
                }
            }
            images.push_back(std::move(e));
        }
    }

    const std::filesystem::path img_dir =
        std::filesystem::path(cfg_.dataset_dir) / cfg_.images_subdir;

    struct PredRec { float conf; int cls; bool tp; };
    std::vector<PredRec> all_preds;
    std::map<int, int> gt_count;  // per class total GT
    std::map<int, int> tp_count;  // per class TP
    std::map<int, int> fp_count;  // per class FP

    std::error_code ec;
    if (cfg_.dump_mismatch) std::filesystem::create_directories(cfg_.dump_dir, ec);

    int total_tp = 0, total_fp = 0, total_fn = 0;

    for (const auto& e : images) {
        int iw = 0, ih = 0;
        std::vector<uint8_t> rgb;
        const auto img_path = img_dir / e.file;
        const bool ok = load_ppm(img_path.string(), iw, ih, rgb);
        ImageEvalResult ir;
        ir.file = e.file;
        if (!ok) {
            EVS_LOGW("eval: cannot load image %s (skipped)",
                     img_path.string().c_str());
            ir.fn = static_cast<int>(e.boxes.size());
            out.images.push_back(ir);
            continue;
        }

        // Build the frame and run the same pipeline as the device.
        Frame frame;
        frame.camera_id = 0;
        frame.width = iw;
        frame.height = ih;
        frame.stride = iw * 3;
        frame.format = PixelFormat::RGB;
        frame.data = rgb.data();

        std::vector<uint8_t> input;
        PreprocessMap pmap{};
        const uint32_t pr = pre_.run(frame, input, pmap);
        if (pr != 0u) {
            EVS_LOGW("eval: preprocess failed for %s", e.file.c_str());
            ir.fn = static_cast<int>(e.boxes.size());
            out.images.push_back(ir);
            continue;
        }
        std::vector<float> out_tensor;
        if (backend_->infer(input, out_tensor) != 0u) {
            EVS_LOGW("eval: infer failed for %s", e.file.c_str());
            ir.fn = static_cast<int>(e.boxes.size());
            out.images.push_back(ir);
            continue;
        }
        std::vector<Target> targets;
        dec_->decode(out_tensor, out.confidence_threshold, targets);
        for (auto& t : targets)
            map_target_to_original(pmap, t, iw, ih);

        // Group ground truth by class.
        std::map<int, std::vector<int>> gt_by_cls;
        for (size_t gi = 0; gi < e.boxes.size(); ++gi)
            gt_by_cls[e.boxes[gi].cls].push_back(static_cast<int>(gi));
        std::map<int, std::vector<char>> gt_matched;
        for (auto& kv : gt_by_cls) gt_matched[kv.first].assign(kv.second.size(), 0);

        // Sort predictions by confidence (desc) for greedy matching.
        std::vector<int> order(targets.size());
        for (size_t k = 0; k < targets.size(); ++k) order[k] = static_cast<int>(k);
        std::sort(order.begin(), order.end(), [&](int a, int b) {
            return targets[a].confidence > targets[b].confidence;
        });

        int img_tp = 0, img_fp = 0;
        for (int pi : order) {
            const Target& t = targets[pi];
            const int cls = t.class_id;
            gt_count[cls] += 0; // ensure key exists
            bool matched = false;
            auto git = gt_by_cls.find(cls);
            if (git != gt_by_cls.end()) {
                auto& gm = gt_matched[cls];
                float best_iou = cfg_.iou_threshold;
                int best_g = -1;
                for (size_t gi = 0; gi < git->second.size(); ++gi) {
                    if (gm[gi]) continue;
                    const float iou = box_iou(e.boxes[git->second[gi]], t);
                    if (iou >= best_iou) { best_iou = iou; best_g = static_cast<int>(gi); }
                }
                if (best_g >= 0) {
                    gm[best_g] = 1;
                    matched = true;
                }
            }
            if (matched) {
                ++img_tp; ++tp_count[cls];
                all_preds.push_back({t.confidence, cls, true});
            } else {
                ++img_fp; ++fp_count[cls];
                all_preds.push_back({t.confidence, cls, false});
            }
        }
        int img_fn = 0;
        for (auto& kv : gt_by_cls) {
            const int total = static_cast<int>(kv.second.size());
            int matched_g = 0;
            for (char m : gt_matched[kv.first]) matched_g += m;
            const int fn = total - matched_g;
            img_fn += fn;
            gt_count[kv.first] += total;
        }

        total_tp += img_tp; total_fp += img_fp; total_fn += img_fn;
        ir.tp = img_tp; ir.fp = img_fp; ir.fn = img_fn;
        out.images.push_back(ir);

        // Optional visual dump of mismatch images.
        if (cfg_.dump_mismatch && (img_fp > 0 || img_fn > 0)) {
            std::vector<uint8_t> vis = rgb;
            for (const auto& t : targets)
                draw_box(vis, iw, ih, t.x, t.y, t.w, t.h, 255, 0, 0); // pred red
            for (const auto& g : e.boxes)
                draw_box(vis, iw, ih, g.x, g.y, g.w, g.h, 0, 255, 0); // gt green
            const auto dump_path =
                std::filesystem::path(cfg_.dump_dir) /
                (std::filesystem::path(e.file).stem().string() + "_cmp.ppm");
            save_rgb_ppm(vis.data(), iw, ih, dump_path.string());
        }
    }

    out.n_images = static_cast<int>(images.size());
    out.total_tp = total_tp; out.total_fp = total_fp; out.total_fn = total_fn;
    out.micro_precision = (total_tp + total_fp) > 0
                             ? total_tp / double(total_tp + total_fp) : 0.0;
    out.micro_recall = (total_tp + total_fn) > 0
                          ? total_tp / double(total_tp + total_fn) : 0.0;

    // Per-class metrics + AP@IoU.
    double ap_sum = 0.0;
    int ap_classes = 0;
    for (auto& kv : gt_count) {
        const int cls = kv.first;
        ClassMetric m;
        m.tp = tp_count[cls];
        m.fp = fp_count[cls];
        m.fn = kv.second - m.tp;
        m.precision = (m.tp + m.fp) > 0 ? m.tp / double(m.tp + m.fp) : 0.0;
        m.recall = (m.tp + m.fn) > 0 ? m.tp / double(m.tp + m.fn) : 0.0;
        m.f1 = (m.precision + m.recall) > 0
                   ? 2.0 * m.precision * m.recall / (m.precision + m.recall)
                   : 0.0;

        // AP (VOC all-point, right-is-max interpolation) at the IoU threshold.
        std::vector<PredRec> cls_preds;
        for (const auto& p : all_preds)
            if (p.cls == cls) cls_preds.push_back(p);
        std::sort(cls_preds.begin(), cls_preds.end(),
                  [](const PredRec& a, const PredRec& b) {
                      return a.conf > b.conf;
                  });
        const int npos = kv.second; // total GT of this class
        double ap = 0.0;
        if (npos > 0) {
            std::vector<double> prec, rec;
            int cum_tp = 0, cum_fp = 0;
            for (const auto& p : cls_preds) {
                if (p.tp) ++cum_tp; else ++cum_fp;
                prec.push_back(cum_tp / double(cum_tp + cum_fp));
                rec.push_back(cum_tp / double(npos));
            }
            // interpolate: for each recall level use max precision to its right
            for (size_t i = 0; i < rec.size(); ++i) {
                double pmax = prec[i];
                for (size_t j = i; j < rec.size(); ++j) pmax = std::max(pmax, prec[j]);
                const double rcur = rec[i];
                const double rprev = (i == 0) ? 0.0 : rec[i - 1];
                ap += (rcur - rprev) * pmax;
            }
        }
        m.ap = ap;
        out.per_class[cls] = m;
        ap_sum += ap;
        ++ap_classes;
    }
    out.map = ap_classes > 0 ? ap_sum / ap_classes : 0.0;
    return 0u;
}

std::string EvalReport::to_json() const {
    const auto names = split_names(class_names_safe());
    std::ostringstream js;
    js << "{\"backend\":" << jstr(backend)
       << ",\"Model_path\":" << jstr(Model_path)
       << ",\"iou_threshold\":" << iou_threshold
       << ",\"confidence_threshold\":" << confidence_threshold
       << ",\"n_images\":" << n_images
       << ",\"map\":" << map
       << ",\"micro_precision\":" << micro_precision
       << ",\"micro_recall\":" << micro_recall
       << ",\"per_class\":{";
    bool first = true;
    for (const auto& kv : per_class) {
        if (!first) js << ',';
        first = false;
        const int cls = kv.first;
        const std::string cname =
            (cls >= 0 && cls < static_cast<int>(names.size()))
                ? names[cls] : std::to_string(cls);
        js << jstr(std::to_string(cls) + ":" + cname) << ":{"
           << "\"tp\":" << kv.second.tp << ",\"fp\":" << kv.second.fp
           << ",\"fn\":" << kv.second.fn
           << ",\"precision\":" << kv.second.precision
           << ",\"recall\":" << kv.second.recall
           << ",\"f1\":" << kv.second.f1
           << ",\"ap\":" << kv.second.ap << "}";
    }
    js << "},\"images\":[";
    first = true;
    for (const auto& im : images) {
        if (!first) js << ',';
        first = false;
        js << "{\"file\":" << jstr(im.file)
           << ",\"tp\":" << im.tp << ",\"fp\":" << im.fp << ",\"fn\":" << im.fn
           << "}";
    }
    js << "]}";
    return js.str();
}

bool EvalReport::write_csv(const std::string& path) const {
    std::ofstream o(path);
    if (!o) return false;
    o << "class,name,tp,fp,fn,precision,recall,f1,ap\n";
    const auto names = split_names(class_names_safe());
    for (const auto& kv : per_class) {
        const std::string cname =
            (kv.first >= 0 && kv.first < static_cast<int>(names.size()))
                ? names[kv.first] : std::to_string(kv.first);
        o << kv.first << "," << cname << "," << kv.second.tp << ","
          << kv.second.fp << "," << kv.second.fn << ","
          << kv.second.precision << "," << kv.second.recall << ","
          << kv.second.f1 << "," << kv.second.ap << "\n";
    }
    o << "ALL,MICRO," << total_tp << "," << total_fp << "," << total_fn << ","
      << micro_precision << "," << micro_recall << "," << (2.0 * micro_precision * micro_recall /
          (micro_precision + micro_recall + 1e-9)) << "," << map << "\n";
    return true;
}

bool EvalReport::write_images_csv(const std::string& path) const {
    std::ofstream o(path);
    if (!o) return false;
    o << "file,tp,fp,fn\n";
    for (const auto& im : images)
        o << im.file << "," << im.tp << "," << im.fp << "," << im.fn << "\n";
    return true;
}

} // namespace evs

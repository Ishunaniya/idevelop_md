// Internal (non-installed) helper shared by the host backends that consume raw
// YOLOv5 detection heads: the x86/ONNXRuntime backend and the RKNN (NPU)
// backend. Keeping one copy is deliberate -- the two paths must produce
// bit-identical boxes, otherwise an offline evaluation on the PC and a field
// test on the board would disagree for no visible reason.
//
// Layout of a standard Ultralytics YOLOv5 export head:
//   [1, anchors*(5 + num_classes), S, S]   channel-major, strides 8/16/32
// i.e. channel `ch` of anchor `a` lives at ((a*(5+C) + ch) * S + row) * S + col.
// Getting this wrong (writing base[4] instead of base[4*S*S]) silently reads the
// neighbouring grid cell instead of the next channel.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>

namespace evs {
namespace yolo {

// YOLOv5s COCO model. A custom-trained model must use its own class count /
// anchors / input size; those become task-config fields when we need them.
constexpr int kClasses = 80;
constexpr float kInputSize = 640.f;

// Candidate score kept before NMS. Deliberately low: the real cut happens in the
// decoder (score_threshold), but NMS is O(n^2) so the candidate list must stay
// small -- on a real image only ~150 anchors pass 0.05, but a flat/garbage frame
// can push all 25200 grid cells through.
constexpr float kCandidateScore = 0.05f;
constexpr size_t kMaxPreNms = 3000;
constexpr float kDefaultNmsIou = 0.45f;
// Largest plausible box side, in normalised (0..1) coordinates. Anything bigger
// is a decode artefact, not a detection.
constexpr float kMaxBoxSize = 1.2f;

struct Det {
    float cx, cy, w, h, conf;
    int cls;
};

inline float sigmoid(float x) { return 1.f / (1.f + std::exp(-x)); }

// Standard YOLOv5 anchors per stride, already expressed in input pixels.
inline const std::array<std::array<int, 2>, 3>& anchors_for_stride(int stride) {
    static const std::array<std::array<int, 2>, 3> a8 = {
        {{10, 13}, {16, 30}, {33, 23}}};
    static const std::array<std::array<int, 2>, 3> a16 = {
        {{30, 61}, {62, 45}, {59, 119}}};
    static const std::array<std::array<int, 2>, 3> a32 = {
        {{116, 90}, {156, 198}, {373, 326}}};
    if (stride <= 8) return a8;
    if (stride <= 16) return a16;
    return a32;
}

// Activation mode of a head's channels.
//   kRaw   : model returns raw logits (the standard ONNX / PyTorch export) --
//            objectness, class scores and box terms all need sigmoid/exp.
//   kClsAct: the converter fused sigmoid on the objectness/class channels only,
//            so those are already probabilities; box channels are still logits.
//   kAllAct: the converter fused the whole Detect post-processing (box coords
//            exported as sigmoid(2*logit - 0.5 + grid) and sizes as
//            (2*sigmoid(logit))^2 * anchor), i.e. every channel is a
//            ready-to-use number. Rockchip RKNN conversions frequently do this,
//            which is why the mode is selectable instead of hard-coded.
enum class HeadMode { kRaw = 0, kClsAct = 1, kAllAct = 2, kAuto = 3 };

// Pick the activation mode from the value distribution: a raw-logits head has
// almost nothing inside [0,1] (measured: ~2% on a real frame, mean -5.6),
// while a head with the Detect post-processing fused in is ~100% inside. Set
// EVS_YOLO_HEAD_MODE=0|1|2 to force a mode (1 = "sigmoid on objectness/class
// only" cannot be told apart from 0/2 by distribution alone).
inline HeadMode detect_head_mode(const float* data, size_t n) {
    static const int forced = [] {
        const char* e = std::getenv("EVS_YOLO_HEAD_MODE");
        return e ? std::atoi(e) : -1;
    }();
    if (forced >= 0) return static_cast<HeadMode>(forced);
    if (!data || n == 0) return HeadMode::kRaw;
    const size_t step = n / 4096 + 1;
    size_t inside = 0, count = 0;
    for (size_t i = 0; i < n; i += step) {
        const float v = data[i];
        if (v >= 0.f && v <= 1.f) ++inside;
        ++count;
    }
    if (!count) return HeadMode::kRaw;
    return (static_cast<double>(inside) / static_cast<double>(count) >= 0.9)
               ? HeadMode::kAllAct
               : HeadMode::kRaw;
}

// Decode one head. Only the best class per anchor is kept (single-label export
// semantics); emitting one proposal per class would multiply the NMS input by 80.
inline void decode_head(const float* data, int S, int stride,
                        std::vector<Det>& dets, HeadMode mode = HeadMode::kRaw) {
    const int C = 5 + kClasses;
    const auto& anc = anchors_for_stride(stride);
    const float scale = 1.f / kInputSize;
    const long long plane = static_cast<long long>(S) * S;
    const bool cls_act = (mode != HeadMode::kRaw);
    const bool box_act = (mode == HeadMode::kAllAct);
    for (int a = 0; a < 3; ++a) {
        const float aw = static_cast<float>(anc[a][0]);
        const float ah = static_cast<float>(anc[a][1]);
        for (int y = 0; y < S; ++y) {
            for (int x = 0; x < S; ++x) {
                const float* base =
                    data + static_cast<long long>(a) * C * plane + (y * S + x);
                const float obj =
                    cls_act ? base[4 * plane] : sigmoid(base[4 * plane]);
                if (obj < kCandidateScore) continue;
                float best_conf = 0.f;
                int best_cls = -1;
                for (int c = 0; c < kClasses; ++c) {
                    const float s = base[(5 + c) * plane];
                    const float conf = obj * (cls_act ? s : sigmoid(s));
                    if (conf > best_conf) {
                        best_conf = conf;
                        best_cls = c;
                    }
                }
                if (best_cls < 0 || best_conf < kCandidateScore) continue;
                float bx, by, bw, bh;
                if (box_act) {
                    bx = (2.f * base[0] - 0.5f + x) * stride * scale;
                    by = (2.f * base[1 * plane] - 0.5f + y) * stride * scale;
                    bw = (2.f * base[2 * plane]) * (2.f * base[2 * plane]) * aw * scale;
                    bh = (2.f * base[3 * plane]) * (2.f * base[3 * plane]) * ah * scale;
                } else {
                    bx = (sigmoid(base[0]) + x) * stride * scale;
                    by = (sigmoid(base[1 * plane]) + y) * stride * scale;
                    bw = std::exp(base[2 * plane]) * aw * scale;
                    bh = std::exp(base[3 * plane]) * ah * scale;
                }
                // Sanity gate. With raw logits the size terms are exp(tw)*anchor,
                // which blows up into multi-image-sized boxes whenever tw is a
                // large positive outlier; those degenerate boxes survive NMS and
                // wreck precision. A model with fused post-processing cannot
                // produce them (size is bounded by anchor^2), so filtering here
                // keeps the two paths comparable.
                if (!(bw > 0.f && bh > 0.f) || bw > kMaxBoxSize ||
                    bh > kMaxBoxSize)
                    continue;
                dets.push_back({bx, by, bw, bh, best_conf, best_cls});
            }
        }
    }
}

inline float iou(const Det& a, const Det& b) {
    const float ax1 = a.cx - a.w / 2.f, ay1 = a.cy - a.h / 2.f;
    const float ax2 = a.cx + a.w / 2.f, ay2 = a.cy + a.h / 2.f;
    const float bx1 = b.cx - b.w / 2.f, by1 = b.cy - b.h / 2.f;
    const float bx2 = b.cx + b.w / 2.f, by2 = b.cy + b.h / 2.f;
    const float ix1 = std::max(ax1, bx1), iy1 = std::max(ay1, by1);
    const float ix2 = std::min(ax2, bx2), iy2 = std::min(ay2, by2);
    const float iw = std::max(0.f, ix2 - ix1), ih = std::max(0.f, iy2 - iy1);
    const float inter = iw * ih;
    const float uni = a.w * a.h + b.w * b.h - inter;
    return uni > 0.f ? inter / uni : 0.f;
}

// Greedy per-class NMS with a confidence top-k cut before the O(n^2) loop.
inline void nms(std::vector<Det>& dets, float iou_thr, size_t max_pre,
                bool per_class = true) {
    std::sort(dets.begin(), dets.end(),
              [](const Det& a, const Det& b) { return a.conf > b.conf; });
    if (dets.size() > max_pre) dets.resize(max_pre);
    std::vector<bool> keep(dets.size(), true);
    for (size_t i = 0; i < dets.size(); ++i) {
        if (!keep[i]) continue;
        for (size_t j = i + 1; j < dets.size(); ++j) {
            if (!keep[j]) continue;
            if (per_class && dets[j].cls != dets[i].cls) continue;
            if (iou(dets[i], dets[j]) >= iou_thr) keep[j] = false;
        }
    }
    std::vector<Det> kept;
    kept.reserve(dets.size());
    for (size_t i = 0; i < dets.size(); ++i)
        if (keep[i]) kept.push_back(dets[i]);
    dets.swap(kept);
}

// Emit "yolov8" rows consumed by YoloV8Decoder: [x, y, w, h, s0..s79] in 0..1,
// with only the winning class slot carrying the score.
inline void emit_rows(const std::vector<Det>& dets, std::vector<float>& out) {
    out.clear();
    out.reserve(dets.size() * (4 + kClasses));
    for (const auto& d : dets) {
        out.push_back(d.cx);
        out.push_back(d.cy);
        out.push_back(d.w);
        out.push_back(d.h);
        for (int c = 0; c < kClasses; ++c)
            out.push_back(c == d.cls ? d.conf : 0.f);
    }
}

// One detected head: raw float pointer plus its tensor shape as reported by the
// runtime ([1, 255, S, S]).
struct Head {
    const float* data = nullptr;
    int n_dims = 0;
    int d0 = 0, d1 = 0, d2 = 0, d3 = 0;
    size_t elems = 0;
    HeadMode mode = HeadMode::kAuto;
};

// ---------------------------------------------------------------------------
// YOLOv11 (Ultralytics v11) decoupled head.
//
// Unlike YOLOv5/v8 the v11 export carries no anchors and splits every scale into
// three separate output tensors (9 outputs for a 640x640 model):
//   reg : [1, 4*16, S, S]  four box edges x 16 DFL bins, channel-major, i.e.
//         bin b of edge q sits at ((q*16 + b) * S + row) * S + col
//   cls : [1, NC,   S, S]  class logits -- sigmoid gives the probability
//   obj : [1, 1,    S, S]  objectness logit (absent in some exports)
// An edge is a distance from the cell centre (l/t/r/b) measured in input pixels
// after multiplying the DFL expectation by the stride, so
//   x1 = cx - l, y1 = cy - t, x2 = cx + r, y2 = cy + b
//   cx = (col + 0.5) * stride, cy = (row + 0.5) * stride
//   score = sigmoid(obj) * sigmoid(cls[best])
// Ported from X:\apps\yolov5_object_detect\src\yolov11_postprocess.cpp; the
// board's yolo11.rknn is byte-identical to that app's model, so the tensor
// order / grids (80, 40, 20 for a 640 input) are guaranteed to match.
// ---------------------------------------------------------------------------
constexpr int kDflBins = 16;

// softmax over `bins` logits followed by the expectation sum(p_i * i).
inline float dfl_expectation(const float* v, int bins) {
    float maxv = -1e30f;
    for (int i = 0; i < bins; ++i)
        if (v[i] > maxv) maxv = v[i];
    float sum = 0.f, acc = 0.f;
    for (int i = 0; i < bins; ++i) {
        const float e = std::exp(v[i] - maxv);
        sum += e;
        acc += e * static_cast<float>(i);
    }
    return (sum > 0.f) ? acc / sum : 0.f;
}

// One detected scale of a decoupled head.
struct V11Scale {
    const float* reg = nullptr;
    const float* cls = nullptr;
    const float* obj = nullptr;  // may stay null for objectness-free exports
    int H = 0, W = 0, stride = 0;
};

// Group the flat output list into (reg, cls, obj) triples, one per scale.
// Returns false when the graph is not a v11 decoupled head.
inline bool collect_v11_scales(const std::vector<Head>& heads,
                               std::vector<V11Scale>& scales) {
    scales.clear();
    for (const auto& h : heads) {
        if (h.data == nullptr || h.n_dims != 4) continue;
        if (h.d0 != 1 || h.d2 <= 0 || h.d3 != h.d2) continue;
        if (h.d1 != 4 * kDflBins) continue;  // the reg tensor anchors a scale
        V11Scale s;
        s.reg = h.data;
        s.H = h.d2;
        s.W = h.d3;
        s.stride = static_cast<int>(kInputSize) / s.H;
        if (s.stride <= 0) continue;
        for (const auto& g : heads) {
            if (g.data == nullptr || g.n_dims != 4) continue;
            if (g.d2 != s.H || g.d3 != s.W) continue;
            if (g.d1 == kClasses && kClasses != 1 && !s.cls)
                s.cls = g.data;
            else if (g.d1 == 1 && !s.obj)
                s.obj = g.data;
        }
        if (s.cls) scales.push_back(s);
    }
    return !scales.empty();
}

// The RKNN export of yolo11.rknn fuses the head sigmoids into the graph: cls
// and obj arrive as probabilities (measured on a real frame: cls in
// [0.0018, 0.88], obj in [0.146, 0.69], everything inside [0,1]). Applying
// sigmoid again squashes every score towards 0.5 and floods NMS with junk.
// Exports that keep raw logits can restore the old behaviour with
// EVS_YOLO11_SIGMOID=1.
inline bool v11_fused_activation() {
    static const bool fused = [] {
        const char* e = std::getenv("EVS_YOLO11_SIGMOID");
        return !(e && e[0] == '1');
    }();
    return fused;
}

inline void decode_v11_scale(const V11Scale& s, int nc, float thr,
                             std::vector<Det>& dets) {
    const int H = s.H, W = s.W;
    const float in_w = static_cast<float>(s.stride * W);
    const float in_h = static_cast<float>(s.stride * H);
    const long long plane = static_cast<long long>(H) * W;
    float bins[kDflBins];
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const long long idx = static_cast<long long>(y) * W + x;
            const float obj_raw = s.obj ? s.obj[idx] : 1.f;
            const float obj =
                v11_fused_activation() ? obj_raw : sigmoid(obj_raw);
            // Early gate: no class can push the score above `thr` once the
            // objectness is below it (max class probability is 1).
            if (obj < thr) continue;
            float best_conf = 0.f;
            int best_cls = -1;
            for (int c = 0; c < nc; ++c) {
                const float p_raw = s.cls[static_cast<size_t>(c) * plane + idx];
                const float conf =
                    obj * (v11_fused_activation() ? p_raw : sigmoid(p_raw));
                if (conf > best_conf) {
                    best_conf = conf;
                    best_cls = c;
                }
            }
            if (best_cls < 0 || best_conf < thr) continue;
            float edge[4];
            for (int q = 0; q < 4; ++q) {
                for (int b = 0; b < kDflBins; ++b)
                    bins[b] =
                        s.reg[(static_cast<long long>(q) * kDflBins + b) * plane + idx];
                edge[q] = dfl_expectation(bins, kDflBins) *
                          static_cast<float>(s.stride);
            }
            const float cx = (static_cast<float>(x) + 0.5f) * s.stride;
            const float cy = (static_cast<float>(y) + 0.5f) * s.stride;
            float x1 = cx - edge[0], y1 = cy - edge[1];
            float x2 = cx + edge[2], y2 = cy + edge[3];
            x1 = std::max(0.f, std::min(x1, in_w - 1.f));
            y1 = std::max(0.f, std::min(y1, in_h - 1.f));
            x2 = std::max(0.f, std::min(x2, in_w - 1.f));
            y2 = std::max(0.f, std::min(y2, in_h - 1.f));
            if (!(x2 > x1 && y2 > y1)) continue;
            dets.push_back({(x1 + x2) * 0.5f / in_w, (y1 + y2) * 0.5f / in_h,
                            (x2 - x1) / in_w, (y2 - y1) / in_h, best_conf,
                            best_cls});
        }
    }
}


inline bool is_yolo_head(const Head& h) {
    return h.data != nullptr && h.n_dims == 4 &&
           h.d1 == 3 * (5 + kClasses) && h.d2 > 0 && h.d2 == h.d3;
}

// ---------------------------------------------------------------------------
// YOLOX, matched to YOLOX_CPP_ Demo/yolox_demo.cpp.
//
// That demo reads one output blob. Each anchor is 85 contiguous floats
// (cx, cy, w, h, objectness, 80 class scores). Anchors are ordered by stride
// 8, then 16, then 32. Inside a stride the outer loop is y and the inner
// loop is x, so anchor (y, x) sits at (y * S + x) * 85.
//   cx = (raw_cx + x) * stride
//   cy = (raw_cy + y) * stride
//   w  = exp(raw_w) * stride
//   h  = exp(raw_h) * stride
//   score = objectness * class
// Scores are already probabilities. EVS_YOLOX_SIGMOID=1 or HeadMode::kRaw
// applies sigmoid. NMS is class-agnostic at IoU 0.5, same as
// cv::dnn::NMSBoxes(..., 0.25, 0.5). Every class above the candidate score
// is kept, not only the best one.
// A per-scale NCHW [1, 85, S, S] or NHWC [1, S, S, 85] map is also accepted.
// ---------------------------------------------------------------------------
constexpr float kYoloXNmsIou = 0.5f;
inline bool yolox_sigmoid_scores(HeadMode mode) {
    if (mode == HeadMode::kRaw) return true;
    if (mode == HeadMode::kClsAct || mode == HeadMode::kAllAct) return false;
    const char* e = std::getenv("EVS_YOLOX_SIGMOID");
    return e && e[0] == '1';
}

// NCHW when d1 is the channel count; NHWC when the channel count is d3.
inline bool is_yolox_head(const Head& h, bool* nhwc) {
    if (h.data == nullptr || h.n_dims != 4 || h.d0 != 1) return false;
    const int ch = 5 + kClasses;
    if (h.d1 == ch && h.d2 > 0 && h.d2 == h.d3) {
        if (nhwc) *nhwc = false;
        return true;
    }
    if (h.d3 == ch && h.d1 > 0 && h.d1 == h.d2) {
        if (nhwc) *nhwc = true;
        return true;
    }
    return false;
}

inline void decode_yolox_head(const Head& h, bool nhwc, float thr,
                              std::vector<Det>& dets) {
    const int S = nhwc ? h.d1 : h.d2;
    const int stride = static_cast<int>(kInputSize) / S;
    if (S <= 0 || stride <= 0) return;
    const int ch = 5 + kClasses;
    const long long plane = static_cast<long long>(S) * S;
    const float scale = 1.f / kInputSize;
    const bool sig = yolox_sigmoid_scores(h.mode);
    auto at = [&](int c, int y, int x) -> float {
        if (nhwc) {
            return h.data[(static_cast<long long>(y) * S + x) * ch + c];
        }
        return h.data[static_cast<long long>(c) * plane +
                      static_cast<long long>(y) * S + x];
    };
    for (int y = 0; y < S; ++y) {
        for (int x = 0; x < S; ++x) {
            const float obj_raw = at(4, y, x);
            const float obj = sig ? sigmoid(obj_raw) : obj_raw;
            if (obj < thr) continue;
            const float bw = std::exp(at(2, y, x)) * static_cast<float>(stride) * scale;
            const float bh = std::exp(at(3, y, x)) * static_cast<float>(stride) * scale;
            if (!(bw > 0.f && bh > 0.f) || bw > kMaxBoxSize || bh > kMaxBoxSize)
                continue;
            const float cx =
                (at(0, y, x) + static_cast<float>(x)) * static_cast<float>(stride) * scale;
            const float cy =
                (at(1, y, x) + static_cast<float>(y)) * static_cast<float>(stride) * scale;
            for (int c = 0; c < kClasses; ++c) {
                const float p_raw = at(5 + c, y, x);
                const float conf = obj * (sig ? sigmoid(p_raw) : p_raw);
                if (conf < thr) continue;
                dets.push_back({cx, cy, bw, bh, conf, c});
            }
        }
    }
}

// Number of anchors in the demo's single blob: 80*80 + 40*40 + 20*20 at 640.
inline int yolox_packed_anchors(int input) {
    int n = 0;
    const int strides[3] = {8, 16, 32};
    for (int s : strides) {
        if (s <= 0 || input % s != 0) return 0;
        const int g = input / s;
        n += g * g;
    }
    return n;
}

// True when `h` is the demo's one blob: N anchors × 85 floats, not a
// per-scale [1, 85, S, S] map.
inline bool is_yolox_packed(const Head& h) {
    if (h.data == nullptr) return false;
    const int ch = 5 + kClasses;
    const int anchors = yolox_packed_anchors(static_cast<int>(kInputSize));
    if (anchors <= 0) return false;
    const size_t need = static_cast<size_t>(anchors) * static_cast<size_t>(ch);
    if (h.elems == need) return true;
    long long count = 1;
    const int nd = h.n_dims;
    const int dims[4] = {h.d0, h.d1, h.d2, h.d3};
    if (nd <= 0) return false;
    for (int i = 0; i < nd && i < 4; ++i) {
        if (dims[i] <= 0) return false;
        count *= dims[i];
    }
    return count == static_cast<long long>(need);
}

// Walk the demo blob. Outer grid index is y, inner is x.
inline void decode_yolox_packed(const float* data, HeadMode mode, float thr,
                                std::vector<Det>& dets) {
    const int ch = 5 + kClasses;
    const float scale = 1.f / kInputSize;
    const bool sig = yolox_sigmoid_scores(mode);
    int anchor = 0;
    const int strides[3] = {8, 16, 32};
    for (int stride : strides) {
        const int S = static_cast<int>(kInputSize) / stride;
        for (int y = 0; y < S; ++y) {
            for (int x = 0; x < S; ++x) {
                const float* b = data + static_cast<long long>(anchor) * ch;
                ++anchor;
                const float obj = sig ? sigmoid(b[4]) : b[4];
                if (obj < thr) continue;
                const float bw = std::exp(b[2]) * static_cast<float>(stride) * scale;
                const float bh = std::exp(b[3]) * static_cast<float>(stride) * scale;
                if (!(bw > 0.f && bh > 0.f) || bw > kMaxBoxSize || bh > kMaxBoxSize)
                    continue;
                const float cx = (b[0] + static_cast<float>(x)) * stride * scale;
                const float cy = (b[1] + static_cast<float>(y)) * stride * scale;
                for (int c = 0; c < kClasses; ++c) {
                    const float p = sig ? sigmoid(b[5 + c]) : b[5 + c];
                    const float conf = obj * p;
                    if (conf < thr) continue;
                    dets.push_back({cx, cy, bw, bh, conf, c});
                }
            }
        }
    }
}

// Full post-processing chain: raw heads -> decoded boxes -> NMS -> rows.
inline void decode_heads(const std::vector<Head>& heads, std::vector<float>& out) {
    std::vector<Det> dets;
    // YOLOv11 first: its per-scale tensors are separate outputs and only make
    // sense as a group. The v5 path below rejects every one of them (d1 != 255)
    // and would silently return zero rows.
    std::vector<V11Scale> v11;
    if (collect_v11_scales(heads, v11)) {
        if (std::getenv("EVS_YOLO_DUMP"))
            std::fprintf(stderr, "[yolo11] decoupled head: %zu scale(s)\n",
                         v11.size());
        for (const auto& s : v11)
            decode_v11_scale(s, kClasses, kCandidateScore, dets);
        nms(dets, kDefaultNmsIou, kMaxPreNms);
        if (std::getenv("EVS_YOLO_DUMP")) {
            for (const auto& d : dets)
                std::fprintf(
                    stderr,
                    "[yolo11] cls=%d conf=%.4f cx=%.4f cy=%.4f w=%.4f h=%.4f\n",
                    d.cls, d.conf, d.cx, d.cy, d.w, d.h);
        }
        emit_rows(dets, out);
        return;
    }
    // YOLOX. The demo blob is one [anchors, 85] tensor. Per-scale maps are
    // accepted too. v5 needs 255 channels and v11 needs a 64-channel DFL map.
    bool any_yolox = false;
    for (const auto& h : heads) {
        if (!is_yolox_packed(h)) continue;
        any_yolox = true;
        decode_yolox_packed(h.data, h.mode, kCandidateScore, dets);
    }
    if (!any_yolox) {
        for (const auto& h : heads) {
            bool nhwc = false;
            if (!is_yolox_head(h, &nhwc)) continue;
            any_yolox = true;
            decode_yolox_head(h, nhwc, kCandidateScore, dets);
        }
    }
    if (any_yolox) {
        nms(dets, kYoloXNmsIou, kMaxPreNms, /*per_class=*/false);
        if (std::getenv("EVS_YOLO_DUMP")) {
            std::fprintf(stderr, "[yolox] heads decoded, dets=%zu\n", dets.size());
            for (const auto& d : dets)
                std::fprintf(stderr,
                             "[yolox] cls=%d conf=%.4f cx=%.4f cy=%.4f w=%.4f h=%.4f\n",
                             d.cls, d.conf, d.cx, d.cy, d.w, d.h);
        }
        emit_rows(dets, out);
        return;
    }
    for (const auto& h : heads) {
        if (!is_yolo_head(h)) continue;
        const int stride = static_cast<int>(kInputSize) / h.d2;
        const HeadMode mode =
            (h.mode == HeadMode::kAuto)
                ? detect_head_mode(h.data,
                                   h.elems ? h.elems
                                           : static_cast<size_t>(h.d1) * h.d2 * h.d3)
                : h.mode;
        decode_head(h.data, h.d2, stride, dets, mode);
    }
    nms(dets, kDefaultNmsIou, kMaxPreNms);
    if (std::getenv("EVS_YOLO_DUMP")) {
        for (const auto& d : dets)
            std::fprintf(stderr,
                         "[yolo] cls=%d conf=%.4f cx=%.4f cy=%.4f w=%.4f h=%.4f\n",
                         d.cls, d.conf, d.cx, d.cy, d.w, d.h);
    }
    emit_rows(dets, out);
}

} // namespace yolo
} // namespace evs
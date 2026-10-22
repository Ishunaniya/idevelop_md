#include "evs/preprocess.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace evs {

namespace {

// BT.601 limited-range in integer, driven by lookup tables. The per-pixel
// float math (and its 8 multiplies) was the biggest single cost in the whole
// pipeline: the NV12 -> RGB pass touches every source pixel (~2.7 MB at 720p).
// Built once; per pixel it becomes 3 lookups + 2 adds + 3 stores.
struct Bt601Tables {
    int32_t yr[256], yg[256], yb[256];
    int32_t ur[256], ug[256], ub[256];
    int32_t vr[256], vg[256], vb[256];
    Bt601Tables() {
        for (int i = 0; i < 256; ++i) {
            const int y = (i - 16) * 298;
            yr[i] = y >> 8;
            yg[i] = y >> 8;
            yb[i] = y >> 8;
        }
        for (int i = 0; i < 256; ++i) {
            const int u = i - 128, v = i - 128;
            ur[i] = (409 * v) >> 8;
            ug[i] = (-100 * u - 208 * v) >> 8;
            ub[i] = (516 * u) >> 8;
            vr[i] = (409 * v) >> 8;
            vg[i] = (-100 * u - 208 * v) >> 8;
            vb[i] = (516 * u) >> 8;
        }
    }
};

inline uint8_t clamp_u8(int32_t v) {
    return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
}

// NV12 (YUV420 semi-planar) -> interleaved RGB/BGR. Y plane stride = ystride;
// UV plane interleaved, same row stride, half the rows.
void nv12_to_target(const uint8_t* yuv, int w, int h, int ystride,
                    bool bgr, std::vector<uint8_t>& out) {
    static const Bt601Tables t;
    out.resize(static_cast<size_t>(w) * h * 3);
    const uint8_t* yp = yuv;
    const uint8_t* up = yuv + static_cast<size_t>(h) * ystride;
    for (int y = 0; y < h; ++y) {
        const uint8_t* yrow = yp + static_cast<size_t>(y) * ystride;
        const uint8_t* urow = up + static_cast<size_t>(y >> 1) * ystride;
        uint8_t* orow = out.data() + static_cast<size_t>(y) * w * 3;
        for (int x = 0; x < w; ++x) {
            const int Y = yrow[x];
            const int uv_off = (x & ~1);
            const int U = urow[uv_off];
            const int V = urow[uv_off + 1];
            const int r = t.yr[Y] + t.ur[U] + t.vr[V];
            const int g = t.yg[Y] + t.ug[U] + t.vg[V];
            const int b = t.yb[Y] + t.ub[U] + t.vb[V];
            uint8_t* o = orow + static_cast<size_t>(x) * 3;
            if (bgr) {
                o[0] = clamp_u8(b);
                o[1] = clamp_u8(g);
                o[2] = clamp_u8(r);
            } else {
                o[0] = clamp_u8(r);
                o[1] = clamp_u8(g);
                o[2] = clamp_u8(b);
            }
        }
    }
}

} // anonymous namespace

bool Preprocessor::configure(const PreprocessConfig& cfg) {
    if (cfg.input_width <= 0 || cfg.input_height <= 0) {
        configured_ = false;
        return false;
    }
    cfg_ = cfg;
    configured_ = true;
    return true;
}

uint32_t Preprocessor::run(const Frame& frame, std::vector<uint8_t>& out,
                           PreprocessMap& map) const {
    if (!configured_) return static_cast<uint32_t>(EvsStatus::NotInitialized);
    const int ow = cfg_.input_width;
    const int oh = cfg_.input_height;
    const int fw = frame.width;
    const int fh = frame.height;
    if (frame.data == nullptr) return static_cast<uint32_t>(EvsStatus::NullPointer);
    if (fw <= 0 || fh <= 0) return static_cast<uint32_t>(EvsStatus::InvalidParameter);

    // 1) Convert the host frame into a planar target-format buffer (3 ch).
    std::vector<uint8_t> src;
    const bool tgt_bgr = (cfg_.target_format == PixelFormat::BGR);
    if (frame.format == PixelFormat::NV12) {
        const int ystride = frame.stride > 0 ? frame.stride : fw;
        nv12_to_target(frame.data, fw, fh, ystride, tgt_bgr, src);
    } else {
        const bool src_bgr = (frame.format == PixelFormat::BGR);
        const int in_stride = frame.stride > 0 ? frame.stride : fw * 3;
        src.resize(static_cast<size_t>(fw) * fh * 3);
        for (int y = 0; y < fh; ++y) {
            for (int x = 0; x < fw; ++x) {
                const uint8_t* p =
                    frame.data + static_cast<size_t>(y) * in_stride + x * 3;
                uint8_t r = p[0], g = p[1], b = p[2];
                if (src_bgr != tgt_bgr) std::swap(r, b);
                const size_t o = (static_cast<size_t>(y) * fw + x) * 3;
                src[o] = r;
                src[o + 1] = g;
                src[o + 2] = b;
            }
        }
    }

    // 2) Letterbox / stretch resize into the model input (NHWC, target_format).
    float sx, sy, padx, pady;
    if (cfg_.letterbox) {
        const float s = std::min(static_cast<float>(ow) / fw,
                                 static_cast<float>(oh) / fh);
        sx = sy = s;
        const int rw = static_cast<int>(std::lround(fw * s));
        const int rh = static_cast<int>(std::lround(fh * s));
        padx = (ow - rw) / 2.0f;
        pady = (oh - rh) / 2.0f;
    } else {
        sx = static_cast<float>(ow) / fw;
        sy = static_cast<float>(oh) / fh;
        padx = 0.f;
        pady = 0.f;
    }

    out.resize(static_cast<size_t>(ow) * oh * 3);
    const uint8_t pad = 114; // YOLO-style gray padding

    // Fixed-point bilinear (6-bit weights) with a per-row x-mapping cache.
    // The float version cost ~109 ms for 1280x720 -> 640x640 on the RK3576 --
    // more than the whole rest of the frame. Geometry and padding are exactly
    // the same as before (same half-pixel centres, a pixel is padding when
    // either tap falls outside the source); only the arithmetic is integer.
    constexpr int kFracBits = 6;
    constexpr int kFracOne = 1 << kFracBits;

    std::vector<int32_t> map_x0(ow), map_wx(ow);
    std::vector<uint8_t> pad_col(ow, pad);
    for (int x = 0; x < ow; ++x) {
        const float sx_f = (x + 0.5f - padx) / sx - 0.5f;
        const int x0 = static_cast<int>(std::floor(sx_f));
        const int x1 = x0 + 1;
        if (x0 < 0 || x1 >= fw) {
            map_x0[x] = 0;
            map_wx[x] = 0;
            pad_col[x] = 1;
            continue;
        }
        pad_col[x] = 0;
        map_x0[x] = x0;
        map_wx[x] = static_cast<int32_t>((sx_f - static_cast<float>(x0)) * kFracOne + 0.5f);
    }

    // Letterbox leaves whole output rows as pure padding (640x640 from a
    // 1280x720 frame pads 140 rows top and bottom). Interpolating those rows is
    // wasted work -- memset them and resample only the rows that map inside the
    // source: on the RK3576 that removed ~55% of the resampling cost.
    int y_first = oh;
    int y_last = -1;
    for (int y = 0; y < oh; ++y) {
        const float sy_f = (y + 0.5f - pady) / sy - 0.5f;
        const int y0 = static_cast<int>(std::floor(sy_f));
        if (y0 >= 0 && y0 + 1 < fh) {
            if (y < y_first) y_first = y;
            y_last = y;
        }
    }

    for (int y = 0; y < oh; ++y) {
        uint8_t* orow = out.data() + static_cast<size_t>(y) * ow * 3;
        if (y < y_first || y > y_last) {
            std::memset(orow, pad, static_cast<size_t>(ow) * 3);
            continue;
        }
        const float sy_f = (y + 0.5f - pady) / sy - 0.5f;
        const int y0 = static_cast<int>(std::floor(sy_f));
        const int y1 = y0 + 1;
        const int wy = static_cast<int>((sy_f - static_cast<float>(y0)) * kFracOne + 0.5f);
        const int wy0 = kFracOne - wy;

        const uint8_t* r0 = src.data() + static_cast<size_t>(y0) * fw * 3;
        const uint8_t* r1 = src.data() + static_cast<size_t>(y1) * fw * 3;

        int x = 0;
        for (; x < ow; ++x) {
            if (pad_col[x]) {
                orow[x * 3 + 0] = pad;
                orow[x * 3 + 1] = pad;
                orow[x * 3 + 2] = pad;
                continue;
            }
            const int x0 = map_x0[x];
            const int wx = map_wx[x];
            const int wx0 = kFracOne - wx;
            const uint8_t* a = r0 + static_cast<size_t>(x0) * 3;
            const uint8_t* b = r0 + static_cast<size_t>(x0 + 1) * 3;
            const uint8_t* c = r1 + static_cast<size_t>(x0) * 3;
            const uint8_t* d = r1 + static_cast<size_t>(x0 + 1) * 3;
            for (int ch = 0; ch < 3; ++ch) {
                const int top = a[ch] * wx0 + b[ch] * wx;
                const int bot = c[ch] * wx0 + d[ch] * wx;
                const int v = (top * wy0 + bot * wy + (kFracOne * kFracOne) / 2) >>
                              (2 * kFracBits);
                orow[x * 3 + ch] =
                    static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
            }
        }
    }

    map.scale_x = sx;
    map.scale_y = sy;
    map.pad_x = padx;
    map.pad_y = pady;
    map.in_w = ow;
    map.in_h = oh;
    return 0u;
}

void map_target_to_original(const PreprocessMap& map, Target& t,
                            int orig_w, int orig_h) {
    if (orig_w <= 0 || orig_h <= 0 || map.in_w <= 0 || map.in_h <= 0) return;
    const float mx = t.x * map.in_w;
    const float my = t.y * map.in_h;
    const float mw = t.w * map.in_w;
    const float mh = t.h * map.in_h;
    const float ox = (mx - map.pad_x) / map.scale_x;
    const float oy = (my - map.pad_y) / map.scale_y;
    const float ow = mw / map.scale_x;
    const float oh = mh / map.scale_y;
    t.x = ox / orig_w;
    t.y = oy / orig_h;
    t.w = ow / orig_w;
    t.h = oh / orig_h;
}

} // namespace evs

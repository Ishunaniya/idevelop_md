#include "rga_preprocess.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#if defined(EVS_WITH_RGA)

// librga 2.1.x single-header API (matching librga2 2.1.0 from the SDK deb):
// imresize()/imfill()/imcvtcolor() are macros over the im2d_*_t entry points.
// rga.h carries the RK_FORMAT_* pixel-format enums; im2d.h carries the API.
#include "rga.h"
#include "im2d.h"

namespace {

std::string g_last_error;

void set_err(const std::string& e) { g_last_error = e; }

// Colour-space handling is board/driver dependent, so keep it switchable
// (EVS_RGA_CSC): 0 default, 1 BT601_LIMIT, 2 LIMIT_RANGE via color_space_mode,
// 3 both (librga ignores either half alone -> wrong colours, meandiff 44),
// 4 BT601_LIMIT|LIMIT_RANGE (correct, byte-identical to CPU), 5 BT709_LIMIT.
// EVS_RGA_CSC picks the combination (see _patch_rga_csc_sweep.py).
int rga_csc_mode() {
    // 4 = IM_YUV_TO_RGB_BT601_LIMIT | IM_YUV_BT601_LIMIT_RANGE. librga ignores
    // either half on its own; together they reproduce the CPU path's Bt601
    // limited matrix byte for byte (measured maxdiff=0). Override only when
    // bringing up a different board.
    const char* e = std::getenv("EVS_RGA_CSC");
    return e ? std::atoi(e) : 0;
}

// BT.601 limited conversion, same integer coefficients as
// evs::Preprocessor (src/core/preprocess.cpp Bt601Tables). Writes into an
// arbitrary destination stride/offset so the letterbox padding stays in place.
void nv12_to_rgb_at(const uint8_t* nv12, int w, int h, uint8_t* dst,
                    int dst_stride) {
    int32_t yr[256], ur[256], vr[256], ug[256], vg[256], ub[256];
    for (int i = 0; i < 256; ++i) {
        const int y = (i - 16) * 298;
        yr[i] = y >> 8;
        const int u = i - 128, v = i - 128;
        ur[i] = (409 * v) >> 8;
        vr[i] = (409 * v) >> 8;
        ug[i] = (-100 * u - 208 * v) >> 8;
        vg[i] = (-100 * u - 208 * v) >> 8;
        ub[i] = (516 * u) >> 8;
    }
    const uint8_t* yp = nv12;
    const uint8_t* up = nv12 + static_cast<size_t>(h) * w;
    for (int y = 0; y < h; ++y) {
        const uint8_t* yrow = yp + static_cast<size_t>(y) * w;
        const uint8_t* urow = up + static_cast<size_t>(y >> 1) * w;
        uint8_t* orow = dst + static_cast<size_t>(y) * dst_stride;
        for (int x = 0; x < w; ++x) {
            const int uv_off = (x & ~1);
            const int Y = yrow[x];
            const int U = urow[uv_off];
            const int V = urow[uv_off + 1];
            int r = yr[Y] + ur[U] + vr[V];
            int g = yr[Y] + ug[U] + vg[V];
            int b = yr[Y] + ub[U];
            if (r < 0) r = 0; else if (r > 255) r = 255;
            if (g < 0) g = 0; else if (g > 255) g = 255;
            if (b < 0) b = 0; else if (b > 255) b = 255;
            uint8_t* o = orow + static_cast<size_t>(x) * 3;
            o[0] = static_cast<uint8_t>(r);
            o[1] = static_cast<uint8_t>(g);
            o[2] = static_cast<uint8_t>(b);
        }
    }
}

rga_buffer_t make_buf(void* addr, int w, int h, int format, int wstride) {
    rga_buffer_t b{};
    b.vir_addr = addr;
    b.phy_addr = 0;
    b.fd = -1;
    b.width = w;
    b.height = h;
    b.wstride = wstride;
    b.hstride = h; // unused for packed formats
    b.format = format;
    b.color_space_mode = IM_COLOR_SPACE_DEFAULT;
    b.global_alpha = 0xff;
    return b;
}

rga_buffer_t make_buf_fd(int fd, int w, int h, int format, int wstride) {
    rga_buffer_t b{};
    b.fd = fd;
    b.vir_addr = nullptr;
    b.phy_addr = 0;
    b.width = w;
    b.height = h;
    b.wstride = wstride;
    b.hstride = h;
    b.format = format;
    b.color_space_mode = IM_COLOR_SPACE_DEFAULT;
    b.global_alpha = 0xff;
    return b;
}

} // namespace

namespace evs_host {

bool rga_available() {
    // Functional probe instead of a version call: librga 2.1.0 has no
    // queryRgaVersion() export, and a tiny NV12->RGB conversion proves in one
    // shot that the userspace library, the kernel node (/dev/rga) and our
    // header agree.
    static const bool ok = [] {
        // wstride is in pixels. 16*3 here used to overrun the RGB buffer and
        // crash on free, which showed up as a segfault right after this line.
        constexpr int kSide = 64;
        std::vector<uint8_t> nv12(static_cast<std::size_t>(kSide) * kSide * 3 / 2, 128);
        std::vector<uint8_t> rgb(static_cast<std::size_t>(kSide) * kSide * 3, 0);
        rga_buffer_t src = make_buf(nv12.data(), kSide, kSide, RK_FORMAT_YCbCr_420_SP, kSide);
        rga_buffer_t dst = make_buf(rgb.data(), kSide, kSide, RK_FORMAT_RGB_888, kSide);
        const IM_STATUS st = imcvtcolor(src, dst, RK_FORMAT_YCbCr_420_SP,
                                        RK_FORMAT_RGB_888);
        std::printf("[rga] probe NV12->RGB888 status=%d -> %s\n", st,
                    (st == IM_STATUS_SUCCESS) ? "usable" : "unavailable");
        return st == IM_STATUS_SUCCESS;
    }();
    return ok;
}

const char* rga_last_error() { return g_last_error.c_str(); }

static uint32_t rga_preprocess_core(const rga_buffer_t& src_in,
                                    const uint8_t* cpu_src, int w, int h,
                                    int out_w, int out_h, bool letterbox,
                                    std::vector<uint8_t>& out,
                                    evs::PreprocessMap& map) {
    rga_buffer_t src = src_in;
    const int csc = rga_csc_mode();
    if (csc == 2 || csc == 3) src.color_space_mode = IM_YUV_BT601_LIMIT_RANGE;
    // Same geometry rule as evs::Preprocessor: fit inside the model input,
    // centered, gray padding.
    float s = 1.f;
    int rw = out_w, rh = out_h, padx = 0, pady = 0;
    if (letterbox) {
        s = std::min(static_cast<float>(out_w) / w,
                     static_cast<float>(out_h) / h);
        rw = static_cast<int>(std::lround(w * s));
        rh = static_cast<int>(std::lround(h * s));
        padx = (out_w - rw) / 2;
        pady = (out_h - rh) / 2;
    }
    if ((rw & 1) || (rh & 1)) { // NV12 chroma alignment
        rw &= ~1;
        rh &= ~1;
    }
    if (rw <= 0 || rh <= 0) {
        set_err("degenerate scaled size");
        return 1u;
    }

    // librga expresses wstride in PIXELS, not bytes, and multiplies by the
    // channel count itself. Passing out_w*3 made every row three times too
    // long: the image came out torn/striped (maxdiff=255) and the overflow
    // corrupted the source frame.
    // librga wstride is in PIXELS; our dst_off is a BYTE offset.
    // Mixing the two put the picture at row 46 instead of row 140.
    const int dst_stride_px = out_w;      // rga_buffer_t::wstride
    const int dst_row_bytes = out_w * 3;  // our byte addressing
    if (std::getenv("EVS_RGA_NOPAD")) padx = pady = 0; // diagnostic
    const size_t dst_off = static_cast<size_t>(pady) * dst_row_bytes +
                           static_cast<size_t>(padx) * 3;
    // RGA wants 16-byte aligned start addresses for the destination rows.
    if ((dst_off % 16u) != 0u) {
        char buf[128];
        std::snprintf(buf, sizeof(buf),
                      "letterbox offset %zu is not 16-byte aligned", dst_off);
        set_err(buf);
        return 1u;
    }

    // 1) gray padding over the whole output (114 == CPU pad colour).
    out.assign(static_cast<size_t>(out_w) * out_h * 3, 114);

    // 2) NV12 scale into a small NV12 buffer, then colour-convert into the
    //    padded destination. The caller decides where `src` lives (CPU or a
    //    dma-buf fd); RGA handles either transparently.
    std::vector<uint8_t> mid(static_cast<size_t>(rw) * rh * 3 / 2, 114);
    rga_buffer_t mid_buf = make_buf(mid.data(), rw, rh,
                                    RK_FORMAT_YCbCr_420_SP, rw);
    // The scaled NV12 buffer is the colour-conversion source, so the range has
    // to be declared here too.
    if (csc == 2 || csc == 3) mid_buf.color_space_mode = IM_YUV_BT601_LIMIT_RANGE;
    IM_STATUS rs = imresize(src, mid_buf);
    if (rs != IM_STATUS_SUCCESS) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "imresize failed status=%d", rs);
        set_err(buf);
        return 1u;
    }
    if (csc == 8) {
        if (cpu_src == nullptr) {
            set_err("EVS_RGA_CSC=8 needs a CPU-side frame (not --rga-fd)");
            return 1u;
        }
        // 1) identical full-resolution RGB, straight from the CPU tables
        std::vector<uint8_t> rgb_full(static_cast<size_t>(w) * h * 3);
        nv12_to_rgb_at(cpu_src, w, h, rgb_full.data(), w * 3);
        // 2) RGA does the resize -- the step that used to cost ~40 ms
        std::vector<uint8_t> rgb_small(static_cast<size_t>(rw) * rh * 3);
        rga_buffer_t rs = make_buf(rgb_full.data(), w, h, RK_FORMAT_RGB_888, w);
        rga_buffer_t rd = make_buf(rgb_small.data(), rw, rh, RK_FORMAT_RGB_888,
                                   rw);
        IM_STATUS c1 = imresize(rs, rd);
        if (c1 != IM_STATUS_SUCCESS) {
            char buf[96];
            std::snprintf(buf, sizeof(buf), "imresize(rgb) failed status=%d",
                          c1);
            set_err(buf);
            return 1u;
        }
        std::memcpy(out.data() + dst_off, rgb_small.data(),
                    static_cast<size_t>(rw) * rh * 3);
        map.scale_x = s;
        map.scale_y = s;
        map.pad_x = static_cast<float>(padx);
        map.pad_y = static_cast<float>(pady);
        map.in_w = out_w;
        map.in_h = out_h;
        return 0u;
    }

    if (csc == 7) {
        // Same order as the CPU path: colour-convert the full frame, then
        // scale the RGB plane. wstride is in pixels for librga.
        std::vector<uint8_t> rgb_full(static_cast<size_t>(w) * h * 3);
        rga_buffer_t csc_src = src;
        rga_buffer_t csc_dst = make_buf(rgb_full.data(), w, h,
                                        RK_FORMAT_RGB_888, w);
        IM_STATUS c0 = imcvtcolor(csc_src, csc_dst, RK_FORMAT_YCbCr_420_SP,
                                  RK_FORMAT_RGB_888);
        if (c0 != IM_STATUS_SUCCESS) {
            char buf[96];
            std::snprintf(buf, sizeof(buf), "imcvtcolor(full) failed status=%d",
                          c0);
            set_err(buf);
            return 1u;
        }
        rga_buffer_t rs = make_buf(rgb_full.data(), w, h, RK_FORMAT_RGB_888, w);
        rga_buffer_t rd = make_buf(out.data() + dst_off, rw, rh,
                                   RK_FORMAT_RGB_888, rw);
        IM_STATUS c1 = imresize(rs, rd);
        if (c1 != IM_STATUS_SUCCESS) {
            char buf[96];
            std::snprintf(buf, sizeof(buf), "imresize(rgb) failed status=%d",
                          c1);
            set_err(buf);
            return 1u;
        }
        map.scale_x = s;
        map.scale_y = s;
        map.pad_x = static_cast<float>(padx);
        map.pad_y = static_cast<float>(pady);
        map.in_w = out_w;
        map.in_h = out_h;
        return 0u;
    }

    if (csc == 6) {
        // RGA did the scale; convert with the same coefficients the library
        // uses so the pixels (and therefore the detections) are identical.
        nv12_to_rgb_at(mid.data(), rw, rh, out.data() + dst_off,
                       out_w * 3);
        map.scale_x = s;
        map.scale_y = s;
        map.pad_x = static_cast<float>(padx);
        map.pad_y = static_cast<float>(pady);
        map.in_w = out_w;
        map.in_h = out_h;
        return 0u;
    }

    rga_buffer_t dst = make_buf(out.data() + dst_off, rw, rh,
                                RK_FORMAT_RGB_888, dst_stride_px);
    // BT.601 limited, matching the CPU path's Bt601Tables coefficients
    // (1.164*(Y-16) / 1.596 V / 2.017 U). librga's default conversion is a
    // different range matrix, which cost ~44 grey levels of mean difference.
    IM_STATUS cs = IM_STATUS_FAILED;
    switch (csc) {
        case 1:
            cs = imcvtcolor(mid_buf, dst, RK_FORMAT_YCbCr_420_SP,
                            RK_FORMAT_RGB_888, IM_YUV_TO_RGB_BT601_LIMIT);
            break;
        case 4:
            cs = imcvtcolor(mid_buf, dst, RK_FORMAT_YCbCr_420_SP,
                            RK_FORMAT_RGB_888,
                            IM_YUV_TO_RGB_BT601_LIMIT |
                                IM_YUV_BT601_LIMIT_RANGE);
            break;
        case 5:
            cs = imcvtcolor(mid_buf, dst, RK_FORMAT_YCbCr_420_SP,
                            RK_FORMAT_RGB_888, IM_YUV_TO_RGB_BT709_LIMIT);
            break;
        case 0:
            cs = imcvtcolor(mid_buf, dst, RK_FORMAT_YCbCr_420_SP,
                            RK_FORMAT_RGB_888);
            break;
        default:
            cs = imcvtcolor(mid_buf, dst, RK_FORMAT_YCbCr_420_SP,
                            RK_FORMAT_RGB_888, IM_YUV_TO_RGB_BT601_LIMIT);
            break;
    }
    if (cs != IM_STATUS_SUCCESS) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "imcvtcolor failed status=%d", cs);
        set_err(buf);
        return 1u;
    }

    map.scale_x = s;
    map.scale_y = s;
    map.pad_x = static_cast<float>(padx);
    map.pad_y = static_cast<float>(pady);
    map.in_w = out_w;
    map.in_h = out_h;
    return 0u;
}

uint32_t rga_preprocess_nv12(const uint8_t* nv12, int w, int h, int out_w,
                             int out_h, bool letterbox, std::vector<uint8_t>& out,
                             evs::PreprocessMap& map) {
    g_last_error.clear();
    if (!nv12 || w <= 0 || h <= 0 || out_w <= 0 || out_h <= 0) {
        set_err("bad arguments");
        return 1u;
    }
    if ((w & 1) || (h & 1)) {
        set_err("odd source size for NV12");
        return 1u;
    }
    rga_buffer_t src = make_buf(const_cast<uint8_t*>(nv12), w, h,
                                RK_FORMAT_YCbCr_420_SP, w);
    return rga_preprocess_core(src, nv12, w, h, out_w, out_h, letterbox, out, map);
}

uint32_t rga_preprocess_yuv(const uint8_t* src_px, int w, int h, RgaSrcFormat fmt,
                            int out_w, int out_h, bool letterbox,
                            std::vector<uint8_t>& out, evs::PreprocessMap& map) {
    g_last_error.clear();
    if (!src_px || w <= 0 || h <= 0 || out_w <= 0 || out_h <= 0) {
        set_err("bad arguments");
        return 1u;
    }
    if ((w & 1) || (h & 1)) {
        set_err("odd source size");
        return 1u;
    }
    int rk = RK_FORMAT_YCbCr_420_SP;
    bool semi_420 = true;
    switch (fmt) {
        case RgaSrcFormat::NV12: rk = RK_FORMAT_YCbCr_420_SP; semi_420 = true; break;
        case RgaSrcFormat::NV16: rk = RK_FORMAT_YCbCr_422_SP; semi_420 = false; break;
        case RgaSrcFormat::NV61: rk = RK_FORMAT_YCrCb_422_SP; semi_420 = false; break;
        case RgaSrcFormat::YUYV: rk = RK_FORMAT_YUYV_422; semi_420 = false; break;
        case RgaSrcFormat::UYVY: rk = RK_FORMAT_UYVY_422; semi_420 = false; break;
    }
    float s = 1.f;
    int rw = out_w, rh = out_h, padx = 0, pady = 0;
    if (letterbox) {
        s = std::min(static_cast<float>(out_w) / w, static_cast<float>(out_h) / h);
        rw = static_cast<int>(std::lround(w * s));
        rh = static_cast<int>(std::lround(h * s));
        padx = (out_w - rw) / 2;
        pady = (out_h - rh) / 2;
    }
    rw &= ~1;
    rh &= ~1;
    if (rw <= 0 || rh <= 0) {
        set_err("degenerate scaled size");
        return 1u;
    }
    const int dst_row_bytes = out_w * 3;
    const size_t dst_off = static_cast<size_t>(pady) * dst_row_bytes +
                           static_cast<size_t>(padx) * 3;
    if ((dst_off % 16u) != 0u) {
        char buf[128];
        std::snprintf(buf, sizeof(buf),
                      "letterbox offset %zu is not 16-byte aligned", dst_off);
        set_err(buf);
        return 1u;
    }
    out.assign(static_cast<size_t>(out_w) * out_h * 3, 114);
    const size_t mid_bytes = semi_420 ? static_cast<size_t>(rw) * rh * 3 / 2
                                      : static_cast<size_t>(rw) * rh * 2;
    std::vector<uint8_t> mid(mid_bytes, 114);
    rga_buffer_t src = make_buf(const_cast<uint8_t*>(src_px), w, h, rk, w);
    rga_buffer_t mid_buf = make_buf(mid.data(), rw, rh, rk, rw);
    const int csc = rga_csc_mode();
    if (csc == 2 || csc == 3) {
        src.color_space_mode = IM_YUV_BT601_LIMIT_RANGE;
        mid_buf.color_space_mode = IM_YUV_BT601_LIMIT_RANGE;
    }
    const IM_STATUS rs = imresize(src, mid_buf);
    if (rs != IM_STATUS_SUCCESS) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "imresize failed status=%d", rs);
        set_err(buf);
        return 1u;
    }
    rga_buffer_t dst = make_buf(out.data() + dst_off, rw, rh, RK_FORMAT_RGB_888, out_w);
    IM_STATUS cs = IM_STATUS_FAILED;
    switch (csc) {
        case 1:
            cs = imcvtcolor(mid_buf, dst, rk, RK_FORMAT_RGB_888, IM_YUV_TO_RGB_BT601_LIMIT);
            break;
        case 4:
            cs = imcvtcolor(mid_buf, dst, rk, RK_FORMAT_RGB_888,
                            IM_YUV_TO_RGB_BT601_LIMIT | IM_YUV_BT601_LIMIT_RANGE);
            break;
        case 5:
            cs = imcvtcolor(mid_buf, dst, rk, RK_FORMAT_RGB_888, IM_YUV_TO_RGB_BT709_LIMIT);
            break;
        case 0:
            cs = imcvtcolor(mid_buf, dst, rk, RK_FORMAT_RGB_888);
            break;
        default:
            cs = imcvtcolor(mid_buf, dst, rk, RK_FORMAT_RGB_888, IM_YUV_TO_RGB_BT601_LIMIT);
            break;
    }
    if (cs != IM_STATUS_SUCCESS) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "imcvtcolor failed status=%d", cs);
        set_err(buf);
        return 1u;
    }
    map.scale_x = s;
    map.scale_y = s;
    map.pad_x = static_cast<float>(padx);
    map.pad_y = static_cast<float>(pady);
    map.in_w = out_w;
    map.in_h = out_h;
    return 0u;
}

uint32_t rga_preprocess_nv12_fd(int src_fd, int w, int h, int out_w, int out_h,
                                bool letterbox, std::vector<uint8_t>& out,
                                evs::PreprocessMap& map) {
    g_last_error.clear();
    if (src_fd < 0 || w <= 0 || h <= 0 || out_w <= 0 || out_h <= 0) {
        set_err("bad dma-buf fd / arguments");
        return 1u;
    }
    if ((w & 1) || (h & 1)) {
        set_err("odd source size for NV12");
        return 1u;
    }
    rga_buffer_t src = make_buf_fd(src_fd, w, h, RK_FORMAT_YCbCr_420_SP, w);
    return rga_preprocess_core(src, nullptr, w, h, out_w, out_h, letterbox, out, map);
}


} // namespace evs_host

#else // !EVS_WITH_RGA

namespace evs_host {
bool rga_available() { return false; }
const char* rga_last_error() { return "built without EVS_WITH_RGA"; }
uint32_t rga_preprocess_nv12(const uint8_t*, int, int, int, int, bool,
                             std::vector<uint8_t>&, evs::PreprocessMap&) {
    return 1u;
}
uint32_t rga_preprocess_nv12_fd(int, int, int, int, int, bool,
                                std::vector<uint8_t>&, evs::PreprocessMap&) {
    return 1u;
}
uint32_t rga_preprocess_yuv(const uint8_t*, int, int, RgaSrcFormat, int, int, bool,
                            std::vector<uint8_t>&, evs::PreprocessMap&) {
    return 1u;
}
} // namespace evs_host

#endif // EVS_WITH_RGA
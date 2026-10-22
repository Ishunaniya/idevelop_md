// Host-side RGA (Rockchip Graphics Acceleration) preprocessing.
//
// WHY THIS LIVES IN THE HOST, NOT IN evs
// ---------------------------------------
// `evs::Preprocessor` is deliberately pure C++ so the algorithm library builds
// with zero third-party dependencies on a PC (offline evaluation, unit tests)
// and cross-compiles for the board without dragging in Rockchip SDK headers.
// The design doc already anticipates this: "RGA target must be a dma-buf, never
// malloc'd heap" -- i.e. the acceleration belongs to whoever owns the capture
// buffers. So this module is a *host-side* helper: copy it next to your capture
// code (or keep using it from tools/v4l2_detect) and hand the result to the
// library as the same packed NHWC uint8 tensor the library would have produced.
//
// WHAT IT DOES
// ------------
// Exactly what Preprocessor does, but on the RGA: NV12 -> RGB888, letterbox with
// gray(114) padding, and it fills the same PreprocessMap so the rest of the
// pipeline (decode -> map_target_to_original) is untouched. On the RK3576 this
// replaces a ~109 ms CPU bilinear resize with a couple of milliseconds.
//
// Usage:
//   std::vector<uint8_t> input;
//   evs::PreprocessMap map{};
//   uint32_t rc = evs_host::rga_preprocess_nv12(nv12, 1280, 720, 640, 640,
//                                               /*letterbox=*/true, input, map);
//   // rc != 0 -> fall back to evs::Preprocessor
#pragma once

#include <cstdint>
#include <vector>

#include "evs/preprocess.h"

namespace evs_host {

// True when librga is present and the RGA path initialises (queried once).
bool rga_available();

// Last RGA error string (for logs / bring-up); empty when the last call worked.
const char* rga_last_error();

// NV12 frame -> packed RGB888 `out_w * out_h * 3`, letterboxed like the library's
// Preprocessor (gray 114 padding, centered). Returns 0 on success; on any RGA
// failure returns non-zero and leaves `out` untouched so the caller can fall
// back to the CPU path. `map` is filled exactly like PreprocessMap.
uint32_t rga_preprocess_nv12(const uint8_t* nv12, int w, int h, int out_w,
                            int out_h, bool letterbox, std::vector<uint8_t>& out,
                            evs::PreprocessMap& map);

// dma-buf fd variant: `src_fd` is an externally-exported NV12 dma-buf (e.g. via
// V4L2 VIDIOC_EXPBUF from a capture buffer, or DRM-allocated + PRIME-exported).
// RGA imports the fd directly, avoiding the capture->host copy. Geometry and
// output semantics are identical to the CPU variant; the result is still written
// to the CPU-side `out`. Returns 0 on success, non-zero on failure (leaves `out`
// intact so the caller can fall back to the CPU path).
uint32_t rga_preprocess_nv12_fd(int src_fd, int w, int h, int out_w, int out_h,
                                bool letterbox, std::vector<uint8_t>& out,
                                evs::PreprocessMap& map);

// Packed camera formats the live tool can hand to RGA without a CPU convert.
// NV12 is the semi-planar 4:2:0 the CPU preprocessor already accepts.
enum class RgaSrcFormat { NV12, NV16, NV61, YUYV, UYVY };

// Scale + colour-convert straight into the letterboxed model input.
// Same padding and PreprocessMap as rga_preprocess_nv12. Returns 0 on success.
uint32_t rga_preprocess_yuv(const uint8_t* src, int w, int h, RgaSrcFormat fmt,
                            int out_w, int out_h, bool letterbox,
                            std::vector<uint8_t>& out, evs::PreprocessMap& map);

} // namespace evs_host
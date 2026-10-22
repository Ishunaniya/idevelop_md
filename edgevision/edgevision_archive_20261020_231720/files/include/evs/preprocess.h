#pragma once
#include <cstdint>
#include <vector>

#include "evs/frame.h"
#include "evs/status.h"

namespace evs {

// Inverse mapping recorded by Preprocessor::run: how a model-input pixel maps
// back onto the original frame, so decode-time boxes (0..1 of the input) can be
// un-done to 0..1 of the original frame.
struct PreprocessMap {
    float scale_x = 1.f; // input_px / orig_px
    float scale_y = 1.f;
    float pad_x = 0.f;   // letterbox padding in input px (top-left origin)
    float pad_y = 0.f;
    int in_w = 0;
    int in_h = 0;
};

// Configuration for the frame -> model-input conversion.
struct PreprocessConfig {
    int input_width = 0;
    int input_height = 0;
    PixelFormat target_format = PixelFormat::RGB; // model input color order
    bool letterbox = true; // preserve aspect ratio (gray pad); else stretch
};

// Frame -> model-input preprocessing.
//
// Pure C++ implementation: no OpenCV / RGA dependency, so the scaffold builds on
// x86 (mock backend) and on the target without pulling heavy libs. On RK3588 a
// production build may replace the copy/resize with RGA + dma-buf zero-copy
// (design doc §6: RGA target must be a dma-buf, never malloc'd heap); the
// public behaviour (input bytes + PreprocessMap) stays identical.
//
// Output layout: packed uint8, NHWC, in `target_format`, sized
// input_width * input_height * 3. This matches the IInferBackend::infer()
// `std::vector<uint8_t>` contract and what an INT8 RKNN model consumes.
class Preprocessor {
public:
    // Returns false (and leaves the preprocessor unconfigured) when input dims
    // are invalid (<=0). Callers treat an unconfigured preprocessor as a
    // pass-through (empty input, identity map) for backward compatibility.
    bool configure(const PreprocessConfig& cfg);

    // Run preprocessing. `out` is resized to in_w*in_h*3 (uint8, NHWC).
    // `map` records the inverse mapping for decode -> original coordinate undo.
    // Returns an EvsStatus code (0 == success).
    uint32_t run(const Frame& frame, std::vector<uint8_t>& out, PreprocessMap& map) const;

    bool configured() const { return configured_; }

private:
    PreprocessConfig cfg_;
    bool configured_ = false;
};

// Map a decoded target (center x,y,w,h in 0..1 of the model input) back to
// 0..1 coordinates of the original frame, undoing letterbox / stretch.
void map_target_to_original(const PreprocessMap& map, Target& t,
                            int orig_w, int orig_h);

} // namespace evs

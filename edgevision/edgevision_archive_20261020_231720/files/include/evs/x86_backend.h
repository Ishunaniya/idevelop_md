#pragma once
#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "evs/iinfer_backend.h"

namespace evs {

// X86 host inference backend. One instance per (model path), same contract as
// every other IInferBackend, so VisionTask and OfflineEvaluator treat x86
// exactly like rknn / mock.
//
// Two operating modes, selected at build time:
//
//   * Reference mode (default, NO third-party SDK needed):
//     A dependency-free CPU detector that runs a simple but *real* colour-blob
//     algorithm over the preprocessed RGB input (NHWC uint8). It lets the whole
//     offline-evaluation pipeline run on any x86 laptop / CI without pulling in
//     OpenCV or ONNXRuntime, and it is the reference baseline the real model is
//     validated against.
//
//   * ONNX mode (define EVS_WITH_ORT and link onnxruntime):
//     Loads an exported ONNX model with ONNXRuntime (CPU) and runs the *same*
//     network you deploy on the edge device, so offline field photos give the
//     real production precision / recall. Output layout must match the decoder
//     selected by `output_format` in the task YAML (flat6 / yolov8 / ...).
//
// Switch between them without touching any algorithm code: just rebuild with
// -DEVS_BACKEND=x86 (reference) or -DEVS_BACKEND=onnx (real model).
class X86Backend : public IInferBackend {
public:
    X86Backend();
    ~X86Backend() override;

    uint32_t load(const std::string& model_path, int init_timeout_s) override;
    uint32_t infer(const std::vector<uint8_t>& input,
                   std::vector<float>& output) override;
    void configure_input(int width, int height) override;
    void release() override;
    bool loaded() const override { return loaded_.load(); }

    // True when running the real ONNX model (vs the dependency-free reference).
    bool using_onnx() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::atomic<bool> loaded_{false};
};

} // namespace evs

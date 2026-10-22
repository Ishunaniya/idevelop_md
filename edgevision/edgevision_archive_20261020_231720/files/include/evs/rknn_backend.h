#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "evs/iinfer_backend.h"

namespace evs {

// RK3576/RK3588 inference backend built on the RKNN runtime 2.x (librknnrt).
// Only compiled when EVS_BACKEND=rknn (see CMakeLists.txt). The NPU context is
// hidden behind a PIMPL so rknn_api.h never leaks into the algorithm / public
// headers, keeping the mock (host) and QNN (Android) builds free of Rockchip
// headers.
//
// Input contract: IInferBackend::infer() receives the uint8 NHWC tensor produced
// by Preprocessor (RGB or BGR, model-chosen order). Two model flavours are
// handled transparently:
//   * float32 model input  -> the backend converts NHWC uint8 to float in the
//     value range the model expects (raw 0..255 by default, see
//     EVS_RKNN_INPUT_SCALE);
//   * quantised (int8/uint8) model input -> the uint8 buffer is handed to the
//     runtime unchanged and the model's own quant params do the dequant.
// The raw YOLOv5 heads are decoded with the same code as the x86/ONNX backend
// (src/backend/yolo_decode.h) and emitted as "yolov8" rows, so the NPU path and
// the PC path produce the same boxes for the same model.
class RknnBackend : public IInferBackend {
public:
    RknnBackend();
    ~RknnBackend() override;

    uint32_t load(const std::string& model_path, int init_timeout_s) override;
    uint32_t infer(const std::vector<uint8_t>& input, std::vector<float>& output) override;
    // Winch path: explicit single-tensor box contract, independent of COCO heads.
    uint32_t validateDetectionModel(const std::string& format, int classes, int expected_width,
                                    int expected_height) const;
    uint32_t inferDetectionRows(const std::vector<uint8_t>& input, std::vector<float>& output,
                                const std::string& format, int classes, int expected_width,
                                int expected_height);
    void release() override;

    bool loaded() const override { return loaded_.load(); }

    // Diagnostics: model input geometry queried at load time (false if not loaded).
    bool input_shape(int& w, int& h, int& c) const;
    // Wall time of the last rknn_run() call in microseconds (0 if none yet).
    uint64_t last_run_us() const;

private:
    uint32_t inferImpl(const std::vector<uint8_t>&, std::vector<float>&, const std::string&, int);
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::atomic<bool> loaded_{false};
};

} // namespace evs

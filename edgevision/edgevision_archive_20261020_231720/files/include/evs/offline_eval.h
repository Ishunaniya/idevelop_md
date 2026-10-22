#pragma once
#include <map>
#include <string>
#include <vector>

#include "evs/detection_decoder.h"
#include "evs/iinfer_backend.h"
#include "evs/preprocess.h"
#include "evs/status.h" // Target
#include "evs/task_config.h"

namespace evs {

// One ground-truth box, in original-image coordinates (0..1, same convention as
// Target: x/y = center, w/h = size).
struct GtBox {
    int cls = 0;
    float x = 0.f, y = 0.f, w = 0.f, h = 0.f;
};

// A single evaluation image and its manual annotation.
struct EvalImageEntry {
    std::string file;                 // image file name inside images_subdir
    std::vector<GtBox> boxes;         // ground truth
};

// Evaluation configuration. Reuses a standard task YAML (backend / model /
// input dims / decoder) so the offline run exercises the exact same pipeline
// that runs on the device.
struct EvalConfig {
    std::string task_yaml;            // VisionTask config path
    std::string dataset_dir;          // root dir: contains images/ + annotations.json
    std::string images_subdir = "images";
    std::string anno_file = "annotations.json";
    float iou_threshold = 0.5f;       // match threshold for TP/FP/FN
    float confidence_threshold = -1.f; // <0 => use the task YAML confidence_threshold
    bool dump_mismatch = false;       // dump FP/FN/TP images for inspection
    std::string dump_dir = "./eval_dump";
    std::string class_names;          // optional "person,helmet" for reporting
};

struct ClassMetric {
    int tp = 0, fp = 0, fn = 0;
    double precision = 0.0, recall = 0.0, f1 = 0.0, ap = 0.0; // ap = AP@IoU(thr)
};

struct ImageEvalResult {
    std::string file;
    int tp = 0, fp = 0, fn = 0;
};

// Full evaluation report.
struct EvalReport {
    std::string backend;
    std::string Model_path;
    float iou_threshold = 0.5f;
    float confidence_threshold = 0.0f;
    int n_images = 0;
    std::map<int, ClassMetric> per_class;
    double map = 0.0;                 // mean AP over classes that have GT
    int total_tp = 0, total_fp = 0, total_fn = 0;
    double micro_precision = 0.0, micro_recall = 0.0;
    std::vector<ImageEvalResult> images;
    std::string class_names_;         // comma-separated, for reporting only

    // Machine-readable report.
    std::string to_json() const;
    std::string class_names_safe() const { return class_names_; }
    // Write a per-class + summary CSV. Returns true on success.
    bool write_csv(const std::string& path) const;
    // Write a per-image CSV (file, tp, fp, fn). Returns true on success.
    bool write_images_csv(const std::string& path) const;
};

// Offline evaluation engine.
//
//   dataset (photos + annotations) -> Preprocessor -> IInferBackend -> decoder
//   -> match against ground truth (IoU) -> precision / recall / F1 / mAP@0.5.
//
// Backend-agnostic: it uses whatever backend the task YAML selects (x86
// reference, x86+ONNX, mock, rknn on a host with the SDK, ...). This is exactly
// how you validate the algorithm on a laptop with field photos before trusting
// it on the crane.
class OfflineEvaluator {
public:
    // Parse the task YAML + dataset annotations. Returns an EvsStatus code.
    uint32_t load(const EvalConfig& cfg);

    // Run the whole dataset and fill `out`. Returns an EvsStatus code.
    uint32_t run(EvalReport& out);

private:
    EvalConfig cfg_;
    TaskConfig task_;
    Preprocessor pre_;
    std::shared_ptr<IInferBackend> backend_;
    std::unique_ptr<IDetectionDecoder> dec_;
};

} // namespace evs

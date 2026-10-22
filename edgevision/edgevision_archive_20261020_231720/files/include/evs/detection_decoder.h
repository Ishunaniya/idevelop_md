#pragma once
#include <memory>
#include <string>
#include <vector>

#include "evs/status.h" // Target

namespace evs {

// Model-output decoder. Encapsulates a model-specific tensor layout so the
// detection policy (person_plugin) and VisionTask never hardcode one format.
// A backend's raw float tensor is turned into a list of Targets in 0..1 image
// coordinates; ground fields and distance_m / angle_deg remain unknown until
// the shared camera mapper and task-reference stages run.
class IDetectionDecoder {
public:
    virtual ~IDetectionDecoder() = default;

    // Parse `output` into person targets. Implementations must clear `out`
    // first and drop detections below `score_threshold`.
    virtual void decode(const std::vector<float>& output,
                        float score_threshold,
                        std::vector<Target>& out) const = 0;

    // Human-readable format tag (for logs / diagnostics).
    virtual const char* format() const = 0;
};

// Factory: build the decoder for a config-driven output format.
// Returns nullptr for unknown formats so callers can fail fast.
//
// Built-in formats:
//   "flat6"  : per detection [class, conf, x, y, w, h] (legacy single-class
//              person-detector layout, 0..1 image coords). class is not filtered.
//   "yolov8" : [N, 4 + num_classes] rows [x_c, y_c, w, h, s0, s1, ... sC-1]
//              in 0..1 image coords. Each row's argmax class becomes class_id
//              and its score becomes confidence; when person_class_id >= 0 only
//              that class is kept. num_classes defaults to 80 (COCO); pass the
//              real count to match a non-COCO export.
//   "yolo11" : YOLOv11 decoupled head (DFL-16) + 3 scales (stride 8/16/32) +
//              objectness. The RKNN backend concatenates the 9 output tensors
//              (reg/cls/obj x 80/40/20) in model order; this decoder expects
//              float32 (rknn_outputs_get want_float=1). Coordinates are emitted
//              in 0..1 of the model input; vision_task undoes letterbox after.
//              model_in_w/h should match the YAML input_width/height (default
//              640). Aliases: "yolo11_raw", "yolo11_decoup".
//   "yolox"  : YOLOX. The usual export is one blob [anchors, 85], strides
//              8/16/32, y outer and x inner. Per-scale [1, 85, S, S] is also
//              accepted. Grid decode and class-agnostic NMS (IoU 0.5) run in
//              yolo_decode.h. This decoder sees yolov8 rows. Scores are already
//              probabilities unless EVS_YOLOX_SIGMOID=1.
std::unique_ptr<IDetectionDecoder> create_decoder(const std::string& format,
                                                  int person_class_id = -1,
                                                  int num_classes = 80,
                                                  int model_in_w = 640,
                                                  int model_in_h = 640);

} // namespace evs

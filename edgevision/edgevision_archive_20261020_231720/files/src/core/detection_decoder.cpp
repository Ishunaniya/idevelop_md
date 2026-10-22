#include "evs/detection_decoder.h"

#include "evs/person_plugin.h" // decode_person (flat6 layout)
#include "evs_register.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace evs {
namespace {

std::map<std::string, DecoderCreator>& decoders() {
    static std::map<std::string, DecoderCreator> table;
    return table;
}

} // namespace

bool register_decoder(const std::string& format, DecoderCreator creator) {
    if (format.empty() || !creator) return false;
    auto& table = decoders();
    if (table.find(format) != table.end()) return false;
    table.emplace(format, creator);
    return true;
}

void decode_person(const std::vector<float>& output, float score_threshold,
                   std::vector<Target>& out_persons) {
    out_persons.clear();
    const size_t stride = 6;
    if (output.empty() || output.size() % stride != 0) return; // malformed / no detections
    for (size_t i = 0; i + stride <= output.size(); i += stride) {
        if (output[i + 1] < score_threshold) continue; // drop low-confidence
        Target t{};
        t.class_id = static_cast<int>(output[i + 0]);
        t.confidence = output[i + 1];
        t.x = output[i + 2];
        t.y = output[i + 3];
        t.w = output[i + 4];
        t.h = output[i + 5];
        t.distance_m = -1.f; // set by shared ground-metric adaptation
        t.angle_deg = 0.f;
        t.track_id = -1;
        out_persons.push_back(t);
    }
}

namespace {

// flat6: legacy single-class layout, [class, conf, x, y, w, h] x stride.
// Wraps the existing decode_person() so tests and behaviour are unchanged.
class Flat6Decoder : public IDetectionDecoder {
public:
    const char* format() const override { return "flat6"; }
    void decode(const std::vector<float>& output, float score_threshold,
                std::vector<Target>& out) const override {
        decode_person(output, score_threshold, out);
    }
};

// yolov8: [N, 4 + num_classes] where each row is
//   [x_center, y_center, width, height, s0, s1, ... s_{C-1}]
// in 0..1 image coordinates. class_id = argmax score, confidence = that score.
class YoloV8Decoder : public IDetectionDecoder {
public:
    YoloV8Decoder(int person_class_id, int num_classes)
        : person_class_(person_class_id), num_classes_(num_classes) {}

    const char* format() const override { return "yolov8"; }

    void decode(const std::vector<float>& output, float score_threshold,
                std::vector<Target>& out) const override {
        out.clear();
        if (num_classes_ <= 0) return;
        const size_t row_width = static_cast<size_t>(4 + num_classes_);
        if (output.size() < row_width || output.size() % row_width != 0) return;

        for (size_t i = 0; i + row_width <= output.size(); i += row_width) {
            // argmax over class scores
            float best_score = -1.f;
            int best_cls = -1;
            for (int c = 0; c < num_classes_; ++c) {
                const float s = output[i + 4 + static_cast<size_t>(c)];
                if (s > best_score) {
                    best_score = s;
                    best_cls = c;
                }
            }
            if (best_score < score_threshold) continue;
            if (person_class_ >= 0 && best_cls != person_class_) continue;

            Target t{};
            t.class_id = best_cls;
            t.confidence = best_score;
            t.x = output[i + 0];
            t.y = output[i + 1];
            t.w = output[i + 2];
            t.h = output[i + 3];
            t.distance_m = -1.f; // shared ground adaptation fills this
            t.angle_deg = 0.f;
            t.track_id = -1;
            out.push_back(t);
        }
    }

private:
    int person_class_;
    int num_classes_;
};

// yolov11: decoupled head (DFL-16) + 3 scales (stride 8/16/32) + objectness.
// The RKNN backend concatenates the 9 output tensors in model order into
// `output` (each already dequantised to float32):
//   reg_80, cls_80, obj_80, reg_40, cls_40, obj_40, reg_20, cls_20, obj_20
// Layout per head:
//   reg : [1, 4*16, H, W]  (l, t, r, b  x  16 DFL bins)
//   cls : [1, NC,   H, W]
//   obj : [1, 1,    H, W]
// This mirrors X:\apps\yolov5_object_detect\src\yolov11_postprocess.cpp exactly;
// the board yolo11.rknn is byte-identical to that app's reference model, so the
// tensor order / dtype / grids (80/40/20 for 640 input) are guaranteed to match.
// yolo11: the heavy lifting (DFL-16 decode, objectness gating, per-class NMS)
// happens in the backend -- see src/backend/yolo_decode.h::decode_v11_scale --
// because YOLOv11 exports the head as nine separate tensors that only make sense
// as a group. What reaches this decoder is therefore the same row matrix as
// yolov8: [N, 4 + num_classes] with x, y, w, h normalised to 0..1 of the model
// input. Kept as its own format name so a task config can stay explicit about
// which model it runs.
class YoloV11Decoder : public IDetectionDecoder {
public:
    YoloV11Decoder(int person_class_id, int num_classes,
                   int model_in_w, int model_in_h,
                   const char* format_name = "yolo11")
        : person_class_(person_class_id), num_classes_(num_classes),
          model_in_w_(model_in_w > 0 ? model_in_w : 640),
          model_in_h_(model_in_h > 0 ? model_in_h : 640),
          format_name_(format_name) {}

    const char* format() const override { return format_name_; }

    void decode(const std::vector<float>& output, float score_threshold,
                std::vector<Target>& out) const override {
        (void)model_in_w_;  // rows are already normalised to 0..1
        (void)model_in_h_;
        out.clear();
        if (num_classes_ <= 0) return;
        const size_t row_width = static_cast<size_t>(4 + num_classes_);
        if (output.size() < row_width || output.size() % row_width != 0) return;

        for (size_t i = 0; i + row_width <= output.size(); i += row_width) {
            float best_score = -1.f;
            int best_cls = -1;
            for (int c = 0; c < num_classes_; ++c) {
                const float s = output[i + 4 + static_cast<size_t>(c)];
                if (s > best_score) {
                    best_score = s;
                    best_cls = c;
                }
            }
            if (best_score < score_threshold) continue;
            if (person_class_ >= 0 && best_cls != person_class_) continue;

            Target t{};
            t.class_id = best_cls;
            t.confidence = best_score;
            t.x = output[i + 0];
            t.y = output[i + 1];
            t.w = output[i + 2];
            t.h = output[i + 3];
            t.distance_m = -1.f;
            t.angle_deg = 0.f;
            t.track_id = -1;
            out.push_back(t);
        }
    }

private:
    int person_class_;
    int num_classes_;
    int model_in_w_;
    int model_in_h_;
    const char* format_name_;
};

std::unique_ptr<IDetectionDecoder> make_flat6(int, int, int, int) {
    return std::make_unique<Flat6Decoder>();
}

std::unique_ptr<IDetectionDecoder> make_yolov8(int person_class_id, int num_classes,
                                               int, int) {
    return std::make_unique<YoloV8Decoder>(person_class_id, num_classes);
}

std::unique_ptr<IDetectionDecoder> make_yolo11(int person_class_id, int num_classes,
                                               int model_in_w, int model_in_h) {
    return std::make_unique<YoloV11Decoder>(person_class_id, num_classes,
                                            model_in_w, model_in_h);
}

std::unique_ptr<IDetectionDecoder> make_yolox(int person_class_id, int num_classes,
                                              int model_in_w, int model_in_h) {
    return std::make_unique<YoloV11Decoder>(person_class_id, num_classes,
                                            model_in_w, model_in_h, "yolox");
}

const DecoderRegistration kFlat6("flat6", make_flat6);
const DecoderRegistration kYoloV8("yolov8", make_yolov8);
const DecoderRegistration kYolo11("yolo11", make_yolo11);
const DecoderRegistration kYolo11Raw("yolo11_raw", make_yolo11);
const DecoderRegistration kYolo11Decoup("yolo11_decoup", make_yolo11);
const DecoderRegistration kYoloX("yolox", make_yolox);

} // namespace

std::unique_ptr<IDetectionDecoder> create_decoder(const std::string& format,
                                                  int person_class_id,
                                                  int num_classes,
                                                  int model_in_w,
                                                  int model_in_h) {
    const std::string key = format.empty() ? "flat6" : format;
    const auto it = decoders().find(key);
    if (it == decoders().end()) return nullptr;
    return it->second(person_class_id, num_classes, model_in_w, model_in_h);
}

} // namespace evs

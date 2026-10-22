#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace evs {
namespace testing {
class IceReplayAccess;
}

namespace winch_ice {

using ErrorCode = uint32_t;
// Optional wrapping monotonic millisecond clock for image timestamps.
uint32_t monotonic_timestamp_ms();
enum class ImageFormat { RGB, BGR, NV12, Gray };
enum class ResultCode : int32_t { RESULT_NO_RESULT = 0, RESULT_MATCH = 1 };
enum class AlertLevel : int32_t { ALERT_NONE = 0, ALERT_WARN = 1 };
enum class ObjectState { Unknown, Normal, Alarm, Failed };
enum class Reason {
    None,
    ObjectNotFound,
    Unobservable,
    InvalidSurface,
    UnsupportedInput,
    OverlappingBoxes,
    UnexpectedObjectCount
};

struct ImageData {
    const uint8_t* data = nullptr;
    uint32_t width = 0, height = 0;
    uint32_t channels = 3; // RGB/BGR=3; NV12=1. Gray is explicitly unsupported.
    uint32_t timestamp_ms = 0;
    ImageFormat format = ImageFormat::RGB;
    uint32_t stride = 0;   // bytes; 0 means tightly packed
    size_t size_bytes = 0; // 0: caller guarantees size; otherwise verified
};

struct ModelConfig {
    std::string Model_path;
    int32_t input_width = 0, input_height = 0;
    bool warmup = false;
    std::string backend = "rknn";
    // Explicit model contract; no implicit COCO decoder.
    std::string output_format; // yolox, flat6 or yolov8; see docs/winch_ice.md
    int32_t num_classes = 1, winch_class_id = 0;
    std::string input_format = "rgb";
    bool letterbox = true;
};

struct DetectConfig {
    float confidence_threshold = .45f, nms_threshold = .5f;
    float area_ratio = .5f; // inclusive: >= 50%
    int32_t saturation_max = 80, value_min = 180;
};

struct WinchSnowConfig {
    ModelConfig model_config;
    DetectConfig detect_config;
    uint32_t camera_channel = 0;
};

struct Detection {
    uint32_t class_id = 0;
    float confidence = 0.f;
    uint32_t x = 0, y = 0, width = 0, height = 0; // original pixels; width/height
};

struct WinchObjectResult {
    uint32_t winch_id = 0;
    ObjectState state = ObjectState::Unknown;
    Reason reason = Reason::None;
    uint32_t status = 0;
    Detection box; // original pixels; ID is frame-local, not a tracked identity
    uint32_t measurement_timestamp_ms = 0; // metadata for this call, never used for caching
    float ice_ratio = -1.f;
    uint64_t surface_pixels = 0, snow_pixels = 0;
    bool valid = false;
};

struct WinchSnowResult {
    ResultCode result_code = ResultCode::RESULT_NO_RESULT;
    AlertLevel alert_level = AlertLevel::ALERT_NONE;
    std::vector<Detection> snow_detections; // empty: winch boxes are not snow detections
    uint32_t inference_time_ms = 0;
    uint32_t camera_channel = 0xFFFFFFFFu; // plain camera id, not packed bytes
    std::vector<WinchObjectResult> objects;
    Reason reason = Reason::None;
    bool processed = false;
    double processing_time_ms = 0;
    // Call error; object-level validity/status must also be checked.
    uint32_t status = 0;
};

// Customer detect prototype. One synchronous call at a time per instance.
// Each call processes this image, including repeated timestamps. No alarm latching.
// The host owns working-condition gating, retry and alarm persistence.
class Detector {
public:
    Detector();
    ~Detector();
    Detector(const Detector&) = delete;
    Detector& operator=(const Detector&) = delete;
    ErrorCode initialize(const WinchSnowConfig& config);
    ErrorCode detect(const ImageData& image, WinchSnowResult& result);
    ErrorCode updateConfig(const DetectConfig& config);
    void release();

private:
    friend class evs::testing::IceReplayAccess;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Standalone config loader; no temporary YAML files are used by Detector.
ErrorCode load_config(const std::string& path, WinchSnowConfig& out);
} // namespace winch_ice
} // namespace evs

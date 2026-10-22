#pragma once
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
enum class ResultCode : int32_t { RESULT_NO_RESULT = 0, RESULT_MATCH = 1 };
enum class AlertLevel : int32_t { ALERT_NONE = 0, ALERT_WARN = 1 };
struct ImageData {
    const uint8_t* data = nullptr;
    uint32_t width = 0, height = 0;
    uint32_t channels = 3; // Tightly packed RGB24=3 or 8-bit grayscale=1.
    uint32_t timestamp_ms = 0;
};

struct ModelConfig {
    std::string Model_path;
    int32_t input_width = 0, input_height = 0;
    bool warmup = false;
};

struct DetectConfig {
    float confidence_threshold = .45f, nms_threshold = .5f;
    float area_ratio = .5f; // inclusive: >= 50%
};

struct WinchSnowConfig {
    ModelConfig model_config;
    DetectConfig detect_config;
    uint32_t camera_channel = 0;
};

struct Detection {
    uint32_t class_id = 0; // Temporary model contract: 0 = winch, not snow.
    float confidence = 0.f; // Model confidence [0,1], not HSV ice_ratio.
    uint32_t x = 0, y = 0, width = 0, height = 0; // original pixels; width/height
};

struct WinchObjectResult {
    Detection box; // Winch box in original pixels, not a snow model detection.
    float ice_ratio = -1.f; // [0,1] when valid; -1 otherwise.
    bool valid = false;
};

struct WinchSnowResult {
    // Temporary: MATCH means a winch survived business confidence/NMS,
    // independently of whether HSV measurement is valid.
    ResultCode result_code = ResultCode::RESULT_NO_RESULT;
    AlertLevel alert_level = AlertLevel::ALERT_NONE;
    // Temporary winch candidates, before confidence filtering and NMS.
    // Original-image pixel boxes; model order; no top-K/two-object truncation.
    // Name retained for the customer interface; NOT snow-model detections.
    std::vector<Detection> snow_detections;
    uint32_t inference_time_ms = 0;
    uint32_t camera_channel = 0xFFFFFFFFu; // plain camera id, not packed bytes
    std::vector<WinchObjectResult> objects; // Frame-local, sorted from left to right.
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

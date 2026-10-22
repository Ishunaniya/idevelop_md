#pragma once
#include <memory>
#include <string_view>
#include <vector>

#include "evs/debug.h"
#include "evs/frame.h"
#include "evs/preprocess.h"
#include "evs/status.h"
#include "evs/task_config.h"

namespace evs {
namespace winch_ice {
class Detector;
struct WinchSnowResult;
}
namespace testing {
struct IceMaskSnapshot;
} // namespace testing

class VisionTask {
public:
    VisionTask();
    ~VisionTask();

    VisionTask(const VisionTask&) = delete;
    VisionTask& operator=(const VisionTask&) = delete;

    // Read YAML and acquire the configured model.
    // On any failure it rolls back and leaves no half-initialized state.
    // Repeated calls must NOT reload the model.
    uint32_t initialize(std::string_view config_path);
    uint32_t initialize(const TaskConfig& config);

    // Synchronous detection. One call at a time per instance.
    // The library preprocesses `frame` on the CPU; ice ignores ctx.
    uint32_t detect(const Frame& frame, const MachineContext& ctx, Result& out);

    // Same pipeline as detect(), but the host already built the model input
    // (RGA letterbox on the board). `tensor` is packed RGB/BGR, NHWC,
    // inputWidth()*inputHeight()*3. `map` undoes that letterbox back onto
    // `frame`, which still carries the original width, height and camera_id.
    // `preprocess_ms` is the host's preprocess time and is stored as pre.
    uint32_t detectPrepared(const Frame& frame, const MachineContext& ctx,
                            std::vector<uint8_t> tensor, const PreprocessMap& map,
                            double preprocess_ms, Result& out);

    int inputWidth() const;
    int inputHeight() const;
    bool letterbox() const;

    // Update thresholds / zone / switch only. Reject backend / model change.
    uint32_t updateConfig(std::string_view config_path);
    uint32_t updateConfig(const TaskConfig& config);
    bool lastWinchSnowResult(winch_ice::WinchSnowResult& result) const;
    bool copyIceMasks(std::vector<testing::IceMaskSnapshot>& out) const;

    // Return NPU context and caches. Safe to call again from the destructor.
    void release();

    // --- observability / debug API ---------------------------------------
    // These are additive: they never change the behaviour of the four calls
    // above and stay cheap when debug is disabled.

    // Runtime counters + latency statistics of this instance.
    const DebugStats& debugStats() const;

    // JSON snapshot of config, counters and active dump policy (for IPC / log).
    std::string debugSnapshot() const;

    // Hot-enable / retarget debugging without restarting the task.
    void setDebugConfig(const DebugConfig& cfg);

    // Write the last preprocessed tensor to disk (only cached while debugging).
    bool saveLastInput(const std::string& path) const;

private:
    friend class winch_ice::Detector;
    const winch_ice::WinchSnowResult* winchSnowResultView() const noexcept;
    uint32_t rejectWinchSnow(uint32_t status, Result& out) noexcept;
    uint32_t acceptFrame(const Frame& frame) const;
    uint32_t finishDetect(const Frame& frame, const MachineContext& ctx,
                          std::vector<uint8_t>& input, const PreprocessMap& map,
                          double pre_ms, Result& out);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace evs

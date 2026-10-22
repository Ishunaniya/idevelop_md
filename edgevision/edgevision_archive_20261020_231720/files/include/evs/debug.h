#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "evs/frame.h"
#include "evs/log.h"
#include "evs/preprocess.h"
#include "evs/status.h"

namespace evs {

// Runtime debug controls. Everything is off by default: on a deployed device
// dumping costs IO and storage, so it is opt-in per task (YAML or API).
struct DebugConfig {
    bool enabled = false;

    bool dump_input = false;      // save the model input tensor as PPM
    bool dump_meta = true;        // save a JSON sidecar next to it
    bool dump_on_alarm_only = true; // dump only frames that raise an alarm
    int dump_every_n = 0;         // 0 = never periodic; 1 = every frame

    std::string dump_dir = "/tmp/evs_debug";
    int max_dumps = 100;          // safety valve: stop writing after this many
};

// Latency accumulator (milliseconds).
struct TimingStats {
    uint64_t count = 0;
    double last_ms = 0.0;
    double min_ms = 0.0;
    double max_ms = 0.0;
    double avg_ms = 0.0;
    double p95_ms = 0.0;
};

// Per-task runtime counters and latencies. Exposed to the host through the
// debug API so a technician can answer "is it even running?" without gdb.
struct DebugStats {
    uint64_t frames = 0;
    uint64_t errors = 0;
    uint64_t alarms = 0;
    uint64_t over_budget = 0;     // frames slower than latency_budget_ms
    uint32_t last_status = 0;
    AlarmState last_alarm = AlarmState::None;
    int64_t last_timestamp_us = 0;
    std::size_t last_targets = 0;

    TimingStats total;
    TimingStats pre;
    TimingStats infer;
    TimingStats decode;
};

// Per-instance debug assistant. Owns no resources beyond its ring history.
class TaskDebug {
public:
    void configure(const DebugConfig& cfg);
    const DebugConfig& config() const { return cfg_; }

    void reset();
    const DebugStats& stats() const { return st_; }

    // Record one frame's timings and outcome.
    void note(double pre_ms, double infer_ms, double decode_ms, double total_ms,
              uint32_t status, AlarmState alarm, std::size_t n_targets,
              int64_t timestamp_us);

    // Latency budget feedback: incremental average of missed frames.
    void note_budget(bool exceeded) { if (exceeded) ++st_.over_budget; }

    // Write the model input (and its sidecar) when the dump policy says so.
    // Returns true when something was written.
    bool maybe_dump(int orig_w, int orig_h, const PreprocessMap& map,
                    const std::vector<uint8_t>& input,
                    const std::vector<Target>& targets, uint32_t status,
                    AlarmState alarm, int64_t timestamp_us);

    // Machine-readable snapshot of stats + active dump policy.
    std::string snapshot_json() const;

private:
    DebugConfig cfg_;
    DebugStats st_;
    std::vector<double> hist_;  // bounded recent total_ms samples for p95
    uint64_t seq_ = 0;          // frame sequence number
    uint64_t dumps_ = 0;
};

// --- standalone helpers (usable from host code too) ------------------------

// Save RGB packed pixels (w*h*3) as a P6 PPM. Return false on IO failure.
bool save_rgb_ppm(const uint8_t* data, int width, int height, const std::string& path);

// Convert NV12 / RGB / BGR to RGB and save as a P6 PPM (original frame dump).
bool save_frame_ppm(const Frame& frame, const std::string& path);

// Library-wide snapshot: version, compiled backend, registry contents, log
// level and the most recent log lines. Cheap enough to call from IPC handlers.
std::string library_snapshot(std::size_t log_lines = 20);

// Scaffold version string.
const char* library_version();

} // namespace evs

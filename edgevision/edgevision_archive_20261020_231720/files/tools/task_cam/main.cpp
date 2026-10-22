// Host for one VisionTask on a live camera.
// Capture stays here: GStreamer NV12 in, VisionTask::detect out.
// The library does not open /dev/video*.

#include "capture.h"
#include "rga_preprocess.h"

#include "evs/vision_task.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

const char* alarm_name(evs::AlarmState s) {
    switch (s) {
        case evs::AlarmState::None: return "None";
        case evs::AlarmState::LogOnly: return "LogOnly";
        case evs::AlarmState::Alarm: return "Alarm";
    }
    return "?";
}

int64_t now_us() {
    using namespace std::chrono;
    return duration_cast<microseconds>(system_clock::now().time_since_epoch()).count();
}

using Clock = std::chrono::steady_clock;

double ms_since(const Clock::time_point& t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

std::size_t packed_bytes(const std::string& fmt, int w, int h) {
    if (w <= 0 || h <= 0) return 0;
    const std::size_t px = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
    if (fmt == "NV12" || fmt == "NV21" || fmt == "I420") return px * 3 / 2;
    if (fmt == "NV16" || fmt == "NV61" || fmt == "YUY2" || fmt == "UYVY") return px * 2;
    return 0;
}

bool rga_fmt(const std::string& gst, evs_host::RgaSrcFormat& out) {
    if (gst == "NV12") out = evs_host::RgaSrcFormat::NV12;
    else if (gst == "NV16") out = evs_host::RgaSrcFormat::NV16;
    else if (gst == "NV61") out = evs_host::RgaSrcFormat::NV61;
    else if (gst == "YUY2") out = evs_host::RgaSrcFormat::YUYV;
    else if (gst == "UYVY") out = evs_host::RgaSrcFormat::UYVY;
    else return false;
    return true;
}

struct Held {
    std::vector<uint8_t> buf;
    std::vector<uint8_t> tensor;
    evs::PreprocessMap map{};
    int w = 0;
    int h = 0;
    std::string pixel_format;
    double grab_ms = 0;
    double pre_ms = 0;
    bool has_tensor = false;
};

// Newest frame only. The capture thread overwrites a frame the infer thread
// has not taken yet; that is the drop.
class Latest {
public:
    void push(Held h) {
        std::lock_guard<std::mutex> lk(m_);
        slot_ = std::move(h);
        ++published_;
        cv_.notify_one();
    }
    bool pop(Held& out) {
        std::unique_lock<std::mutex> lk(m_);
        cv_.wait_for(lk, std::chrono::milliseconds(3000), [&] {
            return stop_ || published_ != taken_;
        });
        if (published_ == taken_) return false;
        out = std::move(slot_);
        const uint64_t skipped = published_ - taken_ - 1;
        dropped_ += skipped;
        taken_ = published_;
        ++consumed_;
        return true;
    }
    void stop() {
        std::lock_guard<std::mutex> lk(m_);
        stop_ = true;
        cv_.notify_all();
    }
    uint64_t dropped() const {
        std::lock_guard<std::mutex> lk(m_);
        return dropped_;
    }

private:
    mutable std::mutex m_;
    std::condition_variable cv_;
    Held slot_;
    uint64_t published_ = 0;
    uint64_t taken_ = 0;
    uint64_t consumed_ = 0;
    uint64_t dropped_ = 0;
    bool stop_ = false;
};

void usage(const char* argv0) {
    std::fprintf(stderr,
                 "usage: %s --task <yaml> [--config camera_config.json]\n"
                 "          [--channel N] [--device /dev/video0]\n"
                 "          [--width W --height H --fps N] [--frames N]\n"
                 "\n"
                 "  Opens one camera, builds one VisionTask from the YAML, and\n"
                 "  runs detect() on packed NV12. --channel selects the camera\n"
                 "  entry and is also Frame.camera_id, so it must be listed in\n"
                 "  the YAML video_channel.\n"
                 "  MachineContext is fixed: context_valid=1, is_working=1.\n",
                 argv0);
}

} // namespace

int main(int argc, char** argv) {
    std::string task, config_path, device = "/dev/video0";
    int channel = 0;
    int width = 1280, height = 720, fps = 25;
    int frames = 5;
    bool have_channel = false;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&](const char* def) -> std::string {
            return (i + 1 < argc) ? std::string(argv[++i]) : std::string(def);
        };
        if (a == "--task") task = next("");
        else if (a == "--config") config_path = next("");
        else if (a == "--channel") {
            channel = std::atoi(next("0").c_str());
            have_channel = true;
        } else if (a == "--device") device = next("/dev/video0");
        else if (a == "--width") width = std::atoi(next("1280").c_str());
        else if (a == "--height") height = std::atoi(next("720").c_str());
        else if (a == "--fps") fps = std::atoi(next("25").c_str());
        else if (a == "--frames") frames = std::atoi(next("5").c_str());
        else {
            usage(argv[0]);
            return 1;
        }
    }
    if (task.empty() || frames < 1) {
        usage(argv[0]);
        return 1;
    }

    if (!config_path.empty()) {
        const auto cfg = cap::load_capture_config(config_path);
        const cap::CamEntry* e = cap::select_entry(cfg, have_channel ? channel : -1);
        if (!e) {
            std::fprintf(stderr, "no camera entry in %s for channel %d\n",
                         config_path.c_str(), channel);
            return 2;
        }
        device = e->device;
        width = e->width;
        height = e->height;
        if (e->framerate > 0) fps = e->framerate;
        if (!have_channel) channel = e->channel;
        std::fprintf(stderr, "[config] channel %d device %s %dx%d @%d\n",
                     channel, device.c_str(), width, height, fps);
    }

    evs::VisionTask vision;
    const uint32_t ir = vision.initialize(task);
    if (ir != 0u) {
        std::fprintf(stderr, "VisionTask initialize failed status=0x%08X yaml=%s\n",
                     ir, task.c_str());
        return 3;
    }
    std::printf("VisionTask ready yaml=%s camera_id=%d context_valid=1 is_working=1\n",
                task.c_str(), channel);
    std::fflush(stdout);

    const int model_w = vision.inputWidth();
    const int model_h = vision.inputHeight();
    const bool letterbox = vision.letterbox();
    if (model_w <= 0 || model_h <= 0) {
        std::fprintf(stderr, "task has no model input size\n");
        return 3;
    }

    cap::GstCapture cap;
    if (!cap.open(device, width, height, fps, true)) {
        std::fprintf(stderr, "cannot open camera %s\n", device.c_str());
        return 2;
    }
    std::fprintf(stderr, "[gst] capturing %s %dx%d @%dfps\n",
                 device.c_str(), width, height, fps);

    const auto wall0 = Clock::now();
    cap::CapFrame g0_frame;
    const auto g0_t = Clock::now();
    if (!cap.grab(g0_frame) || !g0_frame.data) {
        std::fprintf(stderr, "frame 0 grab failed\n");
        return 4;
    }
    const double frame0_grab_ms = ms_since(g0_t);

    Held first;
    first.buf = std::move(g0_frame.buf);
    first.w = g0_frame.w;
    first.h = g0_frame.h;
    first.pixel_format = g0_frame.pixel_format.empty() ? "NV12" : g0_frame.pixel_format;
    first.grab_ms = frame0_grab_ms;
    first.has_tensor = false;

    const bool use_rga = evs_host::rga_available();
    std::fprintf(stderr, "[task_cam] preprocess=%s format=%s %dx%d\n",
                 use_rga ? "rga" : "cpu", first.pixel_format.c_str(),
                 first.w, first.h);

    evs::MachineContext ctx{};
    ctx.context_valid = true;
    ctx.is_working = 1;

    Latest box;
    std::atomic<bool> stop{false};
    std::thread grabber([&] {
        while (!stop.load()) {
            cap::CapFrame g;
            const auto g0 = Clock::now();
            if (!cap.grab_for(g, 200) || !g.data) continue;
            Held h;
            h.buf = std::move(g.buf);
            h.w = g.w;
            h.h = g.h;
            h.pixel_format = g.pixel_format;
            h.grab_ms = ms_since(g0);
            box.push(std::move(h));
        }
    });

    double sum_grab_ms = 0.0;
    double sum_detect_ms = 0.0;
    int ok_frames = 0;
    auto run_one = [&](int i, Held& held) -> int {
        if (held.buf.empty() || held.w <= 0 || held.h <= 0) {
            std::fprintf(stderr, "frame %d empty\n", i);
            return 4;
        }
        evs::Frame frame{};
        frame.camera_id = channel;
        frame.width = held.w;
        frame.height = held.h;
        frame.stride = held.w;
        frame.format = evs::PixelFormat::NV12;
        frame.data = held.buf.data();
        frame.timestamp_us = now_us();
        ctx.timestamp_us = frame.timestamp_us;

        evs::Result res{};
        uint32_t dr = 0;
        if (use_rga && !held.has_tensor) {
            evs_host::RgaSrcFormat rf{};
            const auto p0 = Clock::now();
            if (!rga_fmt(held.pixel_format, rf) ||
                evs_host::rga_preprocess_yuv(held.buf.data(), held.w, held.h, rf,
                                             model_w, model_h, letterbox,
                                             held.tensor, held.map) != 0u) {
                std::fprintf(stderr, "frame %d rga failed: %s\n", i,
                             evs_host::rga_last_error());
                return 5;
            }
            held.pre_ms = ms_since(p0);
            held.has_tensor = true;
        }
        const auto d0 = Clock::now();
        if (use_rga) {
            dr = vision.detectPrepared(frame, ctx, std::move(held.tensor), held.map,
                                       held.pre_ms, res);
        } else {
            dr = vision.detect(frame, ctx, res);
        }
        const double detect_ms = ms_since(d0);
        if (dr != 0u) {
            std::printf("frame %d detect status=0x%08X detect=%.1fms\n", i, dr, detect_ms);
            std::fflush(stdout);
            return 5;
        }
        ++ok_frames;
        sum_grab_ms += held.grab_ms;
        sum_detect_ms += detect_ms;
        const evs::DebugStats& st = vision.debugStats();
        std::printf("frame %d %dx%d targets=%zu alarm=%s status=0x%08X "
                    "grab=%.1fms detect=%.1fms (pre=%.1f infer=%.1f decode=%.1f)\n",
                    i, held.w, held.h, res.targets.size(),
                    alarm_name(res.alarm), res.status, held.grab_ms, detect_ms,
                    st.pre.last_ms, st.infer.last_ms, st.decode.last_ms);
        for (const evs::Target& t : res.targets) {
            std::printf("  cls=%d conf=%.3f center=(%.3f,%.3f) wh=(%.3f,%.3f) "
                        "distance_m=%.3f angle_deg=%.2f\n",
                        t.class_id, t.confidence, t.x, t.y, t.w, t.h,
                        t.distance_m, t.angle_deg);
        }
        std::fflush(stdout);
        return 0;
    };

    int rc = run_one(0, first);
    const auto steady0 = Clock::now();
    for (int i = 1; rc == 0 && i < frames; ++i) {
        Held held;
        if (!box.pop(held)) {
            std::fprintf(stderr, "frame %d grab failed\n", i);
            rc = 4;
            break;
        }
        rc = run_one(i, held);
    }
    const double steady_ms = ms_since(steady0);
    stop.store(true);
    box.stop();
    grabber.join();
    cap.close();

    const double steady_fps = (ok_frames > 1 && steady_ms > 0.0)
                                  ? (ok_frames - 1) * 1000.0 / steady_ms
                                  : 0.0;
    const double total_wall_ms = ms_since(wall0);
    const double total_fps = (total_wall_ms > 0.0) ? ok_frames * 1000.0 / total_wall_ms : 0.0;
    const evs::DebugStats& st = vision.debugStats();
    std::printf("done frames=%d/%d steady_fps=%.2f (all_fps=%.2f incl. warmup %.0fms) drop=%llu\n"
                "  steady avg: grab=%.1fms detect=%.1fms (pre=%.1fms infer=%.1fms decode=%.1fms)\n",
                ok_frames, frames, steady_fps, total_fps, frame0_grab_ms,
                static_cast<unsigned long long>(box.dropped()),
                (ok_frames > 1) ? (sum_grab_ms - first.grab_ms) / (ok_frames - 1) : first.grab_ms,
                ok_frames ? sum_detect_ms / ok_frames : 0.0,
                st.pre.avg_ms, st.infer.avg_ms, st.decode.avg_ms);
    return rc != 0 ? rc : (ok_frames == frames ? 0 : 6);
}

// capture.h -- image capture layer for v4l2_detect.
//
// The infer side only ever calls FrameSource::grab(). Where the pixels come
// from is the source's business, and every source returns tightly packed NV12
// (stride == width) so the Preprocessor can consume it with no extra colour
// conversion:
//   1) GstCapture          : live pipeline   v4l2src -> NV12 -> appsink
//   2) Mp4KeyframeCapture  : an MP4 file, keyframes only (sync samples)
//   3) ShmCapture          : POSIX shm "/picshare<channel>" written by
//                            qt5_qml_c001 (BGR CV_8UC3 -> NV12 converted here)
//
// Another source is a new FrameSource. The module depends ONLY on GStreamer +
// POSIX shm + standard C++; it does not touch any evs internal type.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cap {

enum class CapFmt { NV12, BGR };

struct CapFrame {
    const uint8_t* data = nullptr;  // points into buf (valid until next grab/close)
    int w = 0, h = 0;
    CapFmt fmt = CapFmt::NV12;
    std::string pixel_format;       // GStreamer caps format, e.g. "NV16"
    std::vector<uint8_t> buf;       // backing storage owned by the capture object
};

struct CamEntry {
    std::string device = "/dev/video0";
    int channel = 0;
    bool enable = false;
    int width = 1280, height = 720, framerate = 25;
};

// Minimal fixed-schema JSON loader for our own camera_config.json:
//   [ { "device":..., "channel":N, "enable":bool, "width":W, "height":H,
//       "framerate":F, ... }, ... ]
// No third-party JSON lib required.
std::vector<CamEntry> load_capture_config(const std::string& path);

// Pick the entry to use: --channel if given, else first enabled, else first.
const CamEntry* select_entry(const std::vector<CamEntry>& cfg, int channel_hint);

// One grab() returns one NV12 frame. The capture thread and the inline loop
// both talk to this and nothing else, so a live camera and a file look the
// same to inference.
class FrameSource {
public:
    virtual ~FrameSource() = default;
    virtual bool grab(CapFrame& out) = 0;
    virtual void close() = 0;
    virtual int width() const = 0;
    virtual int height() const = 0;
};

class GstCapture : public FrameSource {
public:
    // drop_frames selects the appsink policy, which is what decides the
    // end-to-end latency under load:
    //   true  (default) -- appsink drop=true max-buffers=1
    //           The queue is depth 1 and OVERWRITTEN when full: when inference
    //           (75ms on RK3576) is slower than the camera (25fps), the
    //           consumer always gets the NEWEST frame. Latency stays bounded
    //           at <1 frame + inference; frames are dropped on purpose.
    //   false -- drop=false max-buffers=4, the classic "never lose a frame"
    //           mode. Suitable for offline bring-up / benchmarking ONLY: under
    //           sustained load the queue fills and latency grows without bound
    //           (the picture lags further and further behind reality).
    // native_format empty: convert to packed NV12 (I420 in the middle so the
    // camera is not asked for NV12, which this node fails to allocate).
    // native_format "NV16" / "NV61" / "YUY2" / "UYVY": no videoconvert. The
    // buffer is the camera's own packing; width and height come from the caps.
    bool open(const std::string& device, int w, int h, int fps,
              bool drop_frames = true, const char* native_format = nullptr);
    bool grab(CapFrame& out) override;   // out.buf holds the negotiated pixels
    bool grab_for(CapFrame& out, int timeout_ms);
    void close() override;
    int width() const override { return w_; }
    int height() const override { return h_; }
    ~GstCapture() override { close(); }
private:
    void* pipeline_ = nullptr;  // GstElement*
    void* appsink_ = nullptr;   // GstAppSink*
    int w_ = 0, h_ = 0;
};

// MP4 (or MOV) file. Non-keyframes are dropped before decode, so grab() yields
// only sync samples. On end-of-stream the file seeks back to the first
// keyframe when loop is true, which lets a recording stand in for the camera.
class Mp4KeyframeCapture : public FrameSource {
public:
    bool open(const std::string& path, bool loop = true);
    bool grab(CapFrame& out) override;
    void close() override;
    int width() const override { return w_; }
    int height() const override { return h_; }
    ~Mp4KeyframeCapture() override { close(); }
private:
    bool pull(CapFrame& out, bool allow_loop);
    struct Mp4Pipe* impl_ = nullptr;
    CapFrame pending_;
    bool has_pending_ = false;
    int w_ = 0, h_ = 0;
    bool loop_ = true;
};

class ShmCapture : public FrameSource {
public:
    bool open(int channel, int w, int h);
    bool grab(CapFrame& out) override;   // out.buf holds NV12 (BGR->NV12 done internally)
    void close() override;
    int width() const override { return w_; }
    int height() const override { return h_; }
    ~ShmCapture() override { close(); }
private:
    int fd_ = -1;
    void* mem_ = nullptr;       // mmap of /picshare<channel>
    size_t sz_ = 0;
    int w_ = 0, h_ = 0;
    uint64_t last_seq_ = 0;
};

} // namespace cap

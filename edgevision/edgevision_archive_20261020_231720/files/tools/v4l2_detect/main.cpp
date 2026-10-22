// Real-camera bring-up tool: grab frames from a V4L2 node and run the exact
// production pipeline on them (Preprocessor -> backend -> decoder -> targets).
//
// Why a separate tool instead of eval_offline: eval_offline needs a labelled
// dataset, and a live camera has no ground truth. This one reports the raw
// detections, per-stage timing and (optionally) a PPM snapshot with the boxes
// drawn, which is what you need when bringing a model onto a board.
//
// The capture layer is a FrameSource (live pipeline, MP4 keyframes, shared
// memory, or legacy V4L2). It is a bring-up aid, not part of the library.
// Video plumbing stays outside the algorithm library per the project boundary.
//
//   v4l2_detect --task configs/eval_rknn.yaml --device /dev/video0 \
//               --width 1280 --height 720 --frames 5 --score 0.25 \
//               --save /tmp/shot.ppm
//
// Exit codes: 0 ok, 1 usage/config error, 2 camera open/format error,
//            3 pipeline error.

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <unistd.h>
#include <linux/videodev2.h>

// Some vendor / older kernel headers omit the multi-planar ioctl numbers even
// though they are stable kernel ABI, and keep the mmap offset inside a *named*
// union `m` instead of an anonymous one. Bridge both so the tool builds against
// any aarch64 sysroot (verified with both the Rockchip SDK gcc 10.3 sysroot and
// Ubuntu's linux-libc-dev-arm64-cross headers).
#ifndef VIDIOC_S_FMT_MPLANE
// NOTE: _IOW, *not* _IOWR. The kernel uses _IOW for S_FMT_MPLANE; getting the
// direction wrong changes the ioctl number and the driver answers EINVAL.
#define VIDIOC_S_FMT_MPLANE _IOW('V', 94, struct v4l2_format)
#endif
#ifndef VIDIOC_G_FMT_MPLANE
#define VIDIOC_G_FMT_MPLANE _IOWR('V', 106, struct v4l2_format)
#endif

#include <cerrno>
#include <chrono>
#include <algorithm>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <condition_variable>
#include <mutex>
#include <zlib.h>

#include "evs/backend_factory.h"
#include "evs/detection_decoder.h"
#include "evs/log.h"
#include "evs/preprocess.h"
#include "evs/task_config.h"
#include "rga_preprocess.h"
#include "capture.h"
#include "detection_shm.h"

#if defined(EVS_BACKEND_RKNN)
#include "evs/rknn_backend.h"
#endif

namespace {

// ---------------------------------------------------------------------------
// LatestFrame -- single-producer / single-consumer frame mailbox.
//
// Why this exists: inference on RK3576 takes ~75ms while the camera delivers a
// frame every 40ms (25fps). Running grab() and infer() in one thread couples
// them: the sensor overruns the consumer and something has to give -- either the
// queue grows (latency accumulates, the picture drifts further and further
// behind reality) or frames are dropped. With this mailbox the producer NEVER
// blocks and the consumer ALWAYS ends up on the newest frame:
//
//   capture thread : grab -> memcpy into the free slot -> publish (seq++)
//   infer thread   : wait for a new seq -> copy the pending slot out ->
//                    preprocess / infer / decode / publish
//
// Dropping the stale frames in between is intentional, not a defect: for a live
// monitor "the newest picture" beats "every picture, increasingly late".
//
// Slot discipline: `head_` is the slot holding the newest published frame,
// `tail_` the oldest not yet consumed. The producer writes at
// (head_+1)%3 AFTER publishing head_, so it can never overwrite the frame the
// consumer is reading; when producer and consumer are 3 frames apart the
// mailbox is full and the producer overwrites the oldest, which is precisely
// the "drop stale frames" policy. Geometry travels with the frame (stored per
// slot) because a resolution change must not be paired with the wrong size.
// ---------------------------------------------------------------------------
class LatestFrame {
public:
    // Allocates the slots. Call once, before the threads start.
    void configure(size_t bytes, int w, int h) {
        std::lock_guard<std::mutex> lk(m_);
        slots_.assign(3, std::vector<uint8_t>(bytes));
        geom_.assign(3, std::make_pair(w, h));
        head_ = tail_ = 0;
        ready_ = false;
        published_ = 0;
        consumed_ = 0;
    }

    // Producer side. Overwrites the oldest slot when inference cannot keep up
    // (see the class comment): the frame the consumer never reached is dropped
    // on purpose so it always ends up on the newest picture.
    void push(const uint8_t* data, size_t bytes, uint32_t w, uint32_t h) {
        if (!data || bytes == 0) return;
        std::lock_guard<std::mutex> lk(m_);
        // Write into the slot AFTER the current head, then make it the head.
        const size_t next = (head_ + 1) % slots_.size();
        const size_t n = std::min(bytes, slots_[next].size());
        std::memcpy(slots_[next].data(), data, n);
        geom_[next] = std::make_pair(static_cast<int>(w), static_cast<int>(h));
        head_ = next;
        ++published_;
        ready_ = true;
        cv_.notify_one();
    }

    // Frames the sensor produced but inference never consumed -- i.e. how far
    // behind the NPU is running. This is the honest drop number for --stats:
    // it is measured at the point where frames are actually discarded, not
    // inferred from the appsink.
    unsigned long long dropped() const {
        std::lock_guard<std::mutex> lk(m_);
        return published_ > consumed_ ? published_ - consumed_ : 0;
    }
    unsigned long long published() const {
        std::lock_guard<std::mutex> lk(m_);
        return published_;
    }
    unsigned long long consumed() const {
        std::lock_guard<std::mutex> lk(m_);
        return consumed_;
    }

    // Consumer side.
    //   out  receives a COPY of the frame (the caller preprocesses from it)
    //   seq  receives the publish counter, for latency/drop accounting
    // Returns false on timeout (no new frame within 500ms) or after stop().
    bool wait_and_copy(std::vector<uint8_t>& out, uint32_t& w, uint32_t& h,
                       uint64_t* seq) {
        std::unique_lock<std::mutex> lk(m_);
        cv_.wait_for(lk, std::chrono::milliseconds(500), [&] {
            return stop_ || (ready_ && published_ != consumed_);
        });
        if (stop_) return false;
        if (!ready_ || published_ == consumed_) return false;
        // The oldest frame the consumer has not seen yet.
        out = slots_[tail_];
        w = static_cast<uint32_t>(geom_[tail_].first);
        h = static_cast<uint32_t>(geom_[tail_].second);
        tail_ = (tail_ + 1) % slots_.size();
        ++consumed_;
        if (seq) *seq = published_;
        return true;
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lk(m_);
            stop_ = true;
        }
        cv_.notify_all();
    }

private:
    mutable std::mutex m_;
    std::condition_variable cv_;
    // NOTE: `std::vector<std::vector<uint8_t>> x{3, {}}` is ambiguous in C++11
    // (it could match both the (count, value) and the (count, allocator)
    // constructors), so the three slots are resized in configure() instead of
    // in a member initialiser.
    std::vector<std::vector<uint8_t>> slots_;
    std::vector<std::pair<int, int>> geom_;
    size_t head_ = 0, tail_ = 0;
    uint64_t published_ = 0;
    uint64_t consumed_ = 0;
    bool ready_ = false;
    bool stop_ = false;
};

struct Buffer {
    void* start = nullptr;
    size_t len = 0;
    int fd = -1; // dma-buf fd from VIDIOC_EXPBUF, valid only in --rga-fd mode
};

struct Camera {
    int fd = -1;
    uint32_t width = 0, height = 0;
    uint32_t bytesperline = 0;
    std::vector<Buffer> bufs;
    std::vector<uint8_t> frame; // one dequeued frame, still in the driver's format

    ~Camera() {
        for (auto& b : bufs)
            if (b.start) munmap(b.start, b.len);
        if (fd >= 0) close(fd);
    }
};

std::string fourcc_str(uint32_t f) {
    char s[5] = {static_cast<char>(f & 0xff), static_cast<char>((f >> 8) & 0xff),
                 static_cast<char>((f >> 16) & 0xff), static_cast<char>((f >> 24) & 0xff), 0};
    return std::string(s);
}

int xioctl(int fd, unsigned long req, void* arg) {
    int r = ioctl(fd, req, arg);
    if (r < 0 && errno == EINTR) r = ioctl(fd, req, arg);
    return r;
}

// offset/ length live in union `m` on older headers and directly in anonymous
// unions on newer ones -- resolve at compile time.
template <typename T>
auto buf_offset_impl(const T& b, int) -> decltype(b.m.offset) {
    return static_cast<unsigned long>(b.m.offset);
}
template <typename T>
auto buf_offset_impl(const T& b, long) -> decltype(b.offset) {
    return static_cast<unsigned long>(b.offset);
}
template <typename T>
unsigned long buf_offset(const T& b) {
    return buf_offset_impl(b, 0);
}

// Opens the node and configures a capture format the driver accepts.
// NV12 first (CSI-2 nodes on RK3576 speak NV12 natively and our Preprocessor
// consumes NV12 directly, so no colour conversion is needed before the model).
bool camera_open(Camera& cam, const std::string& dev, uint32_t want_w, uint32_t want_h) {
    cam.fd = ::open(dev.c_str(), O_RDWR | O_NONBLOCK, 0);
    if (cam.fd < 0) {
        std::fprintf(stderr, "open %s failed: %s\n", dev.c_str(), std::strerror(errno));
        return false;
    }
    v4l2_capability cap{};
    if (xioctl(cam.fd, VIDIOC_QUERYCAP, &cap) < 0) {
        std::fprintf(stderr, "VIDIOC_QUERYCAP failed: %s\n", std::strerror(errno));
        return false;
    }
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) &&
        !(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)) {
        std::fprintf(stderr, "%s is not a capture device\n", dev.c_str());
        return false;
    }
    const bool mplane = (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE) != 0;

    const uint32_t kFmt = V4L2_PIX_FMT_NV12;
    if (mplane) {
        v4l2_format fmt{};
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        fmt.fmt.pix.width = want_w;
        fmt.fmt.pix.height = want_h;
        fmt.fmt.pix.pixelformat = kFmt;
        fmt.fmt.pix.field = V4L2_FIELD_ANY;
        // Some CSI-2 capture nodes are locked to their native resolution/format
        // and reject S_FMT outright. Fall back to whatever the driver reports,
        // as long as it is NV12 (our Preprocessor consumes NV12 directly).
        if (xioctl(cam.fd, VIDIOC_S_FMT_MPLANE, &fmt) < 0) {
            std::fprintf(stderr, "S_FMT_MPLANE %ux%u NV12 rejected (%s), "
                         "falling back to the current format\n",
                         want_w, want_h, std::strerror(errno));
        }
        // Read the effective format back either way: the driver is free to clamp
        // width/height/bytesperline, and we must letterbox to what we actually get.
        std::memset(&fmt, 0, sizeof(fmt));
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        if (xioctl(cam.fd, VIDIOC_G_FMT_MPLANE, &fmt) < 0) {
            std::fprintf(stderr, "G_FMT_MPLANE failed: %s\n", std::strerror(errno));
            return false;
        }
        if (fmt.fmt.pix.pixelformat != kFmt) {
            std::fprintf(stderr,
                         "node is '%s', not NV12 -- point --device at a node that "
                         "gives NV12 (see v4l2-ctl --get-fmt-video)\n",
                         fourcc_str(fmt.fmt.pix.pixelformat).c_str());
            return false;
        }
        cam.width = fmt.fmt.pix.width;
        cam.height = fmt.fmt.pix.height;
        cam.bytesperline = fmt.fmt.pix.bytesperline;
    } else {
        v4l2_format fmt{};
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width = want_w;
        fmt.fmt.pix.height = want_h;
        fmt.fmt.pix.pixelformat = kFmt;
        fmt.fmt.pix.field = V4L2_FIELD_ANY;
        if (xioctl(cam.fd, VIDIOC_S_FMT, &fmt) < 0) {
            std::fprintf(stderr, "S_FMT NV12 failed: %s\n", std::strerror(errno));
            return false;
        }
        cam.width = fmt.fmt.pix.width;
        cam.height = fmt.fmt.pix.height;
        cam.bytesperline = fmt.fmt.pix.bytesperline;
    }

    v4l2_requestbuffers req{};
    req.count = 4;
    req.type = mplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (xioctl(cam.fd, VIDIOC_REQBUFS, &req) < 0 || req.count < 2) {
        std::fprintf(stderr, "REQBUFS failed: %s\n", std::strerror(errno));
        return false;
    }
    cam.bufs.resize(req.count);
    for (uint32_t i = 0; i < req.count; ++i) {
        v4l2_buffer b{};
        b.type = req.type;
        b.memory = V4L2_MEMORY_MMAP;
        b.index = i;
        if (xioctl(cam.fd, VIDIOC_QUERYBUF, &b) < 0) {
            std::fprintf(stderr, "QUERYBUF failed: %s\n", std::strerror(errno));
            return false;
        }
        void* p = mmap(nullptr, b.length, PROT_READ | PROT_WRITE, MAP_SHARED, cam.fd,
                       static_cast<off_t>(buf_offset(b)));
        if (p == MAP_FAILED) {
            std::fprintf(stderr, "mmap failed: %s\n", std::strerror(errno));
            return false;
        }
        cam.bufs[i].start = p;
        cam.bufs[i].len = b.length;
        v4l2_buffer qb{};
        qb.type = req.type;
        qb.memory = V4L2_MEMORY_MMAP;
        qb.index = i;
        if (xioctl(cam.fd, VIDIOC_QBUF, &qb) < 0) {
            std::fprintf(stderr, "QBUF failed: %s\n", std::strerror(errno));
            return false;
        }
    }
    v4l2_buffer type{};
    type.type = req.type;
    if (xioctl(cam.fd, VIDIOC_STREAMON, &type) < 0) {
        std::fprintf(stderr, "STREAMON failed: %s\n", std::strerror(errno));
        return false;
    }
    // NV12 has no per-line padding in practice; keep bytesperline*height if given.
    const size_t need = static_cast<size_t>(cam.bytesperline) * cam.height;
    if (need > cam.bufs[0].len) cam.frame.resize(need);
    else cam.frame.resize(need ? need : cam.bufs[0].len);
    std::fprintf(stderr, "[v4l2] %s %ux%u bytesperline=%u mplane=%d\n", dev.c_str(),
                 cam.width, cam.height, cam.bytesperline, mplane ? 1 : 0);
    return true;
}

bool camera_grab(Camera& cam, int* out_index = nullptr, bool keep_cpu = true) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(cam.fd, &fds);
    timeval tv{};
    tv.tv_sec = 3;
    const int r = select(cam.fd + 1, &fds, nullptr, nullptr, &tv);
    if (r <= 0) {
        std::fprintf(stderr, "select timed out waiting for a frame\n");
        return false;
    }
    v4l2_buffer b{};
    b.type = (cam.bufs.empty()) ? V4L2_BUF_TYPE_VIDEO_CAPTURE
                                : V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(cam.fd, VIDIOC_DQBUF, &b) < 0) {
        std::fprintf(stderr, "DQBUF failed: %s\n", std::strerror(errno));
        return false;
    }
    if (out_index) *out_index = b.index;
    const size_t bytes = b.bytesused > 0 ? b.bytesused : cam.frame.size();
    const size_t n = bytes < cam.frame.size() ? bytes : cam.frame.size();
    if (keep_cpu) std::memcpy(cam.frame.data(), cam.bufs[b.index].start, n);
    // When keep_cpu is false the caller consumes the dequeued dma-buf fd and
    // must return it with camera_queue(); we deliberately do NOT re-queue here.
    if (keep_cpu) {
        v4l2_buffer qb{};
        qb.type = b.type;
        qb.memory = V4L2_MEMORY_MMAP;
        qb.index = b.index;
        xioctl(cam.fd, VIDIOC_QBUF, &qb);
    }
    return true;
}

// Export every capture buffer as a PRIME dma-buf fd (VIDIOC_EXPBUF) so the RGA
// can import it directly. Returns false if any export fails.
bool camera_export_buffers(Camera& cam) {
    for (uint32_t i = 0; i < cam.bufs.size(); ++i) {
        v4l2_exportbuffer eb{};
        eb.type = (cam.bufs.empty()) ? V4L2_BUF_TYPE_VIDEO_CAPTURE
                                     : V4L2_BUF_TYPE_VIDEO_CAPTURE;
        eb.index = i;
        eb.flags = O_RDWR;
        if (xioctl(cam.fd, VIDIOC_EXPBUF, &eb) < 0) {
            std::fprintf(stderr, "EXPBUF[%u] failed: %s\n", i, std::strerror(errno));
            return false;
        }
        cam.bufs[i].fd = eb.fd;
    }
    return true;
}

// Re-queue a buffer that was dequeued with camera_grab(..., keep_cpu=false).
void camera_queue(Camera& cam, int index) {
    v4l2_buffer qb{};
    qb.type = (cam.bufs.empty()) ? V4L2_BUF_TYPE_VIDEO_CAPTURE
                                 : V4L2_BUF_TYPE_VIDEO_CAPTURE;
    qb.memory = V4L2_MEMORY_MMAP;
    qb.index = index;
    xioctl(cam.fd, VIDIOC_QBUF, &qb);
}

// BT.601 limited-range NV12 -> RGB8, so the snapshot can be eyeballed.
void nv12_to_rgb(const uint8_t* nv12, int w, int h, std::vector<uint8_t>& rgb) {
    rgb.assign(static_cast<size_t>(w) * h * 3, 0);
    const uint8_t* y = nv12;
    const uint8_t* uv = nv12 + static_cast<size_t>(w) * h;
    for (int row = 0; row < h; ++row) {
        for (int col = 0; col < w; ++col) {
            const int yy = y[row * w + col] - 16;
            const int idx = (row / 2) * (w / 2) + (col / 2);
            const int uu = uv[idx * 2 + 0] - 128;
            const int vv = uv[idx * 2 + 1] - 128;
            int r = (298 * yy + 409 * vv + 128) >> 8;
            int g = (298 * yy - 100 * uu - 208 * vv + 128) >> 8;
            int b = (298 * yy + 516 * uu + 128) >> 8;
            r = r < 0 ? 0 : (r > 255 ? 255 : r);
            g = g < 0 ? 0 : (g > 255 ? 255 : g);
            b = b < 0 ? 0 : (b > 255 ? 255 : b);
            const size_t o = (static_cast<size_t>(row) * w + col) * 3;
            rgb[o] = static_cast<uint8_t>(r);
            rgb[o + 1] = static_cast<uint8_t>(g);
            rgb[o + 2] = static_cast<uint8_t>(b);
        }
    }
}

void draw_box(std::vector<uint8_t>& rgb, int w, int h, float cx, float cy,
              float bw, float bh, uint8_t cr, uint8_t cg, uint8_t cb) {
    const int x1 = static_cast<int>((cx - bw / 2.f) * w);
    const int x2 = static_cast<int>((cx + bw / 2.f) * w);
    const int y1 = static_cast<int>((cy - bh / 2.f) * h);
    const int y2 = static_cast<int>((cy + bh / 2.f) * h);
    for (int x = std::max(0, x1); x < std::min(w, x2); ++x) {
        for (int yy : {y1, y2 - 1}) {
            if (yy < 0 || yy >= h) continue;
            const size_t o = (static_cast<size_t>(yy) * w + x) * 3;
            rgb[o] = cr; rgb[o + 1] = cg; rgb[o + 2] = cb;
        }
    }
    for (int yy = std::max(0, y1); yy < std::min(h, y2); ++yy) {
        for (int x : {x1, x2 - 1}) {
            if (x < 0 || x >= w) continue;
            const size_t o = (static_cast<size_t>(yy) * w + x) * 3;
            rgb[o] = cr; rgb[o + 1] = cg; rgb[o + 2] = cb;
        }
    }
}

bool save_ppm(const std::string& path, const std::vector<uint8_t>& rgb, int w, int h) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    std::fprintf(f, "P6\n%d %d\n255\n", w, h);
    const size_t n = static_cast<size_t>(w) * h * 3;
    std::fwrite(rgb.data(), 1, n, f);
    std::fclose(f);
    return true;
}

// 纯 zlib PNG 编码器（RGB8 / color type 2），无需额外图像库。
static void put_be32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back(static_cast<uint8_t>((x >> 24) & 0xff));
    v.push_back(static_cast<uint8_t>((x >> 16) & 0xff));
    v.push_back(static_cast<uint8_t>((x >> 8) & 0xff));
    v.push_back(static_cast<uint8_t>(x & 0xff));
}
static bool save_png(const std::string& path, const std::vector<uint8_t>& rgb, int w, int h) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    std::vector<uint8_t> raw;
    raw.reserve((size_t)h * (1 + (size_t)w * 3));
    for (int y = 0; y < h; ++y) {
        raw.push_back(0);
        const uint8_t* row = &rgb[(size_t)y * w * 3];
        raw.insert(raw.end(), row, row + (size_t)w * 3);
    }
    uLong cap = compressBound(raw.size());
    std::vector<uint8_t> comp(cap);
    uLong out = cap;
    if (compress(comp.data(), &out, raw.data(), raw.size()) != Z_OK) {
        std::fclose(f);
        return false;
    }
    comp.resize(out);
    const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    std::fwrite(sig, 1, 8, f);
    auto write_chunk = [&](const char* type, const uint8_t* body, uint32_t blen) {
        std::vector<uint8_t> hdr;
        put_be32(hdr, blen);
        for (int i = 0; i < 4; ++i) hdr.push_back((uint8_t)type[i]);
        std::fwrite(hdr.data(), 1, hdr.size(), f);
        if (blen) std::fwrite(body, 1, blen, f);
        uint32_t c = (uint32_t)crc32(0L, reinterpret_cast<const Bytef*>(type), 4);
        c = (uint32_t)crc32(c, body, blen);
        std::vector<uint8_t> crcb;
        put_be32(crcb, c);
        std::fwrite(crcb.data(), 1, 4, f);
    };
    std::vector<uint8_t> ihdr;
    put_be32(ihdr, (uint32_t)w);
    put_be32(ihdr, (uint32_t)h);
    ihdr.push_back(8);
    ihdr.push_back(2);
    ihdr.push_back(0);
    ihdr.push_back(0);
    ihdr.push_back(0);
    write_chunk("IHDR", ihdr.data(), ihdr.size());
    write_chunk("IDAT", comp.data(), comp.size());
    write_chunk("IEND", nullptr, 0);
    std::fclose(f);
    return true;
}

} // namespace

int main(int argc, char** argv) {
    std::string task, device = "/dev/video0", save, raw_path, mp4_path;
    uint32_t width = 1280, height = 720;
    uint32_t raw_width = 0, raw_height = 0;
    int frames = 5;
    float score = -1.f;
    bool use_rga = false, bench = false, use_rga_fd = false;
    std::string config_path;
    int channel_hint = -1, shm_channel = -1, fps_override = 0;
    // Always-on monitoring knobs (see the --daemon help text below).
    bool daemon = false, stats = false, drop_frames = true;
    int shm_write = -1;  // -1 = do not publish to /dev/shm/detection_boxes

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&](const char* def) -> std::string {
            return (i + 1 < argc) ? std::string(argv[++i]) : std::string(def);
        };
        if (a == "--task") task = next("");
        else if (a == "--device") device = next("/dev/video0");
        else if (a == "--raw") raw_path = next("");
        else if (a == "--mp4") mp4_path = next("");
        else if (a == "--raw-width") raw_width = static_cast<uint32_t>(std::atoi(next("1280").c_str()));
        else if (a == "--raw-height") raw_height = static_cast<uint32_t>(std::atoi(next("720").c_str()));
        else if (a == "--width") width = static_cast<uint32_t>(std::atoi(next("1280").c_str()));
        else if (a == "--height") height = static_cast<uint32_t>(std::atoi(next("720").c_str()));
        else if (a == "--frames") frames = std::atoi(next("5").c_str());
        else if (a == "--score") score = static_cast<float>(std::atof(next("0.25").c_str()));
        else if (a == "--save") save = next("");
        else if (a == "--rga") use_rga = true;
        else if (a == "--rga-fd") use_rga_fd = true;
        else if (a == "--bench") { use_rga = true; bench = true; }
        else if (a == "--config") config_path = next("");
        else if (a == "--channel") channel_hint = std::atoi(next("0").c_str());
        else if (a == "--shm") shm_channel = std::atoi(next("0").c_str());
        else if (a == "--fps") fps_override = std::atoi(next("25").c_str());
        // ---- always-on monitoring -------------------------------------------
        // --daemon: never stop. Runs the capture/infer threads until SIGINT or
        //   SIGTERM, publishes every frame into /dev/shm/detection_boxes. This
        //   is the production mode; the default --frames N stays for bring-up.
        // --shm-write N: publish into channel N of /dev/shm/detection_boxes
        //   (read by qt5_qml_c001's DetectionBoxShmBridge).
        // --stats: print a rolling one-line-per-second summary (fps, pre, infer,
        //   drop ratio) instead of one line per frame. A daemon logging 12 lines
        //   a second would bury the SD/NPU logs on a 3.9GB board.
        // --no-drop: restore the legacy lossless appsink queue (max-buffers=4).
        //   Only for benchmarking -- latency then grows without bound.
        else if (a == "--daemon") daemon = true;
        else if (a == "--shm-write") shm_write = std::atoi(next("0").c_str());
        else if (a == "--stats") stats = true;
        else if (a == "--no-drop") drop_frames = false;
        else {
            std::fprintf(stderr,
                         "usage: %s --task <yaml> [--device /dev/video0 | --raw <nv12 file> | --mp4 <file>]\n"
                         "          [--width W --height H] [--raw-width W --raw-height H]\n"
                         "          [--frames N] [--score S] [--save out.png|.ppm]\n"
                         "          [--rga] [--bench] [--rga-fd] [--config file] [--channel N]\n"
                         "          [--shm N] [--fps N]\n"
                         "  --config <json>  camera_config.json: device/width/height/framerate\n"
                         "  --channel N      pick this channel entry from --config\n"
                         "  --shm N          consume qt5_qml_c001 /picshare<N> (BGR->NV12)\n"
                         "  --mp4 <file>     read an MP4 and feed keyframes only; loops at EOS\n"
                         "  --fps N          override framerate for the gstreamer path\n"
                         "\n"
                         "  --raw consumes a raw NV12 dump, e.g. one captured with\n"
                         "  v4l2-ctl -d /dev/video0 --stream-mmap=3 --stream-count=1 \\\n"
                         "              --stream-to=/tmp/cap.nv12\n"
                         "  --rga   preprocess with Rockchip RGA instead of the CPU path\n"
                         "  --rga-fd same, but the NV12 source is an external dma-buf fd\n"
                         "          (VIDIOC_EXPBUF) instead of a host copy -- zero copy\n"
                         "  --bench run both and report per-path timing + pixel diff\n"
                         "\n"
                         "  always-on monitoring (production mode):\n"
                         "  --daemon        run until SIGINT/SIGTERM instead of --frames N\n"
                         "  --shm-write N   publish boxes into /dev/shm/detection_boxes\n"
                         "                  channel N (qt5_qml_c001 reads it, seqlock)\n"
                         "  --stats         one summary line per second, not per frame\n"
                         "  --no-drop       legacy lossless queue (max-buffers=4);\n"
                         "                  latency grows without bound, debugging only\n",
                         argv[0]);
            return 1;
        }
    }
    if (task.empty()) {
        std::fprintf(stderr, "--task is required\n");
        return 1;
    }
    if (frames < 1) frames = 1;
    if (daemon) {
        // A daemon is by definition unbounded; --frames would be meaningless.
        // Detection results reach the UI through the shm segment, not stdout.
        frames = 0;
    }

    evs::TaskConfig cfg;
    uint32_t err = 0;
    if (!evs::load_task_config(task, cfg, err)) {
        std::fprintf(stderr, "cannot parse task yaml %s (status 0x%x)\n", task.c_str(), err);
        return 1;
    }
    if (score < 0.f) score = cfg.confidence_threshold;

    auto backend = evs::create_backend(cfg.backend);
    if (!backend) {
        std::fprintf(stderr, "backend '%s' not compiled in\n", cfg.backend.c_str());
        return 1;
    }
    const uint32_t lrc = backend->load(cfg.Model_path, cfg.init_timeout_s);
    if (lrc != 0u) {
        std::fprintf(stderr, "backend load failed 0x%x\n", lrc);
        return 3;
    }
    backend->configure_input(cfg.input_width, cfg.input_height);

    auto dec = evs::create_decoder(cfg.output_format, cfg.person_class_id,
                                  cfg.decoder_classes, cfg.input_width, cfg.input_height);
    if (!dec) {
        std::fprintf(stderr, "unknown output_format '%s'\n", cfg.output_format.c_str());
        return 1;
    }
    evs::Preprocessor pre;
    evs::PreprocessConfig pc;
    pc.input_width = cfg.input_width;
    pc.input_height = cfg.input_height;
    pc.letterbox = cfg.letterbox;
    pc.target_format = (cfg.input_format == "bgr") ? evs::PixelFormat::BGR
                                                    : evs::PixelFormat::RGB;
    pre.configure(pc);

    // ---- capture source selection -------------------------------------------
    // Inference only sees a FrameSource::grab() of packed NV12. Which source
    // is selected here:
    //   1) --raw            : a pre-captured NV12 file (unchanged)
    //   2) --mp4            : MP4 keyframes (Mp4KeyframeCapture)
    //   3) --shm <channel>  : consume qt5_qml_c001's POSIX shm /picshare<channel>
    //   4) --config [...]   : live GStreamer pipeline (GstCapture)
    //   5) default          : legacy raw V4L2 mmap (kept for backward compat)
    enum class SrcKind { Raw, LegacyV4L2, Gst, Shm, Mp4 };
    SrcKind src_kind = SrcKind::LegacyV4L2;
    cap::GstCapture gst_cap;
    cap::ShmCapture shm_cap;
    cap::Mp4KeyframeCapture mp4_cap;
    cap::FrameSource* source = nullptr;
    Camera cam;
    std::vector<uint8_t> raw;
    uint32_t src_w = 0, src_h = 0, src_stride = 0;

    if (!raw_path.empty()) {
        src_kind = SrcKind::Raw;
        std::FILE* f = std::fopen(raw_path.c_str(), "rb");
        if (!f) {
            std::fprintf(stderr, "cannot open %s\n", raw_path.c_str());
            return 2;
        }
        std::fseek(f, 0, SEEK_END);
        const long n = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        raw.resize(n > 0 ? static_cast<size_t>(n) : 0);
        if (!raw.empty() && std::fread(raw.data(), 1, raw.size(), f) != raw.size()) {
            std::fprintf(stderr, "short read on %s\n", raw_path.c_str());
            std::fclose(f);
            return 2;
        }
        std::fclose(f);
        src_w = raw_width ? raw_width : 1280;
        src_h = raw_height ? raw_height : 720;
        src_stride = src_w;
        const size_t need = static_cast<size_t>(src_w) * src_h * 3 / 2;
        if (raw.size() < need) {
            std::fprintf(stderr,
                         "%s holds %zu bytes, %ux%u NV12 needs %zu\n",
                         raw_path.c_str(), raw.size(), src_w, src_h, need);
            return 2;
        }
        std::fprintf(stderr, "[raw] %s %ux%u NV12 (%zu bytes)\n", raw_path.c_str(),
                     src_w, src_h, raw.size());
    } else if (!mp4_path.empty()) {
        src_kind = SrcKind::Mp4;
        if (!mp4_cap.open(mp4_path, /*loop=*/true)) return 2;
        source = &mp4_cap;
        src_w = static_cast<uint32_t>(mp4_cap.width());
        src_h = static_cast<uint32_t>(mp4_cap.height());
        src_stride = src_w;
    } else if (shm_channel >= 0) {
        src_kind = SrcKind::Shm;
        int sw = static_cast<int>(width), sh = static_cast<int>(height);
        if (!config_path.empty()) {
            auto cfg = cap::load_capture_config(config_path);
            const cap::CamEntry* e = cap::select_entry(cfg, shm_channel);
            if (e) { sw = e->width; sh = e->height; }
        }
        if (sw <= 0 || sh <= 0) {
            std::fprintf(stderr,
                "[shm] resolution unknown: pass --width/--height or a config entry\n");
            return 2;
        }
        if (!shm_cap.open(shm_channel, sw, sh)) return 2;
        source = &shm_cap;
        src_w = static_cast<uint32_t>(sw);
        src_h = static_cast<uint32_t>(sh);
        src_stride = src_w;
        std::fprintf(stderr, "[shm] consuming /picshare%d %ux%u\n", shm_channel, sw, sh);
    } else {
        int gw = static_cast<int>(width), gh = static_cast<int>(height);
        int gfps = fps_override ? fps_override : 25;
        std::string gdev = device;
        if (!config_path.empty()) {
            auto cfg = cap::load_capture_config(config_path);
            const cap::CamEntry* e = cap::select_entry(cfg, channel_hint);
            if (e) {
                gdev = e->device; gw = e->width; gh = e->height;
                if (e->framerate) gfps = e->framerate;
            } else {
                std::fprintf(stderr,
                    "[config] no entry for channel %d; using CLI defaults\n",
                    channel_hint);
            }
        }
        // drop_frames=true is what makes this usable as a live monitor: the
        // appsink keeps depth 1 and overwrites, so a slow inference can never
        // build up a backlog. See GstCapture::open for the latency argument.
        if (!gst_cap.open(gdev, gw, gh, gfps, drop_frames)) {
            std::fprintf(stderr,
                "[gst] cannot start gstreamer capture on %s; "
                "falling back to legacy V4L2\n", gdev.c_str());
            if (!camera_open(cam, device, width, height)) return 2;
            src_kind = SrcKind::LegacyV4L2;
            src_w = cam.width;
            src_h = cam.height;
            src_stride = cam.bytesperline ? cam.bytesperline : cam.width;
        } else {
            src_kind = SrcKind::Gst;
            source = &gst_cap;
            src_w = static_cast<uint32_t>(gw);
            src_h = static_cast<uint32_t>(gh);
            src_stride = src_w;
            std::fprintf(stderr, "[gst] capturing %s %ux%u @%dfps (%s queue)\n",
                         gdev.c_str(), gw, gh, gfps,
                         drop_frames ? "drop=true latency" : "drop=false lossless");
        }
    }
    if (use_rga_fd && !camera_export_buffers(cam)) return 2;

    // ---- publish detections into /dev/shm/detection_boxes -------------------
    // The Qt HMI already maps this segment and polls it every millisecond; this
    // process is the missing producer. Publishing is opt-in so the bring-up
    // flow does not create the segment behind the user's back.
    evs_host::DetectionShmWriter det_writer;
    if (shm_write >= 0) {
        if (!det_writer.open(shm_write)) {
            std::fprintf(stderr,
                "[det_shm] cannot open /dev/shm/detection_boxes ch=%d; "
                "continuing without publishing\n", shm_write);
        }
    }

    std::vector<uint8_t> input, rgb;
    std::vector<float> out_tensor;
    std::vector<evs::Target> targets;
    double sum_total = 0.0, sum_pre = 0.0, sum_infer = 0.0;
    int counted = 0;

    // --stats rolling window (one report per second).
    auto win_mark_ = std::chrono::steady_clock::now();
    int win_frames_ = 0;
    double win_pre_ = 0.0, win_infer_ = 0.0;

    // Daemon shutdown: a monitor process must be stoppable without SIGKILL, and
    // it must leave the shm segment marked invalid so the HMI stops drawing
    // boxes that nobody is producing any more.
    static volatile sig_atomic_t g_stop = 0;
    struct sigaction sa {};
    sa.sa_handler = [](int) { g_stop = 1; };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    // ---- always-on mode: move capture onto its own thread ------------------
    // Live pipeline and MP4 both block inside grab(). The mailbox keeps that
    // wait off the infer thread. Raw and shm stay inline: a blocked grab there
    // is a bring-up path, and --daemon just loops it on the infer thread.
    LatestFrame mailbox;
    std::thread cap_thread;
    const bool threaded_capture = daemon && source &&
                                  (src_kind == SrcKind::Gst || src_kind == SrcKind::Mp4);
    if (threaded_capture) {
        // One NV12 frame at 1280x720 is 1382400 bytes; three slots keep the
        // producer running while the consumer preprocesses.
        mailbox.configure(static_cast<size_t>(src_w) * src_h * 3 / 2,
                          static_cast<int>(src_w), static_cast<int>(src_h));
        cap_thread = std::thread([&] {
            cap::CapFrame f;
            while (true) {
                if (!source->grab(f) || !f.data) {
                    // Source stalled. Do not spin at full speed on the error path.
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    continue;
                }
                mailbox.push(f.data, f.buf.size(), f.w, f.h);
            }
        });
    }

    // Stop flag for the capture thread; set on SIGINT/SIGTERM below.
    auto request_stop = [&] {
        mailbox.stop();
        if (cap_thread.joinable()) cap_thread.detach();
    };

    // Per-frame source, used when capture is NOT threaded. `held` outlives the
    // lambda so a FrameSource buffer stays valid through preprocess.
    std::vector<uint8_t> inline_buf;
    cap::CapFrame held;
    auto grab_inline = [&](const uint8_t*& src, uint32_t& w, uint32_t& h,
                           uint32_t& stride) -> bool {
        if (src_kind == SrcKind::Raw) {
            src = raw.data();
            return src != nullptr;
        }
        if (src_kind == SrcKind::LegacyV4L2) {
            if (raw_path.empty()) {
                if (!camera_grab(cam, nullptr, true)) return false;
            }
            src = cam.frame.data();
            return src != nullptr;
        }
        if (!source || !source->grab(held) || !held.data) return false;
        src = held.data;
        w = static_cast<uint32_t>(held.w);
        h = static_cast<uint32_t>(held.h);
        stride = w; // FrameSource returns tightly packed NV12
        return true;
    };

    for (int f = 0; daemon || f < frames; ++f) {
        if (g_stop) {
            std::fprintf(stderr, "[daemon] signal received, shutting down\n");
            break;
        }
        const bool fd_mode = use_rga_fd;
        const bool keep_cpu = !fd_mode || !save.empty();
        int idx = -1;
        const uint8_t* src = nullptr;
        uint32_t cur_w = src_w, cur_h = src_h, cur_stride = src_stride;

        if (threaded_capture) {
            // Take the newest frame the capture thread has published. A 500ms
            // gap means the camera stalled; keep looping so a transient stall
            // does not kill a long-running daemon.
            if (!mailbox.wait_and_copy(inline_buf, cur_w, cur_h, nullptr)) {
                if (daemon) {
                    std::fprintf(stderr, "[daemon] no frame for 500ms, waiting\n");
                    continue;
                }
                return 2;
            }
            src = inline_buf.data();
        } else if (!grab_inline(src, cur_w, cur_h, cur_stride)) {
            // A long-running daemon must survive a transient capture error;
            // a bounded bring-up run should report it and stop.
            if (daemon) {
                std::fprintf(stderr, "[daemon] capture error, retrying\n");
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                --f;
                continue;
            }
            return 2;
        }
        if (!src) return 2;

        evs::Frame frame{};
        frame.camera_id = 0;
        frame.width = static_cast<int>(cur_w);
        frame.height = static_cast<int>(cur_h);
        frame.stride = static_cast<int>(cur_stride);
        frame.format = evs::PixelFormat::NV12;
        frame.data = const_cast<uint8_t*>(src);

        const auto t0 = std::chrono::steady_clock::now();
        evs::PreprocessMap map{};
        uint32_t pr = 0;
        const char* path_name = "cpu";
        double cpu_ms = -1.0;
        if (use_rga && !use_rga_fd) {
            pr = evs_host::rga_preprocess_nv12(src, static_cast<int>(cur_w),
                                               static_cast<int>(cur_h),
                                               cfg.input_width, cfg.input_height,
                                               cfg.letterbox, input, map);
            if (pr != 0u) {
                std::fprintf(stderr, "[rga] falling back to CPU: %s\n",
                             evs_host::rga_last_error());
                pr = 0;
            } else {
                path_name = "rga";
            }
        } else if (use_rga_fd) {
            if (idx < 0) {
                std::fprintf(stderr, "[rga-fd] needs a live camera, not --raw\n");
                return 3;
            }
            pr = evs_host::rga_preprocess_nv12_fd(cam.bufs[idx].fd,
                                                 static_cast<int>(cur_w),
                                                 static_cast<int>(cur_h),
                                                 cfg.input_width, cfg.input_height,
                                                 cfg.letterbox, input, map);
            if (pr != 0u) {
                std::fprintf(stderr, "[rga-fd] failed: %s\n",
                             evs_host::rga_last_error());
                if (keep_cpu && !cam.frame.empty()) {
                    frame.data = cam.frame.data();
                    path_name = "cpu";
                    pr = 0;
                } else {
                    camera_queue(cam, idx);
                    std::fprintf(stderr,
                                 "[rga-fd] no CPU fallback (pass --save or fix RGA)\n");
                    return 3;
                }
            } else {
                path_name = "rga-fd";
            }
            camera_queue(cam, idx); // fd consumed, return buffer to the driver
        }
        if (pr == 0u && path_name == std::string("cpu")) {
            pr = pre.run(frame, input, map);
            const auto tc = std::chrono::steady_clock::now();
            cpu_ms = std::chrono::duration<double, std::milli>(tc - t0).count();
        }
        const auto t1 = std::chrono::steady_clock::now();
        if (pr != 0u) {
            std::fprintf(stderr, "preprocess failed 0x%x\n", pr);
            return 3;
        }
        // --bench: run the CPU path on the same frame and report the delta, so
        // the RGA port can be verified against the reference it replaces.
        if (bench) {
            std::vector<uint8_t> ref;
            evs::PreprocessMap ref_map{};
            const auto tb0 = std::chrono::steady_clock::now();
            const uint32_t rr = pre.run(frame, ref, ref_map);
            const auto tb1 = std::chrono::steady_clock::now();
            const double ref_ms =
                std::chrono::duration<double, std::milli>(tb1 - tb0).count();
            if (rr == 0u && ref.size() == input.size()) {
                size_t maxdiff = 0, sumdiff = 0;
                for (size_t i = 0; i < ref.size(); ++i) {
                    const size_t d = ref[i] > input[i] ? ref[i] - input[i]
                                                       : input[i] - ref[i];
                    if (d > maxdiff) maxdiff = d;
                    sumdiff += d;
                }
                // EVS_DUMP_BENCH=/tmp/p -> /tmp/p_rga.ppm + /tmp/p_cpu.ppm,
                // so the two preprocessing paths can be diffed pixel by pixel.
                if (const char* dp = std::getenv("EVS_DUMP_BENCH")) {
                    auto dump_ppm = [&](const std::vector<uint8_t>& buf,
                                        const char* tag) {
                        char p[256];
                        std::snprintf(p, sizeof(p), "%s_%s.ppm", dp, tag);
                        FILE* f = std::fopen(p, "wb");
                        if (!f) return;
                        std::fprintf(f, "P6\n%d %d\n255\n", cfg.input_width,
                                     cfg.input_height);
                        std::fwrite(buf.data(), 1, buf.size(), f);
                        std::fclose(f);
                        std::fprintf(stderr, "[dump] %s\n", p);
                    };
                    dump_ppm(input, "rga");
                    dump_ppm(ref, "cpu");
                }
                std::printf("  [bench] rga=%.2fms cpu=%.2fms  speedup=%.1fx  "
                            "maxdiff=%zu meandiff=%.3f\n",
                            std::chrono::duration<double, std::milli>(t1 - t0).count(),
                            std::chrono::duration<double, std::milli>(tb1 - tb0).count(),
                            ref_ms / std::max(
                                1e-6, std::chrono::duration<double, std::milli>(
                                          t1 - t0).count()),
                            maxdiff,
                            static_cast<double>(sumdiff) / ref.size());
            } else {
                std::printf("  [bench] CPU reference failed (0x%x)\n", rr);
            }
        }
        const uint32_t ir = backend->infer(input, out_tensor);
        const auto t2 = std::chrono::steady_clock::now();
        if (ir != 0u) {
            std::fprintf(stderr, "infer failed 0x%x\n", ir);
            return 3;
        }
        dec->decode(out_tensor, score, targets);
        for (auto& t : targets) evs::map_target_to_original(map, t, frame.width, frame.height);

        // ---- publish to the HMI -------------------------------------------
        // dist_cm stays negative: ground mapping (Phase 4) is not implemented,
        // so Target::distance_m is 0 and there is no honest distance to report.
        // The QML filters non-positive mappedY, which is the correct behaviour
        // -- better "distance unknown" than a fabricated number.
        if (det_writer.opened()) {
            det_writer.publish(targets, frame.width, frame.height, -1.f);
        }

        const double ms_pre =
            std::chrono::duration<double, std::milli>(t1 - t0).count();
        const double ms_infer =
            std::chrono::duration<double, std::milli>(t2 - t1).count();
        const double ms_all =
            std::chrono::duration<double, std::milli>(t2 - t0).count();
        sum_pre += ms_pre; sum_infer += ms_infer; sum_total += ms_all; ++counted;

        // --stats: one line per second instead of one per frame. A daemon at
        // 12fps would otherwise emit ~1000 lines/min and bury every other
        // message in the board log.
        if (stats) {
            ++win_frames_;
            win_pre_ += ms_pre;
            win_infer_ += ms_infer;
            const auto now = std::chrono::steady_clock::now();
            if (now - win_mark_ >= std::chrono::seconds(1)) {
                const double sec =
                    std::chrono::duration<double>(now - win_mark_).count();
                const double fps = win_frames_ / std::max(1e-6, sec);
                const double pre = win_frames_ ? win_pre_ / win_frames_ : 0.0;
                const double inf = win_frames_ ? win_infer_ / win_frames_ : 0.0;
                // Drop accounting from the mailbox: frames the sensor produced
                // that inference never got to. In threaded mode this is exact
                // (measured where frames are discarded); inline capture cannot
                // drop anything, so it reports 0/0.
                unsigned long long pub = 0, cons = 0, drop = 0;
                if (threaded_capture) {
                    pub = mailbox.published();
                    cons = mailbox.consumed();
                    drop = mailbox.dropped();
                } else {
                    pub = cons = static_cast<unsigned long long>(win_frames_);
                }
                const unsigned long long drop_pct =
                    pub ? drop * 100 / pub : 0;
                std::printf(
                    "[stats] fps=%.1f targets=%zu pre=%.2fms infer=%.2fms "
                    "drop=%llu/%llu (%llu%%) shm_seq=%u\n",
                    fps, targets.size(), pre, inf, drop, pub, drop_pct,
                    det_writer.seq());
                std::fflush(stdout);
                win_frames_ = 0;
                win_pre_ = 0.0;
                win_infer_ = 0.0;
                win_mark_ = now;
            }
        } else {
        std::printf("frame %d: %dx%d -> model %dx%d, targets=%zu, pre[%s]=%.2fms "
                    "infer=%.2fms total=%.2fms\n",
                    f, frame.width, frame.height, cfg.input_width,
                    cfg.input_height, targets.size(), path_name, ms_pre, ms_infer,
                    ms_all);
        for (const auto& t : targets) {
            std::printf("  class=%d conf=%.3f  box(px)=[%.0f %.0f %.0f %.0f] "
                        "norm=[%.4f %.4f %.4f %.4f]\n",
                        t.class_id, t.confidence, t.x * frame.width, t.y * frame.height,
                        t.w * frame.width, t.h * frame.height, t.x, t.y, t.w, t.h);
        }
#if defined(EVS_BACKEND_RKNN)
        // RKNN backend 通过 PIMPL 暴露，没有 per-run 计时接口；
        // 直接调 last_run_us() 在启用 RKNN 时编译不过，这里改为打印提示。
        std::printf("  npu backend active (per-run timing unavailable)\n");
#endif
        }

        if (!save.empty()) {
            nv12_to_rgb(src, static_cast<int>(cur_w), static_cast<int>(cur_h), rgb);
            for (const auto& t : targets) {
                const uint8_t c = (t.class_id == 0) ? 255 : 0;
                const uint8_t g = (t.class_id == 0) ? 40 : 200;
                const uint8_t b = (t.class_id == 0) ? 40 : 0;
                draw_box(rgb, static_cast<int>(cur_w), static_cast<int>(cur_h),
                         t.x, t.y, t.w, t.h, c, g, b);
            }
            char path[512];
            std::snprintf(path, sizeof(path), "%s", save.c_str());
            std::string out = path;
            if (frames > 1) {
                const size_t dot = out.rfind('.');
                char idx[16];
                std::snprintf(idx, sizeof(idx), "_%d", f);
                out.insert(dot == std::string::npos ? out.size() : dot, idx);
            }
            std::printf("  saved %s\n", out.c_str());
            const bool is_png = out.size() >= 4 &&
                                out.compare(out.size() - 4, 4, ".png") == 0;
            const bool ok = is_png
                                ? save_png(out, rgb, static_cast<int>(cur_w), static_cast<int>(cur_h))
                                : save_ppm(out, rgb, static_cast<int>(cur_w), static_cast<int>(cur_h));
            if (!ok) std::fprintf(stderr, "  cannot write %s\n", out.c_str());
        }
    }

    // Stop the capture thread before tearing anything else down, otherwise it
    // would keep pulling from a destroyed GstCapture.
    request_stop();

    if (counted > 0) {
        std::printf("avg over %d frames: pre=%.2fms infer=%.2fms total=%.2fms "
                    "(%.1f fps)\n",
                    counted, sum_pre / counted, sum_infer / counted,
                    sum_total / counted, 1000.0 / (sum_total / counted));
        if (daemon && threaded_capture) {
            // How much the sensor outran us over the whole run. A high value is
            // expected and healthy on RK3576 (25fps sensor vs ~12.7fps NPU) --
            // it is the number to watch when tuning, not a fault.
            const unsigned long long pub = mailbox.published();
            const unsigned long long skipped = mailbox.dropped();
            std::printf("avg drop: %llu of %llu camera frames skipped (%.1f%%)\n",
                        skipped, pub, pub ? skipped * 100.0 / pub : 0.0);
        }
    }
    // Leaves has=0 behind, so the HMI stops drawing boxes the moment the
    // producer is gone instead of freezing the last detection on screen.
    det_writer.close();
    backend->release();
    return 0;
}
// capture.cpp -- image capture layer for v4l2_detect.
//
// Every FrameSource returns tightly-packed NV12 (stride == width):
//   1) GstCapture          : v4l2src -> NV12 -> appsink
//   2) Mp4KeyframeCapture  : MP4 file, keyframes only
//   3) ShmCapture          : /picshare<channel> (BGR -> NV12)
//
// The module depends ONLY on GStreamer + POSIX shm + standard C++; it does not
// touch any evs internal type.

#include "capture.h"

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <map>
#include <algorithm>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <cerrno>

// ---- GStreamer (only needed for the gst capture path; the whole TU is only
//      compiled into the Linux-only v4l2_detect target, so this is safe) ----
#include <gst/gst.h>
#include <gst/app/gstappsink.h>

namespace cap {

// ---------------------------------------------------------------------------
// Minimal, dependency-free JSON parser (enough for our fixed-schema
// camera_config.json: an array of objects with simple scalar fields).
// ---------------------------------------------------------------------------
namespace json {
struct Value {
    enum Type { Null, Bool, Num, Str, Arr, Obj } t = Null;
    bool b = false;
    double num = 0;
    std::string s;
    std::vector<Value> arr;
    std::map<std::string, Value> obj;
    const Value* find(const std::string& k) const {
        auto it = obj.find(k);
        return it == obj.end() ? nullptr : &it->second;
    }
};

struct Parser {
    const char* p = nullptr;
    const char* end = nullptr;
    bool fail = false;

    void skip_ws() {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
            ++p;
    }
    bool parse_value(Value& v) {
        skip_ws();
        if (p >= end) { fail = true; return false; }
        char c = *p;
        if (c == '{') return parse_object(v);
        if (c == '[') return parse_array(v);
        if (c == '"') { v.t = Value::Str; return parse_string(v.s); }
        if (c == 't' || c == 'f') return parse_bool(v);
        if (c == 'n') { v.t = Value::Null; p += 4; return true; }
        if (c == '-' || (c >= '0' && c <= '9')) return parse_number(v);
        fail = true;
        return false;
    }
    bool parse_string(std::string& out) {
        ++p;  // opening quote
        out.clear();
        while (p < end && *p != '"') {
            if (*p == '\\') {
                ++p;
                if (p >= end) { fail = true; return false; }
                char e = *p++;
                switch (e) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'n': out.push_back('\n'); break;
                    case 't': out.push_back('\t'); break;
                    case 'r': out.push_back('\r'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'u': {
                        // minimal: decode a 4-hex-digit BMP char
                        if (p + 4 > end) { fail = true; return false; }
                        unsigned code = 0;
                        for (int i = 0; i < 4; ++i) {
                            char h = *p++;
                            code <<= 4;
                            if (h >= '0' && h <= '9') code |= (h - '0');
                            else if (h >= 'a' && h <= 'f') code |= (h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') code |= (h - 'A' + 10);
                            else { fail = true; return false; }
                        }
                        if (code < 0x80) out.push_back((char)code);
                        else if (code < 0x800) {
                            out.push_back((char)(0xC0 | (code >> 6)));
                            out.push_back((char)(0x80 | (code & 0x3F)));
                        } else {
                            out.push_back((char)(0xE0 | (code >> 12)));
                            out.push_back((char)(0x80 | ((code >> 6) & 0x3F)));
                            out.push_back((char)(0x80 | (code & 0x3F)));
                        }
                        break;
                    }
                    default: out.push_back(e); break;
                }
            } else {
                out.push_back(*p++);
            }
        }
        if (p >= end) { fail = true; return false; }
        ++p;  // closing quote
        return true;
    }
    bool parse_bool(Value& v) {
        if (end - p >= 4 && std::strncmp(p, "true", 4) == 0) {
            v.t = Value::Bool; v.b = true; p += 4; return true;
        }
        if (end - p >= 5 && std::strncmp(p, "false", 5) == 0) {
            v.t = Value::Bool; v.b = false; p += 5; return true;
        }
        fail = true;
        return false;
    }
    bool parse_number(Value& v) {
        const char* start = p;
        if (*p == '-') ++p;
        while (p < end && ((*p >= '0' && *p <= '9') || *p == '.' || *p == 'e' ||
                           *p == 'E' || *p == '+' || *p == '-'))
            ++p;
        v.t = Value::Num;
        v.num = std::strtod(start, nullptr);
        return true;
    }
    bool parse_array(Value& v) {
        v.t = Value::Arr;
        ++p;  // '['
        skip_ws();
        if (p < end && *p == ']') { ++p; return true; }
        while (true) {
            Value el;
            if (!parse_value(el)) return false;
            v.arr.push_back(std::move(el));
            skip_ws();
            if (p >= end) { fail = true; return false; }
            if (*p == ',') { ++p; continue; }
            if (*p == ']') { ++p; break; }
            fail = true;
            return false;
        }
        return true;
    }
    bool parse_object(Value& v) {
        v.t = Value::Obj;
        ++p;  // '{'
        skip_ws();
        if (p < end && *p == '}') { ++p; return true; }
        while (true) {
            skip_ws();
            if (p >= end || *p != '"') { fail = true; return false; }
            std::string key;
            if (!parse_string(key)) return false;
            skip_ws();
            if (p >= end || *p != ':') { fail = true; return false; }
            ++p;  // ':'
            Value val;
            if (!parse_value(val)) return false;
            v.obj[key] = std::move(val);
            skip_ws();
            if (p >= end) { fail = true; return false; }
            if (*p == ',') { ++p; continue; }
            if (*p == '}') { ++p; break; }
            fail = true;
            return false;
        }
        return true;
    }
};

static bool parse_json(const std::string& text, Value& out) {
    Parser ps;
    ps.p = text.data();
    ps.end = text.data() + text.size();
    if (!ps.parse_value(out)) return false;
    ps.skip_ws();
    return !ps.fail && ps.p == ps.end;
}
} // namespace json

std::vector<CamEntry> load_capture_config(const std::string& path) {
    std::vector<CamEntry> out;
    std::ifstream in(path);
    if (!in) {
        std::fprintf(stderr, "[config] cannot open %s: %s\n", path.c_str(),
                     std::strerror(errno));
        return out;
    }
    std::string content((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
    json::Value root;
    if (!json::parse_json(content, root)) {
        std::fprintf(stderr, "[config] %s is not valid JSON\n", path.c_str());
        return out;
    }
    if (root.t != json::Value::Arr) {
        std::fprintf(stderr, "[config] %s top-level must be a JSON array\n",
                     path.c_str());
        return out;
    }
    for (const auto& e : root.arr) {
        if (e.t != json::Value::Obj) continue;
        CamEntry ce;
        if (auto* p = e.find("device")) ce.device = p->s;
        if (auto* p = e.find("channel")) ce.channel = static_cast<int>(p->num);
        if (auto* p = e.find("enable")) ce.enable = p->b;
        if (auto* p = e.find("width")) ce.width = static_cast<int>(p->num);
        if (auto* p = e.find("height")) ce.height = static_cast<int>(p->num);
        if (auto* p = e.find("framerate")) ce.framerate = static_cast<int>(p->num);
        out.push_back(ce);
    }
    return out;
}

const CamEntry* select_entry(const std::vector<CamEntry>& cfg, int channel_hint) {
    if (channel_hint >= 0) {
        for (const auto& e : cfg)
            if (e.channel == channel_hint) return &e;
    }
    for (const auto& e : cfg)
        if (e.enable) return &e;
    return cfg.empty() ? nullptr : &cfg[0];
}

// ---------------------------------------------------------------------------
// BGR (CV_8UC3, row-major, BGR byte order) -> tightly-packed NV12 (BT.601
// limited range). Pure CPU, no RGA dependency, so the capture module stays
// self-contained. The inverse (NV12 -> RGB) lives in the tools' main.cpp and
// matches these coefficients.
// ---------------------------------------------------------------------------
static inline uint8_t clamp255(int x) {
    return static_cast<uint8_t>(x < 0 ? 0 : (x > 255 ? 255 : x));
}

static void bgr_to_nv12(const uint8_t* bgr, int w, int h,
                        std::vector<uint8_t>& nv12) {
    nv12.resize(static_cast<size_t>(w) * h * 3 / 2);
    const size_t ysize = static_cast<size_t>(w) * h;
    uint8_t* y = nv12.data();
    uint8_t* uv = nv12.data() + ysize;

    for (int row = 0; row < h; ++row) {
        for (int col = 0; col < w; ++col) {
            const uint8_t* p = bgr + (static_cast<size_t>(row) * w + col) * 3;
            const int B = p[0], G = p[1], R = p[2];
            const int yy = ((66 * R + 129 * G + 25 * B + 128) >> 8) + 16;
            y[static_cast<size_t>(row) * w + col] = clamp255(yy);
        }
    }
    for (int row = 0; row < h; row += 2) {
        for (int col = 0; col < w; col += 2) {
            int B = 0, G = 0, R = 0;
            for (int dr = 0; dr < 2; ++dr)
                for (int dc = 0; dc < 2; ++dc) {
                    const uint8_t* p =
                        bgr + (static_cast<size_t>(row + dr) * w + (col + dc)) * 3;
                    B += p[0]; G += p[1]; R += p[2];
                }
            B >>= 2; G >>= 2; R >>= 2;
            const int u = ((-38 * R - 74 * G + 112 * B + 128) >> 8) + 128;
            const int v = ((112 * R - 94 * G - 18 * B + 128) >> 8) + 128;
            const size_t o = (static_cast<size_t>(row / 2) * w + col);
            uv[o] = clamp255(u);
            uv[o + 1] = clamp255(v);
        }
    }
}

// ---------------------------------------------------------------------------
// GstCapture
// ---------------------------------------------------------------------------
bool GstCapture::open(const std::string& device, int w, int h, int fps,
                      bool drop_frames, const char* native_format) {
    static bool inited = false;
    if (!inited) {
        gst_init_check(nullptr, nullptr, nullptr);
        inited = true;
    }
    // The appsink policy is THE latency knob under sustained load.
    //
    //   drop=true max-buffers=1
    //       Queue depth 1, and drop=true makes it a "newest wins" queue: when
    //       the consumer (inference, ~75ms on RK3576) is slower than the sensor
    //       (25fps = 40ms), the pending sample is simply replaced. The consumer
    //       therefore always wakes up on the most recent frame and end-to-end
    //       latency stays bounded by <1 frame + inference, instead of growing
    //       without limit.
    //
    //   drop=false max-buffers=4
    //       Lossless bring-up mode. Fine while benchmarking, WRONG for a live
    //       monitor: the 4-slot queue saturates in ~160ms and from then on
    //       every frame is processed in arrival order, so the displayed result
    //       lags reality by an ever-increasing amount.
    //
    // NOTE: no `leaky-type` here. That property belongs to GstQueue, NOT to
    // GstAppSink -- setting it makes gst_parse_launch fail with
    //   'erroneous pipeline: no property "leaky-type" in element "appsink"'
    // and the source then silently falls back to the legacy V4L2 path. For an
    // appsink, `drop` + `max-buffers` already express the whole policy.
    // Same shape as the pipeline that returns a frame on this board:
    //   v4l2src device=/dev/video0 ! videoconvert ! jpegenc ! filesink
    // That command never asks the camera for NV12. A capsfilter of NV12 is
    // negotiated back onto v4l2src, which then fails to allocate buffers.
    // I420 is not a capture format on this node, so the first videoconvert
    // must run, and the second produces the packed NV12 the detector reads.
    // io-mode=mmap avoids VIDIOC_EXPBUF, which this node rejects.
    (void)fps;
    (void)w;
    (void)h;
    // A named camera format is taken as-is. NV12 is not one of those: this
    // node fails to allocate when the caps negotiate NV12 back onto v4l2src.
    gchar* desc = nullptr;
    if (native_format && native_format[0]) {
        desc = g_strdup_printf(
            "v4l2src device=%s io-mode=mmap ! video/x-raw,format=%s ! "
            "appsink name=cap drop=%s sync=false max-buffers=%d",
            device.c_str(), native_format,
            drop_frames ? "true" : "false", drop_frames ? 1 : 4);
    } else {
        desc = drop_frames
            ? g_strdup_printf(
                  "v4l2src device=%s io-mode=mmap ! videoconvert ! "
                  "video/x-raw,format=I420 ! videoconvert ! video/x-raw,format=NV12 ! "
                  "appsink name=cap drop=true sync=false max-buffers=1",
                  device.c_str())
            : g_strdup_printf(
                  "v4l2src device=%s io-mode=mmap ! videoconvert ! "
                  "video/x-raw,format=I420 ! videoconvert ! video/x-raw,format=NV12 ! "
                  "appsink name=cap drop=false sync=false max-buffers=4",
                  device.c_str());
    }
    GError* err = nullptr;
    std::fprintf(stderr, "[gst] pipeline: %s\n", desc);
    pipeline_ = gst_parse_launch(desc, &err);
    g_free(desc);
    if (!pipeline_) {
        std::fprintf(stderr, "[gst] parse_launch failed: %s\n",
                     err ? err->message : "(null)");
        if (err) g_error_free(err);
        return false;
    }
    if (err) {
        // A parse error still returns a (partial) pipeline, so the failure must
        // be reported here rather than only when pipeline_ is NULL -- otherwise
        // the caller sees a confusing "cannot find appsink" further down.
        std::fprintf(stderr, "[gst] parse warning: %s\n", err->message);
        g_error_free(err);
    }
    GstElement* sink = gst_bin_get_by_name(GST_BIN(pipeline_), "cap");
    if (!sink) {
        std::fprintf(stderr, "[gst] cannot find appsink 'cap'\n");
        close();
        return false;
    }
    appsink_ = sink;
    w_ = w;
    h_ = h;
    const GstStateChangeReturn r =
        gst_element_set_state((GstElement*)pipeline_, GST_STATE_PLAYING);
    if (r == GST_STATE_CHANGE_FAILURE) {
        std::fprintf(stderr, "[gst] failed to set pipeline PLAYING\n");
        close();
        return false;
    }
    return true;
}

namespace {

void dump_gst_bus(void* pipeline) {
    if (!pipeline) return;
    GstBus* bus = gst_element_get_bus((GstElement*)pipeline);
    if (!bus) return;
    while (GstMessage* msg = gst_bus_pop(bus)) {
        if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
            GError* e = nullptr;
            gchar* dbg = nullptr;
            gst_message_parse_error(msg, &e, &dbg);
            std::fprintf(stderr, "[gst] error: %s\n", e ? e->message : "(null)");
            if (dbg && dbg[0]) std::fprintf(stderr, "[gst] debug: %s\n", dbg);
            if (e) g_error_free(e);
            g_free(dbg);
        } else if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_WARNING) {
            GError* e = nullptr;
            gchar* dbg = nullptr;
            gst_message_parse_warning(msg, &e, &dbg);
            std::fprintf(stderr, "[gst] warning: %s\n", e ? e->message : "(null)");
            if (e) g_error_free(e);
            g_free(dbg);
        }
        gst_message_unref(msg);
    }
    gst_object_unref(bus);
}

} // namespace

bool GstCapture::grab(CapFrame& out) {
    // First frame on this sensor can take several seconds; the known-good
    // one-shot capture uses `timeout 8`.
    return grab_for(out, 8000);
}

bool GstCapture::grab_for(CapFrame& out, int timeout_ms) {
    if (!appsink_) return false;
    if (timeout_ms < 0) timeout_ms = 0;
    GstSample* sample = gst_app_sink_try_pull_sample(
        GST_APP_SINK(appsink_), static_cast<GstClockTime>(timeout_ms) * GST_MSECOND);
    if (!sample) {
        // Short polls are the capture thread overlapping inference. A miss
        // there is a retry, not a fault. The 8s grab still reports the bus.
        if (timeout_ms >= 1000) {
            GstState cur = GST_STATE_NULL, pending = GST_STATE_NULL;
            gst_element_get_state((GstElement*)pipeline_, &cur, &pending, 0);
            std::fprintf(stderr,
                         "[gst] no frame (state=%s pending=%s)\n",
                         gst_element_state_get_name(cur),
                         gst_element_state_get_name(pending));
            dump_gst_bus(pipeline_);
        }
        return false;
    }
    GstBuffer* buf = gst_sample_get_buffer(sample);
    out.buf.clear();
    if (buf) {
        // Multiplanar nodes put Y and chroma in separate GstMemory blocks.
        // Concatenating them is the packed layout RGA and the CPU path read.
        const guint nmem = gst_buffer_n_memory(buf);
        for (guint i = 0; i < nmem; ++i) {
            GstMemory* mem = gst_buffer_peek_memory(buf, i);
            GstMapInfo mi;
            if (!mem || !gst_memory_map(mem, &mi, GST_MAP_READ)) continue;
            out.buf.insert(out.buf.end(), mi.data, mi.data + mi.size);
            gst_memory_unmap(mem, &mi);
        }
    }
    out.pixel_format.clear();
    if (GstCaps* caps = gst_sample_get_caps(sample)) {
        if (!gst_caps_is_empty(caps)) {
            const GstStructure* st = gst_caps_get_structure(caps, 0);
            int cw = 0, ch = 0;
            if (gst_structure_get_int(st, "width", &cw) &&
                gst_structure_get_int(st, "height", &ch) &&
                cw > 0 && ch > 0) {
                w_ = cw;
                h_ = ch;
            }
            if (const gchar* f = gst_structure_get_string(st, "format"))
                out.pixel_format = f;
        }
    }
    gst_sample_unref(sample);
    out.w = w_;
    out.h = h_;
    out.fmt = (out.pixel_format == "BGR") ? CapFmt::BGR : CapFmt::NV12;
    out.data = out.buf.empty() ? nullptr : out.buf.data();
    return !out.buf.empty();
}

void GstCapture::close() {
    if (pipeline_) {
        gst_element_set_state((GstElement*)pipeline_, GST_STATE_NULL);
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
        appsink_ = nullptr;
    }
}

// ---------------------------------------------------------------------------
// ShmCapture -- consumes /picshare<channel> written by qt5_qml_c001.
// Layout: [ BGR CV_8UC3 pixels ][ PicMetaV2 { frame_seq uint64, rows int32,
//          cols int32, type int32, pad uint32, data_bytes uint64 } ] (32B tail)
// ---------------------------------------------------------------------------
bool ShmCapture::open(int channel, int w, int h) {
    const std::string name = "/picshare" + std::to_string(channel);
    fd_ = shm_open(name.c_str(), O_RDONLY, 0644);
    if (fd_ < 0) {
        std::fprintf(stderr, "[shm] shm_open %s failed: %s\n", name.c_str(),
                     std::strerror(errno));
        return false;
    }
    struct stat st {};
    if (fstat(fd_, &st) != 0) {
        std::fprintf(stderr, "[shm] fstat %s failed: %s\n", name.c_str(),
                     std::strerror(errno));
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    sz_ = static_cast<size_t>(st.st_size);
    mem_ = mmap(nullptr, sz_, PROT_READ, MAP_SHARED, fd_, 0);
    if (mem_ == MAP_FAILED) {
        std::fprintf(stderr, "[shm] mmap %s failed: %s\n", name.c_str(),
                     std::strerror(errno));
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    w_ = w;
    h_ = h;
    std::fprintf(stderr, "[shm] attached %s (%zu bytes)\n", name.c_str(), sz_);
    return true;
}

bool ShmCapture::grab(CapFrame& out) {
    if (fd_ < 0 || !mem_ || sz_ < 32) return false;
    const uint8_t* base = static_cast<const uint8_t*>(mem_);
    const uint8_t* tail = base + sz_ - 32;

    uint64_t seq, data_bytes;
    int32_t rows, cols, type;
    uint32_t pad;
    std::memcpy(&seq, tail + 0, sizeof(seq));
    std::memcpy(&rows, tail + 8, sizeof(rows));
    std::memcpy(&cols, tail + 12, sizeof(cols));
    std::memcpy(&type, tail + 16, sizeof(type));
    std::memcpy(&pad, tail + 20, sizeof(pad));
    std::memcpy(&data_bytes, tail + 24, sizeof(data_bytes));
    (void)pad;

    if (type != 16) {
        std::fprintf(stderr, "[shm] unexpected pixel type=%d (want CV_8UC3=16)\n",
                     type);
        return false;
    }
    if (static_cast<uint64_t>(data_bytes) + 32 != sz_) {
        std::fprintf(stderr, "[shm] size mismatch: data_bytes=%llu sz=%zu\n",
                     static_cast<unsigned long long>(data_bytes), sz_);
        return false;
    }
    if (cols <= 0 || rows <= 0) {
        std::fprintf(stderr, "[shm] invalid dims rows=%d cols=%d\n", rows, cols);
        return false;
    }

    // The meta is authoritative; prefer it over the open()-time w/h.
    bgr_to_nv12(base, cols, rows, out.buf);
    out.w = cols;
    out.h = rows;
    out.fmt = CapFmt::NV12;
    out.data = out.buf.data();
    last_seq_ = seq;
    return true;
}

void ShmCapture::close() {
    if (mem_ && mem_ != MAP_FAILED) {
        munmap(mem_, sz_);
        mem_ = nullptr;
    }
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    sz_ = 0;
}

// ---------------------------------------------------------------------------
// Mp4KeyframeCapture -- keyframes from an MP4, delivered as packed NV12.
//
// qtdemux knows the sync-sample table. h264parse / h265parse re-state it as
// GST_BUFFER_FLAG_DELTA_UNIT on every non-key access unit. A pad probe drops
// those buffers before decodebin, so the decoder only ever sees keyframes.
// Codecs without a parser fall back to the same probe on the demux pad.
// ---------------------------------------------------------------------------
struct Mp4Pipe {
    GstElement* pipeline = nullptr;
    GstElement* decode = nullptr;
    GstElement* convert = nullptr;
    GstElement* capsfilter = nullptr;
    GstElement* sink = nullptr;
    bool video_linked = false;
    bool failed = false;
};

static GstPadProbeReturn drop_non_key(GstPad*, GstPadProbeInfo* info, gpointer) {
    if (!(GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_BUFFER))
        return GST_PAD_PROBE_OK;
    GstBuffer* buf = GST_PAD_PROBE_INFO_BUFFER(info);
    if (buf && GST_BUFFER_FLAG_IS_SET(buf, GST_BUFFER_FLAG_DELTA_UNIT))
        return GST_PAD_PROBE_DROP;
    return GST_PAD_PROBE_OK;
}

static void on_mp4_decode_pad(GstElement*, GstPad* pad, gpointer user) {
    auto* b = static_cast<Mp4Pipe*>(user);
    GstCaps* caps = gst_pad_get_current_caps(pad);
    if (!caps) caps = gst_pad_query_caps(pad, nullptr);
    bool raw = false;
    if (caps && !gst_caps_is_empty(caps)) {
        const char* n = gst_structure_get_name(gst_caps_get_structure(caps, 0));
        raw = n && g_str_has_prefix(n, "video/x-raw");
    }
    if (caps) gst_caps_unref(caps);
    if (!raw) return;

    GstPad* sink = gst_element_get_static_pad(b->convert, "sink");
    if (!sink) return;
    if (!gst_pad_is_linked(sink)) {
        if (gst_pad_link(pad, sink) != GST_PAD_LINK_OK) {
            std::fprintf(stderr, "[mp4] decodebin -> videoconvert link failed\n");
            b->failed = true;
        }
    }
    gst_object_unref(sink);
    gst_element_sync_state_with_parent(b->convert);
}

static void on_mp4_demux_pad(GstElement*, GstPad* pad, gpointer user) {
    auto* b = static_cast<Mp4Pipe*>(user);
    if (b->video_linked) return;

    GstCaps* caps = gst_pad_get_current_caps(pad);
    if (!caps) caps = gst_pad_query_caps(pad, nullptr);
    // Copy the name out: it points into `caps`, which we release below.
    std::string codec;
    if (caps && !gst_caps_is_empty(caps))
        codec = gst_structure_get_name(gst_caps_get_structure(caps, 0));
    if (caps) gst_caps_unref(caps);
    const bool video = !codec.empty() && g_str_has_prefix(codec.c_str(), "video/");
    if (!video) return;

    GstElement* parser = nullptr;
    if (codec == "video/x-h264")
        parser = gst_element_factory_make("h264parse", "parse");
    else if (codec == "video/x-h265")
        parser = gst_element_factory_make("h265parse", "parse");

    GstPad* decsink = gst_element_get_static_pad(b->decode, "sink");
    if (!decsink) {
        if (parser) gst_object_unref(parser);
        return;
    }
    if (gst_pad_is_linked(decsink)) {
        gst_object_unref(decsink);
        if (parser) gst_object_unref(parser);
        return;
    }

    bool linked = false;
    if (parser) {
        gst_bin_add(GST_BIN(b->pipeline), parser);
        GstPad* psink = gst_element_get_static_pad(parser, "sink");
        GstPad* psrc = gst_element_get_static_pad(parser, "src");
        gst_pad_add_probe(psrc, GST_PAD_PROBE_TYPE_BUFFER, drop_non_key, nullptr, nullptr);
        const bool ok = gst_pad_link(pad, psink) == GST_PAD_LINK_OK &&
                        gst_pad_link(psrc, decsink) == GST_PAD_LINK_OK;
        gst_object_unref(psink);
        gst_object_unref(psrc);
        if (!ok) {
            std::fprintf(stderr, "[mp4] %s parser link failed\n", codec.c_str());
            b->failed = true;
        } else {
            gst_element_sync_state_with_parent(parser);
            linked = true;
        }
    } else {
        gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, drop_non_key, nullptr, nullptr);
        if (gst_pad_link(pad, decsink) != GST_PAD_LINK_OK) {
            std::fprintf(stderr, "[mp4] cannot link %s into decodebin\n", codec.c_str());
            b->failed = true;
        } else {
            linked = true;
        }
    }
    gst_object_unref(decsink);
    if (!linked) return;

    b->video_linked = true;
    gst_element_sync_state_with_parent(b->decode);
    gst_element_sync_state_with_parent(b->convert);
    gst_element_sync_state_with_parent(b->capsfilter);
    gst_element_sync_state_with_parent(b->sink);
    std::fprintf(stderr, "[mp4] video %s, keyframe filter on %s\n",
                 codec.c_str(), parser ? "parser" : "demux");
}

static void copy_nv12_packed(const uint8_t* src, size_t src_size, int w, int h,
                             std::vector<uint8_t>& dst) {
    const size_t packed = static_cast<size_t>(w) * static_cast<size_t>(h) * 3 / 2;
    if (src_size == packed) {
        dst.assign(src, src + src_size);
        return;
    }
    // NV12 byte count is stride * h * 3 / 2. Recover the stride and repack
    // so the rest of the tool can keep treating stride as equal to width.
    if (h > 0 && (src_size * 2) % (static_cast<size_t>(h) * 3) == 0) {
        const int stride = static_cast<int>((src_size * 2) / (static_cast<size_t>(h) * 3));
        if (stride >= w) {
            dst.resize(packed);
            uint8_t* ydst = dst.data();
            const uint8_t* ysrc = src;
            for (int row = 0; row < h; ++row)
                std::memcpy(ydst + static_cast<size_t>(row) * w,
                            ysrc + static_cast<size_t>(row) * stride,
                            static_cast<size_t>(w));
            uint8_t* uvdst = dst.data() + static_cast<size_t>(w) * h;
            const uint8_t* uvsrc = src + static_cast<size_t>(stride) * h;
            for (int row = 0; row < h / 2; ++row)
                std::memcpy(uvdst + static_cast<size_t>(row) * w,
                            uvsrc + static_cast<size_t>(row) * stride,
                            static_cast<size_t>(w));
            return;
        }
    }
    dst.clear();
}

bool Mp4KeyframeCapture::pull(CapFrame& out, bool allow_loop) {
    if (!impl_ || !impl_->sink) return false;
    GstSample* sample = gst_app_sink_try_pull_sample(GST_APP_SINK(impl_->sink),
                                                     5 * GST_SECOND);
    if (!sample) {
        GstBus* bus = gst_element_get_bus(impl_->pipeline);
        GstMessage* msg = bus ? gst_bus_pop_filtered(
            bus, static_cast<GstMessageType>(GST_MESSAGE_EOS | GST_MESSAGE_ERROR))
                              : nullptr;
        const bool saw_eos = msg && GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS;
        if (msg && GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
            GError* err = nullptr;
            gst_message_parse_error(msg, &err, nullptr);
            std::fprintf(stderr, "[mp4] pipeline error: %s\n",
                         err ? err->message : "(unknown)");
            if (err) g_error_free(err);
        }
        if (msg) gst_message_unref(msg);
        if (bus) gst_object_unref(bus);
        if (allow_loop && loop_ && saw_eos) {
            const gboolean ok = gst_element_seek_simple(
                impl_->pipeline, GST_FORMAT_TIME,
                static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                0);
            if (!ok) {
                std::fprintf(stderr, "[mp4] seek back to start failed\n");
                return false;
            }
            return pull(out, false);
        }
        if (!saw_eos) {
            if (impl_->failed)
                std::fprintf(stderr, "[mp4] pipeline failed to link\n");
            else
                std::fprintf(stderr, "[mp4] no keyframe within 5s\n");
        }
        return false;
    }

    int w = w_, h = h_;
    GstCaps* caps = gst_sample_get_caps(sample);
    if (caps && !gst_caps_is_empty(caps)) {
        const GstStructure* s = gst_caps_get_structure(caps, 0);
        gst_structure_get_int(s, "width", &w);
        gst_structure_get_int(s, "height", &h);
    }
    GstBuffer* buf = gst_sample_get_buffer(sample);
    bool ok = false;
    if (buf && w > 0 && h > 0) {
        GstMapInfo mi;
        if (gst_buffer_map(buf, &mi, GST_MAP_READ)) {
            copy_nv12_packed(mi.data, mi.size, w, h, out.buf);
            gst_buffer_unmap(buf, &mi);
            ok = !out.buf.empty();
        }
    }
    gst_sample_unref(sample);
    if (!ok) {
        std::fprintf(stderr, "[mp4] empty or unsupported frame %dx%d\n", w, h);
        return false;
    }
    out.w = w;
    out.h = h;
    out.fmt = CapFmt::NV12;
    out.data = out.buf.data();
    w_ = w;
    h_ = h;
    return true;
}

bool Mp4KeyframeCapture::open(const std::string& path, bool loop) {
    close();
    static bool inited = false;
    if (!inited) {
        gst_init_check(nullptr, nullptr, nullptr);
        inited = true;
    }

    GstElement* pipe = gst_pipeline_new("mp4kf");
    GstElement* src = gst_element_factory_make("filesrc", "src");
    GstElement* demux = gst_element_factory_make("qtdemux", "demux");
    GstElement* decode = gst_element_factory_make("decodebin", "dec");
    GstElement* convert = gst_element_factory_make("videoconvert", "conv");
    GstElement* capsfilter = gst_element_factory_make("capsfilter", "nv12");
    GstElement* sink = gst_element_factory_make("appsink", "cap");
    if (!pipe || !src || !demux || !decode || !convert || !capsfilter || !sink) {
        std::fprintf(stderr,
                     "[mp4] missing gstreamer element (need filesrc, qtdemux, "
                     "decodebin, videoconvert, appsink)\n");
        auto drop = [](gpointer p) { if (p) gst_object_unref(p); };
        drop(src); drop(demux); drop(decode); drop(convert);
        drop(capsfilter); drop(sink); drop(pipe);
        return false;
    }

    impl_ = new Mp4Pipe();
    impl_->pipeline = pipe;
    impl_->decode = decode;
    impl_->convert = convert;
    impl_->capsfilter = capsfilter;
    impl_->sink = sink;
    loop_ = loop;

    g_object_set(src, "location", path.c_str(), nullptr);
    GstCaps* nv12 = gst_caps_from_string("video/x-raw,format=NV12");
    g_object_set(capsfilter, "caps", nv12, nullptr);
    gst_caps_unref(nv12);
    g_object_set(sink, "sync", FALSE, "max-buffers", 1, "drop", TRUE, nullptr);

    gst_bin_add_many(GST_BIN(pipe), src, demux, decode, convert, capsfilter, sink, nullptr);
    if (!gst_element_link(src, demux) ||
        !gst_element_link_many(convert, capsfilter, sink, nullptr)) {
        std::fprintf(stderr, "[mp4] failed to link filesrc/qtdemux or NV12 sink\n");
        close();
        return false;
    }
    g_signal_connect(demux, "pad-added", G_CALLBACK(on_mp4_demux_pad), impl_);
    g_signal_connect(decode, "pad-added", G_CALLBACK(on_mp4_decode_pad), impl_);

    const GstStateChangeReturn r = gst_element_set_state(pipe, GST_STATE_PLAYING);
    if (r == GST_STATE_CHANGE_FAILURE) {
        std::fprintf(stderr, "[mp4] failed to set pipeline PLAYING\n");
        close();
        return false;
    }
    // Prime one keyframe so width/height are known before the capture thread
    // sizes its mailbox. grab() returns this frame first.
    if (!pull(pending_, false)) {
        std::fprintf(stderr, "[mp4] cannot read a keyframe from %s\n", path.c_str());
        close();
        return false;
    }
    has_pending_ = true;
    std::fprintf(stderr, "[mp4] %s %dx%d keyframes only%s\n",
                 path.c_str(), w_, h_, loop_ ? ", loop" : "");
    return true;
}

bool Mp4KeyframeCapture::grab(CapFrame& out) {
    if (has_pending_) {
        out = std::move(pending_);
        out.data = out.buf.empty() ? nullptr : out.buf.data();
        pending_ = CapFrame{};
        has_pending_ = false;
        return out.data != nullptr;
    }
    return pull(out, true);
}

void Mp4KeyframeCapture::close() {
    has_pending_ = false;
    pending_ = CapFrame{};
    if (!impl_) return;
    if (impl_->pipeline) {
        gst_element_set_state(impl_->pipeline, GST_STATE_NULL);
        gst_object_unref(impl_->pipeline);
    }
    delete impl_;
    impl_ = nullptr;
    w_ = 0;
    h_ = 0;
}

} // namespace cap

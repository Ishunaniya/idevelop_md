// detection_shm.h -- producer side of /detection_boxes.
//
// The CONSUMER already exists and is deployed: qt5_qml_c001's
// DetectionBoxShmBridge (qml/src MainPage.qml binds to it). What was missing was
// the producer -- /dev/shm/detection_boxes existed on the board with magic
// 0xDE7EC710 and a stale seq, but nothing was writing it, so the UI never
// showed a box.
//
// This file is the write side of that exact contract. The layout below is NOT
// a copy by convenience: it is byte-for-byte dictated by the reader. The
// static_asserts at the bottom are the guard -- if the reader's struct ever
// changes, this file must be changed with it, and the build will say so.
//
//   offset 0   : DetectionShmHeader
//   offset 16  : channels[0]
//   offset 1636: channels[1] ... channels[3]
//   total 6496 bytes (matches the `stat` of the live shm segment on the board)
//
// Concurrency: the reader uses a SEQLOCK, not a mutex:
//   reader: s1 = seq.load(acquire); if (s1 odd) skip; ... ; s2 = seq.load();
//           if (s1 != s2) discard and retry
//   writer: seq++            -> odd  == "write in progress, do not read"
//           ... write payload ...
//           seq++            -> even == "payload stable"
// So the writer must bracket the payload with two increments. Publishing seq
// only once at the end (the naive approach) would let the reader accept a
// half-updated frame, which on a live monitor shows up as boxes flickering or
// a stale width/height paired with fresh boxes.
#ifndef EVS_DETECTION_SHM_H
#define EVS_DETECTION_SHM_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "evs/status.h"

namespace evs_host {

// ---- constants mirrored from qt5_qml_c001/src/DetectionBoxShmBridge.h ----
constexpr uint32_t kDetShmMagic = 0xDE7EC710u;
constexpr uint32_t kDetShmVersion = 1u;
constexpr int kDetMaxBoxes = 64;
constexpr int kDetNumChannels = 4;
constexpr const char* kDetShmName = "/detection_boxes";

struct DetectionBox {
    int32_t x1, y1, x2, y2;  // PIXELS, top-left + bottom-right (inclusive)
    float conf;
    float mappedY;           // ground distance in cm; <=0 means "unknown"
};

struct DetectionChannelData {
    std::atomic<uint32_t> seq;
    uint32_t has;
    uint32_t width, height;
    uint32_t boxCount;
    DetectionBox boxes[kDetMaxBoxes];
    uint8_t padding[64];
};

struct DetectionShmHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t reserved[2];
    DetectionChannelData channels[kDetNumChannels];
};

static_assert(sizeof(DetectionBox) == 24, "DetectionBox must stay 24 bytes");
static_assert(sizeof(DetectionChannelData) == 1620, "channel stride mismatch");
static_assert(sizeof(DetectionShmHeader) == 6496, "shm total must stay 6496");
static_assert(offsetof(DetectionShmHeader, channels) == 16, "header stride");
static_assert(sizeof(DetectionBox) == 4 * sizeof(int32_t) + 2 * sizeof(float),
              "box field order must match the reader");

// Publisher for one channel of the detection-box segment.
//
// Lifetime: open() once at startup, publish() per frame, close() at exit.
// Not thread-safe by design -- it is meant to be driven by the single inference
// thread, which is also the only writer of the corresponding channel.
class DetectionShmWriter {
public:
    ~DetectionShmWriter() { close(); }

    DetectionShmWriter() = default;
    DetectionShmWriter(const DetectionShmWriter&) = delete;
    DetectionShmWriter& operator=(const DetectionShmWriter&) = delete;

    // Creates (or re-attaches to) the segment and claims `channel`.
    // Re-attaching to a segment left behind by a previous run keeps the
    // reader's lastSeq continuity intact, so the UI keeps showing boxes across
    // a restart instead of blanking until Qt's timer re-syncs.
    bool open(int channel);

    // Publishes one frame's worth of boxes.
    //
    // `targets` are in evs::Target convention: x/y = CENTRE and w/h = SIZE,
    // all NORMALISED to 0..1 of the original frame. The reader wants PIXELS in
    // top-left + bottom-right form, so the conversion happens here and nowhere
    // else:
    //     x1 = (x - w/2) * W      x2 = (x + w/2) * W
    //     y1 = (y - h/2) * H      y2 = (y + h/2) * H
    // Confusing the two conventions is the easiest way to make perfectly good
    // detections draw in the wrong place, so it is confined to this function.
    //
    // `dist_cm` is the ground distance of the nearest person in cm, matching
    // what MainPage.qml expects (it divides by 100 to show metres). Pass a
    // non-positive value when unknown -- the QML explicitly filters those out
    // instead of treating them as "very close".
    void publish(const std::vector<evs::Target>& targets, int frame_w,
                 int frame_h, float dist_cm);

    // Marks the channel as stale (camera lost / inference wedged) so the UI
    // stops drawing boxes that are no longer real.
    void invalidate();

    void close();
    bool opened() const { return shm_ != nullptr; }
    uint32_t seq() const {
        return shm_ ? shm_->channels[channel_].seq.load(std::memory_order_relaxed)
                    : 0u;
    }

private:
    DetectionShmHeader* shm_ = nullptr;
    int fd_ = -1;
    int channel_ = 0;
};

} // namespace evs_host

#endif // EVS_DETECTION_SHM_H

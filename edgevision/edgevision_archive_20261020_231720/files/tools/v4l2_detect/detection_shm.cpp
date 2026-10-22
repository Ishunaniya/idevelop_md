// detection_shm.cpp -- see detection_shm.h for the contract rationale.
#include "detection_shm.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

namespace evs_host {

namespace {

// Clamp helper: the reader casts these to int32 and draws rectangles with
// them, so an out-of-range value would produce a garbage box, not a clipped one.
int32_t clamp_i32(float v, int32_t lo, int32_t hi) {
    const int32_t i = static_cast<int32_t>(v);
    if (i < lo) return lo;
    if (i > hi) return hi;
    return i;
}

} // namespace

bool DetectionShmWriter::open(int channel) {
    close();
    if (channel < 0 || channel >= kDetNumChannels) {
        std::fprintf(stderr, "[det_shm] bad channel %d (0..%d)\n", channel,
                     kDetNumChannels - 1);
        return false;
    }
    channel_ = channel;

    // O_RDWR: we are the producer, we must be able to create AND write.
    fd_ = shm_open(kDetShmName, O_RDWR, 0666);
    if (fd_ < 0) {
        std::fprintf(stderr, "[det_shm] shm_open %s failed: %s\n", kDetShmName,
                     std::strerror(errno));
        return false;
    }
    // Create the segment if this is the first producer on the board. The size
    // must match the reader's sizeof(DetectionShmHeader) exactly -- Qt rejects
    // the segment outright when it is smaller than it expects.
    if (ftruncate(fd_, static_cast<off_t>(sizeof(DetectionShmHeader))) != 0) {
        std::fprintf(stderr, "[det_shm] ftruncate failed: %s\n",
                     std::strerror(errno));
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    shm_ = static_cast<DetectionShmHeader*>(
        mmap(nullptr, sizeof(DetectionShmHeader), PROT_READ | PROT_WRITE,
             MAP_SHARED, fd_, 0));
    if (shm_ == MAP_FAILED) {
        std::fprintf(stderr, "[det_shm] mmap failed: %s\n", std::strerror(errno));
        shm_ = nullptr;
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    // Header handshake. A live segment already carries the right magic (the
    // reader only maps it when the magic matches), so re-attaching must NOT
    // rewrite the header -- that would reset every channel's seq to 0 and make
    // the reader's lastSeq comparison look like a brand-new stream.
    if (shm_->magic != kDetShmMagic) {
        shm_->magic = kDetShmMagic;
        shm_->version = kDetShmVersion;
        shm_->reserved[0] = 0;
        shm_->reserved[1] = 0;
        for (int i = 0; i < kDetNumChannels; ++i) {
            shm_->channels[i].seq.store(0, std::memory_order_relaxed);
            shm_->channels[i].has = 0;
            shm_->channels[i].width = 0;
            shm_->channels[i].height = 0;
            shm_->channels[i].boxCount = 0;
        }
    }

    // Force an EVEN seq on our channel so the reader will accept the next
    // frame. If a previous producer was killed mid-publish the seq could be
    // left odd, and the reader would then skip every frame forever.
    auto& ch = shm_->channels[channel_];
    uint32_t s = ch.seq.load(std::memory_order_relaxed);
    if (s & 1u) ch.seq.store(s + 1, std::memory_order_release);

    std::fprintf(stderr,
                 "[det_shm] attached %s ch=%d size=%zu seq=%u (seqlock, even)\n",
                 kDetShmName, channel_, sizeof(DetectionShmHeader),
                 ch.seq.load(std::memory_order_relaxed));
    return true;
}

void DetectionShmWriter::publish(const std::vector<evs::Target>& targets,
                                 int frame_w, int frame_h, float dist_cm) {
    if (!shm_ || frame_w <= 0 || frame_h <= 0) return;
    auto& ch = shm_->channels[channel_];

    // --- seqlock: open the write window (seq becomes ODD) -----------------
    // The reader treats an odd seq as "write in progress" and skips the frame.
    // Everything below must stay inside this window.
    ch.seq.fetch_add(1, std::memory_order_acq_rel);

    ch.width = static_cast<uint32_t>(frame_w);
    ch.height = static_cast<uint32_t>(frame_h);

    // The reader does std::min(boxCount, DETECTION_MAX_BOXES) but also loops
    // over boxCount, so clamp on our side and never write past the array.
    const size_t n = targets.size() < static_cast<size_t>(kDetMaxBoxes)
                         ? targets.size()
                         : static_cast<size_t>(kDetMaxBoxes);

    // Default: distance unknown. QML ignores non-positive mappedY, which is
    // exactly right -- better "no distance" than a fabricated one.
    float nearest = (dist_cm > 0.f) ? dist_cm : -1.f;

    for (size_t i = 0; i < n; ++i) {
        const evs::Target& t = targets[i];
        // evs::Target: x/y = centre, w/h = size, all 0..1 of the frame.
        // Reader wants pixels, top-left + bottom-right.
        const float hw = t.w * 0.5f;
        const float hh = t.h * 0.5f;
        DetectionBox& b = ch.boxes[i];
        b.x1 = clamp_i32((t.x - hw) * frame_w, 0, frame_w - 1);
        b.y1 = clamp_i32((t.y - hh) * frame_h, 0, frame_h - 1);
        b.x2 = clamp_i32((t.x + hw) * frame_w, 0, frame_w - 1);
        b.y2 = clamp_i32((t.y + hh) * frame_h, 0, frame_h - 1);
        b.conf = t.confidence;
        // Only the nearest target carries a distance (the reader treats
        // mappedY as "the distance to the closest person"); evs::Target's
        // distance_m is only meaningful once phase-4 ground mapping lands, so
        // prefer the explicit argument and fall back to the per-target value.
        b.mappedY = (i == 0) ? nearest : (t.distance_m > 0.f
                                                ? t.distance_m * 100.f
                                                : -1.f);
    }
    ch.boxCount = static_cast<uint32_t>(n);
    ch.has = 1;

    // --- seqlock: close the write window (seq becomes EVEN) ---------------
    // This release store is what makes the payload above visible to the
    // reader before it observes the final seq.
    ch.seq.fetch_add(1, std::memory_order_release);
}

void DetectionShmWriter::invalidate() {
    if (!shm_) return;
    auto& ch = shm_->channels[channel_];
    ch.seq.fetch_add(1, std::memory_order_acq_rel);   // odd: write in progress
    ch.has = 0;
    ch.boxCount = 0;
    ch.seq.fetch_add(1, std::memory_order_release);    // even: stable again
}

void DetectionShmWriter::close() {
    if (shm_) {
        // Leave has=0 behind so a stale box is never left on screen after the
        // producer exits.
        invalidate();
        munmap(shm_, sizeof(DetectionShmHeader));
        shm_ = nullptr;
    }
    if (fd_ >= 0) {
        // shm_unlink is deliberately NOT called: the reader keeps polling the
        // same name and the segment must outlive the producer.
        ::close(fd_);
        fd_ = -1;
    }
}

} // namespace evs_host

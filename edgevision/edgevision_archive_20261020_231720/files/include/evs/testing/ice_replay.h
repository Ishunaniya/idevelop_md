#pragma once
#include "evs/winch_ice.h"

namespace evs {
class VisionTask;

namespace testing {
struct IceMaskSnapshot {
    uint32_t winch_id = 0;
    int width = 0, height = 0;
    uint32_t timestamp_ms = 0;
    std::vector<uint8_t> surface, snow; // original-size grayscale, 0 or 255
};

// Read-only diagnostic snapshots for demo overlays. No image/box injection.
class IceReplayAccess {
public:
    static bool copyMasks(const winch_ice::Detector&, std::vector<IceMaskSnapshot>&);
};
} // namespace testing
} // namespace evs

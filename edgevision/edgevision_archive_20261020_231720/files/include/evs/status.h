#pragma once
#include <cstdint>
#include <vector>

namespace evs {

// Alarm level for a single detection result.
enum class AlarmState : int { None = 0, LogOnly = 1, Alarm = 2 };

// A target exists but cannot be projected to a trustworthy ground position.
constexpr float kDistanceUnknown = -1.f;

// Ice ratios belong to the current frame; use per-object valid/status.
// Unobservable objects are unknown, never zero coverage.
constexpr uint32_t kIceUnobservable = 0x0008C001u;
constexpr float kIceRatioUnknown = -1.f;

// Error codes are 0xMMMMEEEE. High 16 bits are the function module
// (0x0004 load-near, 0x0005 load-under, 0x0006 boom-under, 0x0007 height,
// 0x0008 winch ice). Low 16 bits are the error. Values below keep the high
// half at 0 so they can be stamped with with_module() once the task is known.
// 0x0000..0x0FFF is reserved for success. 0x1000..0xBFFF is the shared table.
// 0xC000..0xFFFF is for one function's own errors.
enum class EvsStatus : uint32_t {
    Success            = 0x00000000,
    InvalidParameter   = 0x00001001,
    NullPointer        = 0x00001002,
    OutOfRange         = 0x00001003,
    MemoryAllocation   = 0x00001004,
    InvalidCameraIndex = 0x00001005,
    Timeout            = 0x00001006,
    NotInitialized     = 0x00001007,
    AlreadyRunning     = 0x00001008,
    NotSupported       = 0x00001009,
    HardwareFault      = 0x0000100A,

    ModelNotLoaded     = 0x00002001,
    ModelLoadFailed    = 0x00002002,
    ModelInvalidFormat = 0x00002003,

    InferFailed        = 0x00003001,
    InvalidImageData   = 0x00003002,
    InvalidImageSize   = 0x00003003,
    PreprocessFailed   = 0x00003004,
    PostprocessFailed  = 0x00003005,

    ConfigInvalid      = 0x00004001,
    ConfigParse        = 0x00004002,

    NetworkDisconnect  = 0x00005001,
    NetworkTimeout     = 0x00005002,

    StorageRead        = 0x00006001,
    StorageWrite       = 0x00006002,
    StorageFull        = 0x00006003,
};

// Put the function module into the high 16 bits. A code that already names a
// module, including make_module_error(), is left unchanged. module 0 and
// success stay as they are.
inline constexpr uint32_t with_module(uint16_t module, uint32_t status) {
    if (status == 0u || module == 0u || (status >> 16) != 0u) return status;
    return (static_cast<uint32_t>(module) << 16) | (status & 0xFFFFu);
}

// Function-private error in 0xC000..0xFFFF. A code already in that range is
// kept; a small ordinal is placed at 0xC000 + ordinal.
inline constexpr uint32_t make_module_error(uint16_t module, uint16_t code) {
    const uint16_t low = code >= 0xC000u
                             ? code
                             : static_cast<uint16_t>(0xC000u | (code & 0x0FFFu));
    return (static_cast<uint32_t>(module) << 16) | low;
}

// camera_channel is one uint32. Each byte is one camera id, little-endian,
// so one result covers at most four cameras. Byte 0 is the first image of
// this call, byte 1 the second, and so on. 0xFF means that byte is unused:
// channel 0 is a real camera, so zero cannot mean "empty".
constexpr uint8_t kCameraChannelUnused = 0xFFu;

inline constexpr uint32_t pack_camera_channel(uint8_t c0,
                                              uint8_t c1 = kCameraChannelUnused,
                                              uint8_t c2 = kCameraChannelUnused,
                                              uint8_t c3 = kCameraChannelUnused) {
    return static_cast<uint32_t>(c0) |
           (static_cast<uint32_t>(c1) << 8) |
           (static_cast<uint32_t>(c2) << 16) |
           (static_cast<uint32_t>(c3) << 24);
}

// A single detected target in original-frame relative coordinates.
struct Target {
    int class_id = -1;
    float confidence = 0.f;
    float x = 0.f, y = 0.f, w = 0.f, h = 0.f; // relative to original image, 0..1
    float distance_m = -1.f;
    float angle_deg = 0.f;  // clockwise, relative to the function's reference point
    int track_id = -1;
    // Appended for ABI evolution: vehicle/local-ground Cartesian coordinates.
    // ground_ok=false means the mapper could not produce a trustworthy fix.
    float ground_x_m = 0.f;
    float ground_y_m = 0.f;
    bool ground_ok = false;
};

// Output of a single detect() call.
struct Result {
    uint32_t status = 0;       // 0 success, otherwise 0xMMMMEEEE
    AlarmState alarm = AlarmState::None;
    std::vector<Target> targets;
    int limit_state = 0;       // height limit: 0 unknown,1 not installed,2 wrong,3 correct
    float ice_ratio = 0.f;     // single valid ice object: 0..1; multiple/unknown: -1
    bool camera_blind = false; // optical blind; frame not counted in person miss-rate
    // Cameras that produced this result. See pack_camera_channel.
    uint32_t camera_channel = 0xFFFFFFFFu;
};

} // namespace evs

#pragma once
#include <cstdint>

namespace evs {

// Pixel format of the frame buffer supplied by the host.
enum class PixelFormat { NV12, RGB, BGR };

// Input frame. The buffer (data / dma_fd) is owned by the host and is
// guaranteed valid only until detect() returns.
struct Frame {
    int camera_id = -1;
    int width = 0;
    int height = 0;
    int stride = 0;
    PixelFormat format = PixelFormat::NV12;
    int64_t timestamp_us = 0;
    const uint8_t* data = nullptr; // host guarantees validity until detect returns
    int dma_fd = -1;               // -1 when no dma-buf is available
};

// Crane working-state context supplied by the host (MCU / PLC).
struct MachineContext {
    bool context_valid = true;
    float focal_length = 0.f;        // 焦距
    bool hoist_moving = false;       // hoist up/down
    bool telescope_moving = false;   // telescoping boom
    float boom_length = 0.f;         // boom length, metres, spec 0..200
    float boom_angle = 0.f;          // boom-tail angle, degrees, spec 0..90
    float hoist_radius = 0.f;        // working radius, metres, spec 0..200
    int is_working = 0;              // 0 idle, 1 working; also see motion fields
    int64_t timestamp_us = 0;
};

} // namespace evs

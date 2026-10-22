#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "evs/status.h"

namespace evs {

// Inference backend abstraction. One instance is shared (via the model registry)
// per (backend type, model path). Implementations must be PIMPL-friendly and
// must never expose their SDK headers through this interface.
class IInferBackend {
public:
    virtual ~IInferBackend() = default;

    // Load model from path with an init timeout (seconds). Returns EvsStatus.
    virtual uint32_t load(const std::string& model_path, int init_timeout_s) = 0;

    // Run inference on packed input bytes; fills output tensor (raw floats).
    virtual uint32_t infer(const std::vector<uint8_t>& input,
                           std::vector<float>& output) = 0;

    // Optional: inform the backend of the input tensor shape (width, height in
    // pixels) before the first infer(). Most backends derive shape from the
    // model and ignore this (default no-op); the x86 reference detector uses it
    // to interpret the packed NHWC buffer spatially. Always safe to call.
    virtual void configure_input(int /*width*/, int /*height*/) {}

    virtual void release() = 0;
    virtual bool loaded() const = 0;
};

} // namespace evs

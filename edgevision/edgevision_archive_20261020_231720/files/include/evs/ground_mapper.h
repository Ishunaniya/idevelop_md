#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "evs/frame.h"
#include "evs/ground_config.h"
#include "evs/status.h"

namespace evs {

struct GroundPoint {
    bool ok = false;
    float x_m = 0.f; // vehicle/local-ground +X, metres
    float y_m = 0.f; // vehicle/local-ground +Y, metres
};

class IGroundMapper {
public:
    virtual ~IGroundMapper() = default;

    virtual uint32_t configure(const CameraGroundConfig& cfg) = 0;
    virtual GroundPoint map(const Target& target, const Frame& frame,
                            const MachineContext& ctx) const = 0;
};

// Returns nullptr for an unknown mapper type or when configuration fails.
// On failure, status receives the unstamped code when the pointer is not null.
std::unique_ptr<IGroundMapper> create_ground_mapper(const CameraGroundConfig& cfg,
                                                    uint32_t* status = nullptr);

} // namespace evs

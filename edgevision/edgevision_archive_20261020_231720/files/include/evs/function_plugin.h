#pragma once
#include <cstdint>
#include <memory>
#include <vector>

#include "evs/frame.h"
#include "evs/ground_metrics.h"
#include "evs/status.h"
#include "evs/task_config.h"

namespace evs {
namespace winch_ice {
struct WinchSnowResult;
}
namespace testing {
struct IceMaskSnapshot;
}

// Function-rule plugin: person / height-limit / winch-ice.
// VisionTask calls decide() after decode and map-back to the original frame.
struct IFunctionPlugin {
    virtual ~IFunctionPlugin() = default;

    virtual uint16_t module_id() const = 0;

    // Read this module's policy from TaskConfig. Non-zero means failure
    // (EvsStatus or a module error). Called from initialize and updateConfig.
    virtual uint32_t configure(const TaskConfig& cfg) = 0;

    // Runs before any model preprocessing/inference. Default leaves other tasks unchanged.
    virtual uint32_t preflight(const Frame&, const MachineContext&, Result&, bool& run) {
        run = true;
        return 0;
    }

    virtual const winch_ice::WinchSnowResult* winch_result() const { return nullptr; }

    // Record a rejected call. Ice clears the previous frame result.
    virtual void reject_call(uint32_t status, Result& out) noexcept { out.status = status; }

    virtual bool copy_ice_masks(std::vector<testing::IceMaskSnapshot>&) const { return false; }

    // Task-relative ground frame. false means this function does not consume
    // ground distance. Implementations only describe the reference; the shared
    // geometry layer computes distance_m / angle_deg.
    virtual bool ground_reference(const MachineContext&,
                                  GroundReference&) const {
        return false;
    }

    // One frame. `frame` is valid only until decide() returns. `targets` are
    // already mapped back to the original image (0..1). Write only the fields
    // this plugin owns; see the field contract in the design note.
    virtual uint32_t decide(const Frame& frame,
                            const MachineContext& ctx,
                            const std::vector<Target>& targets,
                            Result& out) = 0;

    // Clear cross-frame state (consecutive counts, observation windows).
    virtual void reset() = 0;
};

// nullptr when module_id is not a registered function.
std::unique_ptr<IFunctionPlugin> create_function_plugin(uint16_t module_id);

} // namespace evs

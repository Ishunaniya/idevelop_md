#pragma once
#include "evs/function_plugin.h"
#include "evs/winch_ice.h"
#include "ice_measurement.h"

namespace evs {
class WinchIcePlugin : public IFunctionPlugin {
public:
    uint16_t module_id() const override { return 0x0008; }

    uint32_t configure(const TaskConfig&) override;
    uint32_t preflight(const Frame&, const MachineContext&, Result&, bool& run) override;
    void reject_call(uint32_t status, Result& out) noexcept override;
    uint32_t decide(const Frame&, const MachineContext&, const std::vector<Target>&,
                    Result&) override;

    const winch_ice::WinchSnowResult* winch_result() const override { return &report_; }

    bool copy_ice_masks(std::vector<testing::IceMaskSnapshot>&) const override;
    void reset() override;

private:
    void summarize() noexcept;
    void publish(Result&, uint32_t status = 0) noexcept;
    TaskConfig cfg_;
    winch_ice::WinchSnowResult report_;
    bool configured_ = false;
#if defined(EVS_WITH_OPENCV)
    std::vector<IceMeasurement> measurements_;
#endif
};
} // namespace evs

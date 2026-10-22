#include "evs_register.h"
#include "ground_mapper_internal.h"

namespace evs {
namespace {

class CranePoseMapper final : public IGroundMapper {
public:
    uint32_t configure(const CameraGroundConfig& cfg) override {
        cfg_ = cfg;
        return 0u;
    }

    GroundPoint map(const Target&, const Frame&,
                    const MachineContext& ctx) const override {
        (void)cfg_;
        (void)ctx.boom_angle;
        (void)ctx.boom_length;
        (void)ctx.hoist_radius;
        // The calibrated camera/boom projection formula is intentionally not
        // guessed. Until it is supplied this mapper reports an unknown fix.
        return GroundPoint{};
    }

private:
    CameraGroundConfig cfg_{};
};

} // namespace

std::unique_ptr<IGroundMapper> make_crane_pose_mapper() {
    return std::make_unique<CranePoseMapper>();
}

namespace {

const GroundMapperRegistration kCrane("crane", make_crane_pose_mapper);
const GroundMapperRegistration kCranePose("crane_pose", make_crane_pose_mapper);

} // namespace

} // namespace evs

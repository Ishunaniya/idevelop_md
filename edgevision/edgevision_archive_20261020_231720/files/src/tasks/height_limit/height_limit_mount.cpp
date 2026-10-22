#include "height_limit_mount.h"

namespace evs {

int judge_switch_mount(const Frame& frame, const Target& box) {
#if defined(EVS_WITH_OPENCV)
    // Crop the model box out of the original frame and judge the mount from
    // that image. The geometric rule is added here after the installation
    // pictures are defined. Until then the result stays unknown.
    if (frame.data == nullptr || frame.width <= 0 || frame.height <= 0)
        return 0;
    if (box.w <= 0.f || box.h <= 0.f)
        return 0;
    (void)box;
    return 0;
#else
    (void)frame;
    (void)box;
    return 0;
#endif
}

} // namespace evs

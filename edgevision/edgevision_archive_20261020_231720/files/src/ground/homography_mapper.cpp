#include "evs_register.h"
#include "ground_mapper_internal.h"

#include <cmath>
#include <fstream>

#include "evs/ground_params.h"
#include "evs/log.h"
#include "evs/status.h"

namespace evs {
namespace {

constexpr double kEps = 1e-9;
constexpr int kLegacyChannels = 4;

// Legacy defaults from yolov5_object_detect. Matrix world units are centimetres.
const double kBuiltinH[kLegacyChannels][9] = {
    {-7.7275342703e+00, -1.3471250136e-03, 5.2985900721e+03,
     -2.5097060180e-01, 5.9542166758e+00, -3.9374861256e+03,
     -3.5012757092e-04, -2.5823934544e-02, 1.0000000000e+00},
    {-1.838861103233138650e+01, 1.638688583635547369e+00, 1.154593057687211149e+04,
     6.110064519126514604e-01, 1.515734290879510837e+01, -1.148967331886159263e+04,
     -4.505902192228948035e-03, -4.053284040860953563e-02, 1.000000000000000000e+00},
    {3.809943157808276748e+00, 2.135679816757244109e-01, -2.667109430813105064e+03,
     -2.825805101355236340e-02, -3.289045944596209914e+00, 2.309640302759657516e+03,
     -1.210134438337869120e-03, 8.893815429964053304e-03, 1.000000000000000000e+00},
    {3.809943157808276748e+00, 2.135679816757244109e-01, -2.667109430813105064e+03,
     -2.825805101355236340e-02, -3.289045944596209914e+00, 2.309640302759657516e+03,
     -1.210134438337869120e-03, 8.893815429964053304e-03, 1.000000000000000000e+00},
};

std::string legacy_matrix_path(int id) {
    return "/userdata/matrix_" + std::to_string(id) + "_1280x720.txt";
}

bool load_matrix(const std::string& path, double h[9]) {
    std::ifstream in(path);
    if (!in) return false;
    double tmp[9];
    for (double& value : tmp)
        if (!(in >> value)) return false;
    for (int i = 0; i < 9; ++i) h[i] = tmp[i];
    return true;
}

class HomographyMapper final : public IGroundMapper {
public:
    uint32_t configure(const CameraGroundConfig& cfg) override {
        cfg_ = cfg;
        cfg_.matrix_path =
            ground_param_string(cfg, "matrix", cfg.matrix_path);
        cfg_.calibration_width =
            ground_param_int(cfg, "calibration_width", cfg.calibration_width);
        cfg_.calibration_height =
            ground_param_int(cfg, "calibration_height", cfg.calibration_height);
        if (cfg_.calibration_width <= 0 || cfg_.calibration_height <= 0)
            return static_cast<uint32_t>(EvsStatus::InvalidParameter);

        const bool has_builtin = cfg_.id >= 0 && cfg_.id < kLegacyChannels;
        if (has_builtin)
            for (int i = 0; i < 9; ++i) h_[i] = kBuiltinH[cfg_.id][i];

        const std::string path = cfg_.matrix_path.empty()
                                     ? legacy_matrix_path(cfg_.id)
                                     : cfg_.matrix_path;
        if (!load_matrix(path, h_)) {
            const bool explicit_path = has_ground_param(cfg, "matrix") ||
                                       has_ground_param(cfg, "matrix_path");
            const bool strict =
                ground_param_bool(cfg, "strict_matrix", explicit_path);
            if (strict || !has_builtin)
                return static_cast<uint32_t>(EvsStatus::ConfigInvalid);
            EVS_LOGW("camera %d matrix %s unavailable; using builtin legacy matrix",
                     cfg_.id, path.c_str());
        }
        return 0u;
    }

    GroundPoint map(const Target& target, const Frame& frame,
                    const MachineContext&) const override {
        GroundPoint out;
        if (frame.width <= 0 || frame.height <= 0 || target.w <= 0.f || target.h <= 0.f)
            return out;

        // Bottom-centre foot point in calibration-image pixels.
        const double u = target.x * static_cast<double>(cfg_.calibration_width);
        const double v = (target.y + target.h * 0.5f) *
                         static_cast<double>(cfg_.calibration_height);
        const double xh = h_[0] * u + h_[1] * v + h_[2];
        const double yh = h_[3] * u + h_[4] * v + h_[5];
        const double wh = h_[6] * u + h_[7] * v + h_[8];
        if (std::fabs(wh) < kEps) return out;

        double x_m = (xh / wh) / 100.0;
        double y_m = (yh / wh) / 100.0;
        if (!std::isfinite(x_m) || !std::isfinite(y_m) || !(y_m > 0.0))
            return out;

        if (cfg_.mount.enabled) {
            // mount.on_boom / offset_m are reserved for CranePoseMapper. This
            // fixed homography path only applies the static XY/yaw transform.
            const double yaw = cfg_.mount.yaw_deg * 3.14159265358979323846 / 180.0;
            const double c = std::cos(yaw);
            const double s = std::sin(yaw);
            const double rx = c * x_m - s * y_m;
            const double ry = s * x_m + c * y_m;
            x_m = rx + cfg_.mount.x_m;
            y_m = ry + cfg_.mount.y_m;
        }

        out.ok = true;
        out.x_m = static_cast<float>(x_m);
        out.y_m = static_cast<float>(y_m);
        return out;
    }

private:
    CameraGroundConfig cfg_{};
    double h_[9]{};
};

} // namespace

std::unique_ptr<IGroundMapper> make_homography_mapper() {
    return std::make_unique<HomographyMapper>();
}

namespace {

const GroundMapperRegistration kHomography("homography", make_homography_mapper);

} // namespace

} // namespace evs

#include "capture.h"
#include "replay_io.h"
#include <cstdlib>
#include <iostream>

int main(int argc, char** argv) {
    try {
        using namespace ice_demo;
        std::string task, context_path, camera_config, device = "/dev/video0", out;
        int channel = 0, width = 1280, height = 720, fps = 25, frames = 100, working = 0;
        float angle = 0;
        bool simulate = false;
        for (int i = 1; i < argc; ++i) {
            std::string a = argv[i];
            auto next = [&]() {
                if (i + 1 >= argc)
                    throw std::runtime_error("missing value for " + a);
                return std::string(argv[++i]);
            };
            if (a == "--task")
                task = next();
            else if (a == "--context-json")
                context_path = next();
            else if (a == "--config")
                camera_config = next();
            else if (a == "--device")
                device = next();
            else if (a == "--out")
                out = next();
            else if (a == "--channel")
                channel = std::stoi(next());
            else if (a == "--width")
                width = std::stoi(next());
            else if (a == "--height")
                height = std::stoi(next());
            else if (a == "--fps")
                fps = std::stoi(next());
            else if (a == "--frames")
                frames = std::stoi(next());
            else if (a == "--angle")
                angle = std::stof(next());
            else if (a == "--working")
                working = std::stoi(next());
            else if (a == "--simulate-context")
                simulate = true;
            else
                throw std::runtime_error("unknown argument " + a);
        }
        if (task.empty() || (simulate == !context_path.empty()) || frames <= 0)
            throw std::runtime_error(
                "usage: winch_ice_demo --task yaml [--config camera.json --channel N | --device "
                "dev --width W --height H] (--simulate-context --angle 0 --working 1 | "
                "--context-json live.json) [--frames N --out prefix]");
        WinchSnowConfig cfg;
        if (auto rc = load_config(task, cfg))
            throw std::runtime_error("config error " + std::to_string(rc));
        if (!camera_config.empty()) {
            auto entries = cap::load_capture_config(camera_config);
            auto e = cap::select_entry(entries, channel);
            if (!e)
                throw std::runtime_error("camera channel missing");
            device = e->device;
            width = e->width;
            height = e->height;
            if (e->framerate > 0)
                fps = e->framerate;
        }
        if (channel != int(cfg.camera_channel))
            throw std::runtime_error("camera channel differs from task");
        if (width < 2 || height < 2 || fps <= 0)
            throw std::runtime_error("invalid capture size or framerate");
        Detector detector;
        if (auto rc = detector.initialize(cfg))
            throw std::runtime_error("initialize error " + std::to_string(rc));
        cap::GstCapture camera;
        if (!camera.open(device, width, height, fps, true))
            throw std::runtime_error("camera open failed");
        std::cerr << "context_source=" << (simulate ? "SIMULATED" : "external JSON")
                  << "; frame-by-frame detection\n";
        WinchSnowResult result;
        result.camera_channel = cfg.camera_channel;

        uint32_t any_error = 0;
        for (int i = 0; i < frames; ++i) {
            cap::CapFrame f;
            // RK3588 camera startup can exceed three seconds; allow eight for
            // the first frame, matching GstCapture::grab's startup allowance.
            if (!camera.grab_for(f, i == 0 ? 8000 : 3000) || !f.data)
                throw std::runtime_error("frame timeout");
            if (f.fmt != cap::CapFmt::NV12 || (!f.pixel_format.empty() && f.pixel_format != "NV12"))
                throw std::runtime_error("requires packed NV12");
            const auto packed_bytes = uint64_t(f.w) * uint64_t(f.h) * 3 / 2;
            if (f.w < 2 || f.h < 2 || ((f.w | f.h) & 1) || f.buf.size() != packed_bytes)
                throw std::runtime_error(
                    "NV12 buffer is not tightly packed; stride-aware capture is required");
            if (i == 0) {
                std::cerr << "negotiated frame=" << f.w << 'x' << f.h
                          << "; capture pipeline controls resolution/framerate\n";
            }
            ImageData im;
            im.data = f.data;
            im.width = f.w;
            im.height = f.h;
            im.channels = 1;
            im.format = ImageFormat::NV12;
            im.stride = f.w;
            im.size_bytes = f.buf.size();
            im.timestamp_ms = fresh_time();
            HostWorkingCondition ctx;
            if (simulate) {
                ctx.valid = true;
                ctx.boom_angle = angle;
                ctx.is_working = working;
                ctx.timestamp_ms = im.timestamp_ms;
            } else {
                try {
                    cv::FileStorage state(context_path, cv::FileStorage::READ);
                    if (state.isOpened() && !state["boom_angle"].empty()) {
                        ctx.boom_angle =
                            static_cast<float>(static_cast<double>(state["boom_angle"]));
                        ctx.is_working =
                            static_cast<int32_t>(integer(state["is_working"], "is_working"));
                        ctx.valid = integer(state["valid"], "valid") == 1;
                        ctx.timestamp_ms = integer(state["timestamp_ms"], "timestamp_ms");
                    }
                } catch (const std::exception& e) {
                    ctx = HostWorkingCondition{};
                    std::cerr << "invalid context: " << e.what() << '\n';
                }
            }
            const auto decision = host_decision(ctx, im.timestamp_ms, monotonic_timestamp_ms());
            if (decision == HostDecision::Detect)
                detector.detect(im, result);
            else {
                result = WinchSnowResult{};
                result.camera_channel = cfg.camera_channel;
            }
            any_error |= result.status;
            std::cout << json(result, decision) << '\n';
            if (!out.empty())
                artifacts(detector, im, result, out + "_" + std::to_string(i), decision);
        }
        camera.close();
        return any_error ? 1 : 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 2;
    }
}

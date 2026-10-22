#include "replay_io.h"
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 4 && argc != 6) {
        std::cerr
            << "usage: ice_replay task.yaml input.ppm output_stem [--sequence sequence.json]\n";
        return 2;
    }
    try {
        using namespace ice_demo;
        WinchSnowConfig cfg;
        if (auto rc = load_config(argv[1], cfg))
            throw std::runtime_error("config error " + std::to_string(rc));
        Detector detector;
        if (auto rc = detector.initialize(cfg))
            throw std::runtime_error("initialize error " + std::to_string(rc));
        std::vector<cv::FileNode> frames;
        cv::FileStorage sequence;
        if (argc == 6) {
            if (std::string(argv[4]) != "--sequence")
                throw std::runtime_error("unknown option");
            sequence.open(argv[5], cv::FileStorage::READ);
            if (!sequence.isOpened() || !sequence["frames"].isSeq())
                throw std::runtime_error("sequence requires frames array");
            for (const auto& f : sequence["frames"])
                frames.push_back(f);
            if (frames.empty())
                throw std::runtime_error("empty sequence");
        } else
            frames.push_back(cv::FileNode{});
        WinchSnowResult result;
        result.camera_channel = cfg.camera_channel;

        uint32_t rc = 0, any_error = 0;
        for (size_t i = 0; i < frames.size(); ++i) {
            const auto& n = frames[i];
            std::string path = argv[2];
            if (!n.empty() && !n["image"].empty()) {
                path = static_cast<std::string>(n["image"]);
                if (std::filesystem::path(path).is_relative())
                    path = (std::filesystem::path(argv[5]).parent_path() / path).string();
            }
            PpmImage image(path);
            auto im = image.frame();
            im.timestamp_ms = fresh_time();
            HostWorkingCondition ctx;
            ctx.valid = true;
            ctx.is_working = 1;
            ctx.timestamp_ms = im.timestamp_ms;
            if (!n.empty()) {
                if (n["angle"].empty() || n["working"].empty())
                    throw std::runtime_error("sequence requires angle and working");
                ctx.boom_angle = static_cast<float>(static_cast<double>(n["angle"]));
                ctx.is_working = static_cast<int32_t>(integer(n["working"], "working"));
                if (!n["valid"].empty())
                    ctx.valid = integer(n["valid"], "valid") == 1;
                if (!n["context_age_ms"].empty())
                    ctx.timestamp_ms -= integer(n["context_age_ms"], "context_age_ms");
                if (!n["image_age_ms"].empty())
                    im.timestamp_ms -= integer(n["image_age_ms"], "image_age_ms");
            }
            const auto decision = host_decision(ctx, im.timestamp_ms, monotonic_timestamp_ms());
            if (decision != HostDecision::Detect) {
                result = WinchSnowResult{};
                result.camera_channel = cfg.camera_channel;
            } else
                rc = detector.detect(im, result);
            any_error |= rc;
            const std::string stem =
                std::string(argv[3]) + (frames.size() > 1 ? "_" + std::to_string(i) : "");
            artifacts(detector, im, result, stem, decision);
            std::cout << json(result, decision) << '\n';
        }
        // Successful processing can legitimately return no detections.
        return any_error ? 1 : 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 2;
    }
}

#include "recording.h"
#include "evs/alg_api.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
using namespace evs::winch_ice;
using Clock = std::chrono::steady_clock;
double elapsed(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
void success(uint32_t code, const char* method) {
    if (code) throw std::runtime_error(std::string(method) + " failed; see result.json and run.log");
}
// Runs synchronously on the library caller's thread. Copy strings if queued.
void log_callback(int level, const char* tag, const char* file, int line, const char* message) {
    std::fprintf(stderr, "[ALOG] level=%d tag=%s file=%s line=%d message=%s\n",
                 level, tag ? tag : "", file ? file : "", line, message ? message : "");
}
}

int main(int argc, char** argv) {
    std::string output = demo::default_output();
    bool update_requested = false;
    bool confidence_requested = false;
    float confidence_threshold = 0;
    bool callback_requested = false;
    float area_ratio = 0;
    unsigned repeats = 1;
    try {
        if (argc < 3) {
            std::cerr << "usage: winch_ice_api_demo config.yaml image.ppm [--out NEW_DIRECTORY]"
                         " [--area-ratio 0..1] [--confidence-threshold 0..1] [--repeat 1..10000] [--log-callback]\n";
            return 2;
        }
        for (int i = 3; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--log-callback") { callback_requested = true; continue; }
            if (i + 1 == argc) throw std::runtime_error("missing value: " + option);
            const std::string value = argv[++i];
            if (option == "--out") output = value;
            else if (option == "--area-ratio") {
                size_t used = 0;
                area_ratio = std::stof(value, &used);
                if (used != value.size() || !std::isfinite(area_ratio) || area_ratio < 0 || area_ratio > 1)
                    throw std::runtime_error("area ratio must be in [0,1]");
                update_requested = true;
            } else if (option == "--confidence-threshold") {
                size_t used = 0;
                confidence_threshold = std::stof(value, &used);
                if (used != value.size() || !std::isfinite(confidence_threshold) || confidence_threshold < 0 || confidence_threshold > 1)
                    throw std::runtime_error("confidence threshold must be in [0,1]");
                confidence_requested = true;
            } else if (option == "--repeat") {
                size_t used = 0;
                const auto count = std::stoul(value, &used);
                if (value.empty() || value.front() == '-' || used != value.size() || count < 1 || count > 10000)
                    throw std::runtime_error("repeat must be in [1,10000]");
                repeats = static_cast<unsigned>(count);
            } else throw std::runtime_error("unknown option: " + option);
        }
        demo::Recording record(output, argc, argv);
        std::unique_ptr<Detector> detector;
        int exit_code = 0;
        std::string error;
        try {
            if (callback_requested) {
                const auto start = Clock::now();
                const auto status = alg_init(log_callback);
                record.call("alg_init", "{\"log_cb\":\"log_callback\"}", "null", status, elapsed(start));
                success(status, "alg_init");
            }
            WinchSnowConfig config;
            auto start = Clock::now();
            auto status = load_config(argv[1], config);
            record.call("load_config", "{\"path\":" + demo::json_string(argv[1]) + "}",
                        demo::config_json(config), status, elapsed(start));
            record.save_config(argv[1]);
            success(status, "load_config");
            demo::Image image(argv[2]);
            record.save_input(image);

            detector = std::make_unique<Detector>();
            start = Clock::now();
            status = detector->initialize(config);
            record.call("initialize", demo::config_json(config), "null", status, elapsed(start));
            success(status, "initialize");

            if (update_requested || confidence_requested) {
                auto update = config.detect_config;
                if (update_requested) update.area_ratio = area_ratio;
                if (confidence_requested) update.confidence_threshold = confidence_threshold;
                start = Clock::now();
                status = detector->updateConfig(update);
                record.call("updateConfig", demo::detect_config_json(update), "null", status, elapsed(start));
                success(status, "updateConfig");
                config.detect_config = update;
            }

            for (unsigned i = 0; i < repeats; ++i) {
                const auto input = image.data();
                WinchSnowResult result;
                start = Clock::now();
                status = detector->detect(input, result);
                record.frame(image, input, config, result, status, elapsed(start));
                success(status, "detect");
                std::cout << "frame=" << i + 1 << " result_code=" << int(result.result_code)
                          << " alert_level=" << int(result.alert_level) << " winch_candidates=" << result.snow_detections.size() << " objects=" << result.objects.size() << '\n';
                for (const auto& object : result.objects)
                    std::cout << "  valid=" << object.valid << " box=" << object.box.x << ',' << object.box.y
                              << ',' << object.box.width << ',' << object.box.height
                              << " confidence=" << object.box.confidence << " ice_ratio=" << object.ice_ratio << '\n';
            }
        } catch (const std::exception& failure) {
            exit_code = 1;
            error = failure.what();
            std::cerr << error << '\n';
        }
        if (detector) {
            const auto start = Clock::now();
            detector->release();
            record.released(elapsed(start));
            detector.reset();
        }
        if (callback_requested) {
            const auto start = Clock::now();
            alg_log_set_cb(nullptr);
            record.void_call("alg_log_set_cb", "{\"cb\":null}", elapsed(start));
        }
        record.finish(exit_code, error);
        std::cout << "Run record: " << record.directory() << '\n';
        return exit_code;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

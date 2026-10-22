#include "ice_test_support.h"
#include <evs/log.h>
#include <evs/status.h>
#include <evs/testing/ice_replay.h>
#include <evs/vision_task.h>
#include <evs/winch_ice.h>

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <new>
#include <vector>

namespace {
// 0: normal; 1: fail once; 2: persistent OOM, including result recovery.
std::atomic<int> allocation_failure{0};
std::atomic<int> allocation_countdown{-1};
int passed = 0;
int failed = 0;

void check(bool condition, const char* message) {
    if (condition) {
        ++passed;
    } else {
        ++failed;
        std::cerr << message << '\n';
    }
}
} // namespace

void* operator new(std::size_t size) {
    const int remaining = allocation_countdown.load();
    if (remaining >= 0 && allocation_countdown.fetch_sub(1) == 0) {
        allocation_countdown.store(-1);
        throw std::bad_alloc();
    }
    const int mode = allocation_failure.load();
    if (mode != 0) {
        if (mode == 1) {
            allocation_failure.store(0);
        }
        throw std::bad_alloc();
    }
    if (void* memory = std::malloc(size ? size : 1)) {
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    return ::operator new(size);
}

void operator delete(void* memory) noexcept {
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept {
    std::free(memory);
}

void operator delete[](void* memory) noexcept {
    std::free(memory);
}

void operator delete[](void* memory, std::size_t) noexcept {
    std::free(memory);
}

int main() {
    using namespace evs::winch_ice;
    auto& logger = evs::Logger::instance();
    logger.set_file("");
    logger.set_level(evs::LogLevel::Info);
    logger.set_sink([](evs::LogLevel, const std::string&) {});
    std::vector<uint8_t> pixels(100 * 100 * 3, 230);
    ImageData image;
    image.data = pixels.data();
    image.width = image.height = 100;
    image.size_bytes = pixels.size();
    const std::vector<ice_test::Box> boxes{{0, 0, 0, 100, 100}};

    {
        ice_test::PixelDetector detector;
        WinchSnowConfig config;
        check(detector.initialize(config) == 0, "initialize pixel fixture");
        auto detect = [&](WinchSnowResult& result) {
            return detector.detectWithBoxes(image, boxes, result);
        };
        WinchSnowResult initial;
        check(detect(initial) == 0 && initial.processed &&
                  initial.alert_level == AlertLevel::ALERT_WARN,
              "establish a frame alarm");
        for (int failure : {1, 2}) {
            WinchSnowResult result;
            allocation_failure.store(failure);
            const auto status = detect(result);
            allocation_failure.store(0);
            check(status == evs::with_module(8, uint32_t(evs::EvsStatus::MemoryAllocation)),
                  "report OOM");
            check(result.status == status && !result.processed, "report rejected call");
            check(result.alert_level == AlertLevel::ALERT_NONE && result.objects.empty(),
                  "OOM must not retain a previous alarm");
            std::vector<evs::testing::IceMaskSnapshot> masks;
            check(detector.copyMasks(masks) && masks.empty(), "OOM clears masks");
            WinchSnowResult recovered;
            check(detect(recovered) == 0 && recovered.objects.size() == 1 &&
                      recovered.alert_level == AlertLevel::ALERT_WARN,
                  "recover by processing the image again");
        }
    }
    for (int y = 0; y < 100; ++y)
        for (int x = 0; x < 100; ++x)
            for (int channel = 0; channel < 3; ++channel)
                pixels[(y * 100 + x) * 3 + channel] = x < 70 ? 230 : 60;
    // Plugin policy updates remain transactional under allocation failures.
    int rejected_pixel_updates = 0, applied_pixel_updates = 0;
    for (int fail_at = 0; fail_at < 20; ++fail_at) {
        ice_test::PixelDetector detector;
        WinchSnowConfig config;
        check(detector.initialize(config) == 0, "initialize pixel update sweep");
        auto update = config.detect_config;
        update.area_ratio = .8f;
        allocation_countdown.store(fail_at);
        const auto status = detector.updateConfig(update);
        allocation_countdown.store(-1);
        WinchSnowResult result;
        check(detector.detectWithBoxes(image, boxes, result) == 0 && result.objects.size() == 1,
              "pixel update recovery");
        check(result.alert_level == (status == 0 ? AlertLevel::ALERT_NONE : AlertLevel::ALERT_WARN),
              "pixel update uses the policy reported by its status");
        if (status == 0)
            ++applied_pixel_updates;
        else
            ++rejected_pixel_updates;
    }
    check(rejected_pixel_updates && applied_pixel_updates, "pixel update exercises both outcomes");

    // An update must report its actual policy even if formatting/buffering a
    // success log fails. Sweep both host-only and legacy ring-buffer paths.
    for (int y = 0; y < 100; ++y)
        for (int x = 0; x < 100; ++x)
            for (int channel = 0; channel < 3; ++channel)
                pixels[(y * 100 + x) * 3 + channel] = x < 70 ? 230 : 60;
    int rejected_updates = 0, applied_updates = 0;
    evs::TaskConfig base;
    base.module_id = 4;
    base.video_channel = {0};
    base.ground.enabled = false;
    base.backend = "mock";
    base.Model_path = "allocation_test.mock";
    for (const size_t capacity : {size_t(0), size_t(256)}) {
        logger.set_ring_capacity(capacity);
        for (int fail_at = 0; fail_at < 48; ++fail_at) {
            evs::VisionTask task;
            check(task.initialize(base) == 0, "initialize logger update sweep");
            auto update = base;
            update.confidence_threshold = .8f;
            allocation_countdown.store(fail_at);
            uint32_t status;
            try {
                status = task.updateConfig(update);
            } catch (const std::bad_alloc&) {
                status = evs::with_module(4, uint32_t(evs::EvsStatus::MemoryAllocation));
            }
            allocation_countdown.store(-1);
            const auto snapshot = task.debugSnapshot();
            check(snapshot.find(status == 0 ? "\"confidence_threshold\":0.8"
                                            : "\"confidence_threshold\":0.45") != std::string::npos,
                  "logger failure must not change the reported update outcome");
            if (status == 0)
                ++applied_updates;
            else
                ++rejected_updates;
        }
    }
    check(rejected_updates > 0 && applied_updates > 0, "sweep exercises both update outcomes");

    // A host-owned sink may throw too. Its exception cannot undo a committed update.
    evs::VisionTask task;
    check(task.initialize(base) == 0, "initialize throwing-sink case");
    int host_calls = 0;
    logger.set_sink([&](evs::LogLevel, const std::string&) {
        ++host_calls;
        throw std::bad_alloc();
    });
    auto update = base;
    update.confidence_threshold = .8f;
    check(task.updateConfig(update) == 0 && host_calls > 0,
          "host sink failure does not reject an applied update");
    logger.set_sink([](evs::LogLevel, const std::string&) {});
    check(task.debugSnapshot().find("\"confidence_threshold\":0.8") != std::string::npos,
          "throwing-sink update has the reported new policy");

    const int previous_calls = host_calls;
    logger.set_sink([&](evs::LogLevel, const std::string&) {
        ++host_calls;
        throw 42;
    });
    bool escaped = false;
    try {
        // Direct calls must be safe too, independent of the macro boundary.
        logger.write(evs::LogLevel::Info, __FILE__, __LINE__, "host callback failure");
    } catch (...) {
        escaped = true;
    }
    logger.set_sink([](evs::LogLevel, const std::string&) {});
    check(!escaped && host_calls > previous_calls, "non-memory host exceptions stay in logger");

    std::cout << "ALLOCATION PASS=" << passed << " FAIL=" << failed << '\n';
    return failed ? 1 : 0;
}

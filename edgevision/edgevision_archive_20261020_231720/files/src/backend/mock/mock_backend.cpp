#include "evs/mock_backend.h"

#include <chrono>
#include <thread>

namespace evs {

uint32_t MockBackend::load(const std::string& /*model_path*/, int /*init_timeout_s*/) {
    // Simulated lightweight load; no real model is opened.
    loaded_.store(true);
    return 0u;
}

uint32_t MockBackend::infer(const std::vector<uint8_t>& /*input*/,
                            std::vector<float>& output) {
    if (!loaded_.load()) return static_cast<uint32_t>(EvsStatus::NotInitialized);
    output.clear(); // no detections from the placeholder backend
    return 0u;
}

void MockBackend::release() {
    loaded_.store(false);
}

} // namespace evs

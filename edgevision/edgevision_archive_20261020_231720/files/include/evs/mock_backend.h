#pragma once
#include <atomic>

#include "evs/iinfer_backend.h"

namespace evs {

// Dependency-free backend placeholder for x86 verification and CI.
// It performs no real inference; infer() returns an empty tensor so the
// preprocessing -> infer -> decode -> policy pipeline can be exercised.
class MockBackend : public IInferBackend {
public:
    uint32_t load(const std::string& model_path, int init_timeout_s) override;
    uint32_t infer(const std::vector<uint8_t>& input,
                   std::vector<float>& output) override;
    void release() override;
    bool loaded() const override { return loaded_.load(); }

private:
    std::atomic<bool> loaded_{false};
};

} // namespace evs

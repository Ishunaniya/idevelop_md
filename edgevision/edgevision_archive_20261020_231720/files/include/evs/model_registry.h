#pragma once
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "evs/iinfer_backend.h"

namespace evs {

// Read-only view of one cache entry (used by the debug snapshot).
struct RegistryEntryInfo {
    std::string backend_type;
    std::string model_path;
    int ref = 0;
    bool loaded = false;
};

// Reference-counted model cache. Key = "backend:model_path".
// One NPU context per unique model; shared across plugins that use it.
class ModelRegistry {
public:
    // Acquire (create + load if absent, else bump refcount). Returns nullptr on failure.
    std::shared_ptr<IInferBackend> acquire(const std::string& backend_type,
                                          const std::string& model_path,
                                          int init_timeout_s);

    void release(const std::string& backend_type, const std::string& model_path);
    int refcount(const std::string& backend_type, const std::string& model_path) const;

    // All live entries, for the debug snapshot.
    std::vector<RegistryEntryInfo> snapshot() const;

private:
    std::string key_of(const std::string& t, const std::string& m) const {
        return t + ":" + m;
    }

    struct Entry {
        int ref = 0;
        std::shared_ptr<IInferBackend> backend;
    };
    std::unordered_map<std::string, Entry> map_;
    mutable std::mutex mtx_;
};

// Process-wide registry instance.
ModelRegistry& global_registry();

} // namespace evs

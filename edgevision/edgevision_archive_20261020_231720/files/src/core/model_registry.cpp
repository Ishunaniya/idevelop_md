#include "evs/model_registry.h"
#include "evs/backend_factory.h"
#include "evs/log.h"

#include <mutex>
#include <unordered_map>

namespace evs {

std::shared_ptr<IInferBackend> ModelRegistry::acquire(const std::string& backend_type,
                                                      const std::string& model_path,
                                                      int init_timeout_s) {
    std::lock_guard<std::mutex> lk(mtx_);
    std::string key = key_of(backend_type, model_path);
    auto it = map_.find(key);
    if (it != map_.end()) {
        ++it->second.ref;
        EVS_LOGD("reuse %s (ref=%d)", key.c_str(), it->second.ref);
        return it->second.backend;
    }
    auto backend = create_backend(backend_type);
    if (!backend) {
        EVS_LOGE("backend '%s' not compiled into this build", backend_type.c_str());
        return nullptr;                        // backend type not compiled in
    }
    if (backend->load(model_path, init_timeout_s) != 0u) {
        EVS_LOGE("model load failed: %s (%s)", model_path.c_str(), backend_type.c_str());
        return nullptr;
    }
    Entry e{1, backend};
    map_.emplace(key, std::move(e));
    EVS_LOGI("loaded %s (timeout=%ds)", key.c_str(), init_timeout_s);
    return backend;
}

void ModelRegistry::release(const std::string& backend_type,
                            const std::string& model_path) {
    std::lock_guard<std::mutex> lk(mtx_);
    std::string key = key_of(backend_type, model_path);
    auto it = map_.find(key);
    if (it == map_.end()) {
        EVS_LOGW("release of unknown model %s", key.c_str());
        return;
    }
    if (--it->second.ref <= 0) {
        EVS_LOGI("destroying %s (ref=0)", key.c_str());
        map_.erase(it);
    } else {
        EVS_LOGD("release %s (ref=%d)", key.c_str(), it->second.ref);
    }
}

int ModelRegistry::refcount(const std::string& backend_type,
                            const std::string& model_path) const {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = map_.find(key_of(backend_type, model_path));
    return it == map_.end() ? 0 : it->second.ref;
}

std::vector<RegistryEntryInfo> ModelRegistry::snapshot() const {
    std::vector<RegistryEntryInfo> out;
    std::lock_guard<std::mutex> lk(mtx_);
    out.reserve(map_.size());
    for (const auto& kv : map_) {
        const Entry& e = kv.second;
        RegistryEntryInfo info;
        const std::size_t colon = kv.first.find(':');
        if (colon != std::string::npos) {
            info.backend_type = kv.first.substr(0, colon);
            info.model_path = kv.first.substr(colon + 1);
        } else {
            info.backend_type = kv.first;
        }
        info.ref = e.ref;
        info.loaded = e.backend ? e.backend->loaded() : false;
        out.push_back(std::move(info));
    }
    return out;
}

ModelRegistry& global_registry() {
    static ModelRegistry inst;
    return inst;
}

} // namespace evs

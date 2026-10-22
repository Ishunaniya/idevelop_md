#include "evs/ground_mapper.h"

#include <cstdlib>
#include <map>
#include <string>

#include "evs/log.h"
#include "evs_register.h"

namespace evs {
namespace {

std::map<std::string, GroundMapperCreator>& ground_mappers() {
    static std::map<std::string, GroundMapperCreator> table;
    return table;
}

} // namespace

bool register_ground_mapper(const std::string& type, GroundMapperCreator creator) {
    if (type.empty() || !creator) return false;
    auto& table = ground_mappers();
    if (table.find(type) != table.end()) return false;
    table.emplace(type, creator);
    return true;
}

std::unique_ptr<IGroundMapper> create_ground_mapper(const CameraGroundConfig& cfg,
                                                    uint32_t* status) {
    std::string type = cfg.mapper;
    // Debug-only process-wide override. Production selection belongs in YAML.
    if (const char* override_type = std::getenv("EVS_GROUND_MODEL")) {
        if (*override_type) {
            type = override_type;
            EVS_LOGW("EVS_GROUND_MODEL overrides camera %d mapper %s -> %s",
                     cfg.id, cfg.mapper.c_str(), type.c_str());
        }
    }

    const auto it = ground_mappers().find(type);
    if (it == ground_mappers().end()) {
        if (status) *status = static_cast<uint32_t>(EvsStatus::NotSupported);
        return nullptr;
    }
    std::unique_ptr<IGroundMapper> mapper = it->second();

    const uint32_t rc = mapper->configure(cfg);
    if (rc != 0u) {
        if (status) *status = rc;
        return nullptr;
    }
    if (status) *status = 0u;
    return mapper;
}

} // namespace evs

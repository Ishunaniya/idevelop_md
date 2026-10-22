#include "evs/function_plugin.h"

#include "evs_register.h"

#include <map>

namespace evs {
namespace {

std::map<uint16_t, FunctionPluginCreator>& function_plugins() {
    static std::map<uint16_t, FunctionPluginCreator> table;
    return table;
}

} // namespace

bool register_function_plugin(uint16_t module_id, FunctionPluginCreator creator) {
    if (!creator) return false;
    auto& table = function_plugins();
    if (table.find(module_id) != table.end()) return false;
    table.emplace(module_id, creator);
    return true;
}

std::unique_ptr<IFunctionPlugin> create_function_plugin(uint16_t module_id) {
    const auto& table = function_plugins();
    const auto it = table.find(module_id);
    if (it == table.end()) return nullptr;
    return it->second();
}

} // namespace evs

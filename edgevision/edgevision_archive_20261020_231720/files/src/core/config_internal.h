#pragma once
#include "evs/task_config.h"

namespace evs {
namespace detail {
// Internal diagnostic metadata; the public loader keeps bare parser errors.
bool load_task_config_with_module(std::string_view path, TaskConfig& cfg, uint32_t& err,
                                  uint16_t& parsed_module);
} // namespace detail
} // namespace evs

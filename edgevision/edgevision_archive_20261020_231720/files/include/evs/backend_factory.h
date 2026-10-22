#pragma once
#include <memory>
#include <string>

#include "evs/iinfer_backend.h"

namespace evs {

// Create a backend instance for the requested type. Returns nullptr when the
// type is not compiled in (e.g. rknn / qnn on a host without the SDK).
std::shared_ptr<IInferBackend> create_backend(const std::string& type);

} // namespace evs

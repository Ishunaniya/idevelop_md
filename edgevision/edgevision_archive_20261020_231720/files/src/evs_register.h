#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "evs/detection_decoder.h"
#include "evs/function_plugin.h"
#include "evs/ground_mapper.h"

namespace evs {

// Creators are registered from each algorithm's own .cpp during static
// initialization. create_* only looks the name up. A new function plugin is
// its own shared library linked to libevs_core.so; loading that library runs the
// registrar. Programs that link the static archives must take the whole
// archive so the registrar object is not dropped.
using FunctionPluginCreator = std::unique_ptr<IFunctionPlugin> (*)();
using DecoderCreator = std::unique_ptr<IDetectionDecoder> (*)(int person_class_id,
                                                              int num_classes,
                                                              int model_in_w,
                                                              int model_in_h);
using GroundMapperCreator = std::unique_ptr<IGroundMapper> (*)();

bool register_function_plugin(uint16_t module_id, FunctionPluginCreator creator);
bool register_decoder(const std::string& format, DecoderCreator creator);
bool register_ground_mapper(const std::string& type, GroundMapperCreator creator);

struct FunctionPluginRegistration {
    FunctionPluginRegistration(uint16_t module_id, FunctionPluginCreator creator) {
        register_function_plugin(module_id, creator);
    }
};

struct DecoderRegistration {
    DecoderRegistration(const char* format, DecoderCreator creator) {
        register_decoder(format, creator);
    }
};

struct GroundMapperRegistration {
    GroundMapperRegistration(const char* type, GroundMapperCreator creator) {
        register_ground_mapper(type, creator);
    }
};

} // namespace evs

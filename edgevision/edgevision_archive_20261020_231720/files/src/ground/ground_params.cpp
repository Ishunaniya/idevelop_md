#include "evs/ground_params.h"

#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace evs {
namespace {

std::string trim(const std::string& value) {
    std::size_t first = 0;
    std::size_t last = value.size();
    while (first < last &&
           std::isspace(static_cast<unsigned char>(value[first]))) {
        ++first;
    }
    while (last > first &&
           std::isspace(static_cast<unsigned char>(value[last - 1]))) {
        --last;
    }
    return value.substr(first, last - first);
}

} // namespace

bool has_ground_param(const CameraGroundConfig& cfg, const std::string& key) {
    return cfg.params.find(key) != cfg.params.end();
}

std::string ground_param_string(const CameraGroundConfig& cfg,
                                const std::string& key,
                                const std::string& fallback) {
    const auto it = cfg.params.find(key);
    return it == cfg.params.end() ? fallback : it->second;
}

int ground_param_int(const CameraGroundConfig& cfg, const std::string& key,
                     int fallback) {
    const auto it = cfg.params.find(key);
    if (it == cfg.params.end()) return fallback;
    const char* begin = it->second.c_str();
    char* end = nullptr;
    errno = 0;
    const long value = std::strtol(begin, &end, 10);
    return errno == 0 && end != begin && trim(end).empty()
               ? static_cast<int>(value)
               : fallback;
}

float ground_param_float(const CameraGroundConfig& cfg, const std::string& key,
                         float fallback) {
    const auto it = cfg.params.find(key);
    if (it == cfg.params.end()) return fallback;
    const char* begin = it->second.c_str();
    char* end = nullptr;
    errno = 0;
    const float value = std::strtof(begin, &end);
    return errno == 0 && end != begin && trim(end).empty() ? value : fallback;
}

bool ground_param_bool(const CameraGroundConfig& cfg, const std::string& key,
                       bool fallback) {
    const std::string value = ground_param_string(cfg, key);
    if (value == "true" || value == "True" || value == "1") return true;
    if (value == "false" || value == "False" || value == "0") return false;
    return fallback;
}

std::vector<float> ground_param_float_array(const CameraGroundConfig& cfg,
                                            const std::string& key) {
    const auto it = cfg.params.find(key);
    if (it == cfg.params.end()) return {};
    std::string value = trim(it->second);
    if (!value.empty() && value.front() == '[') value.erase(value.begin());
    if (!value.empty() && value.back() == ']') value.pop_back();

    std::vector<float> result;
    std::istringstream input(value);
    std::string token;
    while (std::getline(input, token, ',')) {
        token = trim(token);
        if (token.empty()) return {};
        char* end = nullptr;
        errno = 0;
        const float parsed = std::strtof(token.c_str(), &end);
        if (errno != 0 || end == token.c_str() || !trim(end).empty()) return {};
        result.push_back(parsed);
    }
    return result;
}

} // namespace evs

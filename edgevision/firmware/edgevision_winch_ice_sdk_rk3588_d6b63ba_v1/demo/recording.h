#pragma once
#include <evs/winch_ice.h>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace demo {
struct Image {
    uint32_t width = 0, height = 0;
    std::vector<uint8_t> rgb;
    explicit Image(const std::filesystem::path& path);
    evs::winch_ice::ImageData data() const;
};

std::string json_string(const std::string& value);
std::string config_json(const evs::winch_ice::WinchSnowConfig& config);
std::string detect_config_json(const evs::winch_ice::DetectConfig& config);
std::string result_json(const evs::winch_ice::WinchSnowResult& result);
std::string default_output();

// Demo output only. The algorithm receives the unmodified RGB image.
class Recording {
public:
    Recording(const std::filesystem::path& output, int argc, char** argv);
    ~Recording();
    void save_config(const std::filesystem::path& source);
    void save_input(const Image& image);
    void call(const std::string& method, const std::string& input,
              const std::string& output, uint32_t status, double milliseconds);
    void released(double milliseconds);
    void void_call(const std::string& method, const std::string& input, double milliseconds);
    void frame(const Image& image, const evs::winch_ice::ImageData& input,
               const evs::winch_ice::WinchSnowConfig& config,
               const evs::winch_ice::WinchSnowResult& result,
               uint32_t status, double milliseconds);
    void finish(int exit_code, const std::string& error);
    const std::filesystem::path& directory() const { return directory_; }
private:
    struct Capture;
    std::filesystem::path directory_;
    std::unique_ptr<Capture> capture_;
    std::vector<std::string> calls_, frames_;
    std::string started_, command_;
};
} // namespace demo

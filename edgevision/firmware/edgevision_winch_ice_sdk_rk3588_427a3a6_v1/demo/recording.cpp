#include "recording.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unistd.h>

namespace demo {
namespace {
using namespace evs::winch_ice;
std::string utc() {
    const auto now = std::chrono::system_clock::now();
    const auto seconds = std::chrono::system_clock::to_time_t(now);
    std::tm time{};
    gmtime_r(&seconds, &time);
    std::ostringstream text;
    text << std::put_time(&time, "%Y-%m-%dT%H:%M:%SZ");
    return text.str();
}
template<class T> std::string numeric(T value) {
    if (!std::isfinite(double(value))) return "null";
    std::ostringstream text;
    text << std::setprecision(std::numeric_limits<T>::max_digits10) << value;
    return text.str();
}
void write_text(const std::filesystem::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary);
    output << text;
    if (!output) throw std::runtime_error("cannot write " + path.string());
}
std::string array(const std::vector<std::string>& values) {
    std::string text = "[";
    for (const auto& value : values) { if (text.size() > 1) text += ','; text += value; }
    return text + ']';
}
std::string box_json(const Detection& box) {
    return "{\"class_id\":" + std::to_string(box.class_id) + ",\"confidence\":" + numeric(box.confidence) +
        ",\"x\":" + std::to_string(box.x) + ",\"y\":" + std::to_string(box.y) +
        ",\"width\":" + std::to_string(box.width) + ",\"height\":" + std::to_string(box.height) + '}';
}
std::string hex(uint32_t code) {
    std::ostringstream text;
    text << "0x" << std::hex << std::setw(8) << std::setfill('0') << code;
    return text.str();
}
std::string shell_quote(const std::string& value) {
    std::string text="'";
    for(char c:value) text+=c=='\''?"'\\''":std::string(1,c);
    return text+"'";
}
std::string ppm_token(std::istream& input) {
    std::string text;
    while (input >> text) {
        if (text.front() != '#') return text;
        input.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
    throw std::runtime_error("truncated PPM header");
}
uint32_t unsigned_number(const std::string& text) {
    size_t used = 0;
    const auto value = std::stoul(text, &used);
    if (text.empty() || text.front() == '-' || used != text.size() || value > UINT32_MAX)
        throw std::runtime_error("invalid PPM dimension");
    return static_cast<uint32_t>(value);
}

// RGB PNG with uncompressed DEFLATE blocks. No image library is needed by the demo.
void be32(std::vector<uint8_t>& bytes, uint32_t value) {
    for (int shift : {24, 16, 8, 0}) bytes.push_back(uint8_t(value >> shift));
}
void chunk(std::vector<uint8_t>& png, const char* name, const std::vector<uint8_t>& data) {
    be32(png, uint32_t(data.size()));
    const auto begin = png.size();
    png.insert(png.end(), name, name + 4);
    png.insert(png.end(), data.begin(), data.end());
    uint32_t crc = UINT32_MAX;
    for (size_t i = begin; i < png.size(); ++i) {
        crc ^= png[i];
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    be32(png, ~crc);
}
void png(const std::filesystem::path& path, const Image& image) {
    std::vector<uint8_t> raw;
    raw.reserve(image.rgb.size() + image.height);
    const size_t row = size_t(image.width) * 3;
    for (uint32_t y = 0; y < image.height; ++y) {
        raw.push_back(0); // PNG row filter: none.
        raw.insert(raw.end(), image.rgb.begin() + y * row, image.rgb.begin() + (y + 1) * row);
    }
    std::vector<uint8_t> compressed{0x78, 0x01};
    for (size_t offset = 0; offset < raw.size();) {
        const auto length = uint16_t(std::min<size_t>(65535, raw.size() - offset));
        compressed.push_back(offset + length == raw.size() ? 1 : 0);
        compressed.push_back(uint8_t(length)); compressed.push_back(uint8_t(length >> 8));
        const auto inverse = uint16_t(~length);
        compressed.push_back(uint8_t(inverse)); compressed.push_back(uint8_t(inverse >> 8));
        compressed.insert(compressed.end(), raw.begin() + offset, raw.begin() + offset + length);
        offset += length;
    }
    uint32_t a = 1, b = 0;
    for (auto byte : raw) { a = (a + byte) % 65521; b = (b + a) % 65521; }
    be32(compressed, (b << 16) | a);
    std::vector<uint8_t> bytes{137, 80, 78, 71, 13, 10, 26, 10}, header;
    be32(header, image.width); be32(header, image.height);
    header.insert(header.end(), {8, 2, 0, 0, 0});
    chunk(bytes, "IHDR", header); chunk(bytes, "IDAT", compressed); chunk(bytes, "IEND", {});
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    if (!output) throw std::runtime_error("cannot write " + path.string());
}
using Color = std::array<uint8_t, 3>;
void pixel(Image& image, int x, int y, Color color) {
    if (x < 0 || y < 0 || x >= int(image.width) || y >= int(image.height)) return;
    std::copy(color.begin(), color.end(), image.rgb.begin() + (size_t(y) * image.width + x) * 3);
}
// Five columns per character, seven bits per column (a small ASCII overlay font).
std::array<uint8_t, 5> glyph(char c) {
    static const uint8_t digits[10][5] = {
        {62,81,73,69,62},{0,66,127,64,0},{66,97,81,73,70},{33,65,69,75,49},{24,20,18,127,16},
        {39,69,69,69,57},{60,74,73,73,48},{1,113,9,5,3},{54,73,73,73,54},{6,73,73,41,30}};
    if (c >= '0' && c <= '9') { std::array<uint8_t,5> out; std::copy(digits[c-'0'],digits[c-'0']+5,out.begin()); return out; }
    switch (c) {
    case '.': return {0,96,96,0,0}; case ':': return {0,54,54,0,0};
    case '%': return {99,19,8,100,99}; case '-': return {8,8,8,8,8}; case '/': return {32,16,8,4,2};
    case 'A': return {126,9,9,9,126}; case 'C': return {62,65,65,65,34};
    case 'E': return {127,73,73,73,65}; case 'F': return {127,9,9,9,1};
    case 'I': return {0,65,127,65,0}; case 'L': return {127,64,64,64,64};
    case 'N': return {127,2,4,8,127}; case 'O': return {62,65,65,65,62};
    case 'R': return {127,9,25,41,70}; case 'S': return {38,73,73,73,50};
    case 'T': return {1,1,127,1,1}; case 'U': return {63,64,64,64,63};
    default: return {0,0,0,0,0};
    }
}
void text(Image& image, int x, int y, const std::string& label, Color color) {
    for (char c : label) {
        const auto bits = glyph(c);
        for (int dx = 0; dx < 12; ++dx)
            for (int dy = 0; dy < 18; ++dy) pixel(image, x + dx, y + dy, {16,16,16});
        for (int column = 0; column < 5; ++column)
            for (int row = 0; row < 7; ++row)
                if (bits[column] & (1 << row))
                    for (int xx = 0; xx < 2; ++xx) for (int yy = 0; yy < 2; ++yy)
                        pixel(image, x + 1 + column * 2 + xx, y + 2 + row * 2 + yy, color);
        x += 12;
    }
}
void annotate(Image& image, const WinchSnowResult& result, uint32_t status, float threshold) {
    const Color red{255,64,64}, green{40,220,100}, amber{255,195,40};
    text(image, 6, 6, status ? "ERROR " + hex(status) :
        result.result_code == evs::ResultCode::RESULT_NO_RESULT ? "NO RESULT" :
        std::none_of(result.objects.begin(), result.objects.end(), [](const auto& o) { return o.valid; }) ? "N/A" :
        result.alert_level == evs::AlertLevel::ALERT_WARN ? "ALERT" : "CLEAR", status ? red : amber);
    if (status) return;
    for (size_t i = 0; i < result.objects.size(); ++i) {
        const auto& object = result.objects[i];
        const auto& box = object.box;
        if (!box.width || !box.height) continue;
        const int x = std::clamp<int>(box.x,0,image.width-1), y = std::clamp<int>(box.y,0,image.height-1);
        const int right = int(std::min<uint64_t>(uint64_t(box.x)+box.width,image.width))-1;
        const int bottom = int(std::min<uint64_t>(uint64_t(box.y)+box.height,image.height))-1;
        const Color color = !object.valid ? amber : object.ice_ratio >= threshold ? red : green;
        for (int line = 0; line < 2; ++line) {
            for (int xx = x; xx <= right; ++xx) { pixel(image,xx,y+line,color); pixel(image,xx,bottom-line,color); }
            for (int yy = y; yy <= bottom; ++yy) { pixel(image,x+line,yy,color); pixel(image,right-line,yy,color); }
        }
        std::ostringstream label;
        label << i << ':';
        if (object.valid) label << std::fixed << std::setprecision(2) << object.ice_ratio * 100 << '%';
        else label << "N/A";
        label << " C:" << std::fixed << std::setprecision(3) << box.confidence;
        text(image,x,std::max(28,y-20),label.str(),color);
    }
}
}

Image::Image(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input || ppm_token(input) != "P6") throw std::runtime_error("requires P6 RGB8 PPM: " + path.string());
    width = unsigned_number(ppm_token(input)); height = unsigned_number(ppm_token(input));
    if (unsigned_number(ppm_token(input)) != 255 || !width || !height || uint64_t(width)*height > 100000000)
        throw std::runtime_error("invalid PPM size or maximum value");
    char separator = 0; input.get(separator);
    if (!std::isspace(static_cast<unsigned char>(separator))) throw std::runtime_error("missing PPM pixel separator");
    if (separator == '\r' && input.peek() == '\n') input.get();
    rgb.resize(size_t(width)*height*3);
    if (!input.read(reinterpret_cast<char*>(rgb.data()),rgb.size())) throw std::runtime_error("truncated PPM pixels");
}
ImageData Image::data() const {
    ImageData image;
    image.data=rgb.data(); image.width=width; image.height=height; image.channels=3;
    image.timestamp_ms=uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
    return image;
}
std::string json_string(const std::string& value) {
    std::ostringstream text; text << '"';
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') text << '\\' << char(c);
        else if (c < 32) text << "\\u" << std::hex << std::setw(4) << std::setfill('0') << unsigned(c) << std::dec;
        else text << char(c);
    }
    text << '"'; return text.str();
}
std::string detect_config_json(const DetectConfig& c) {
    return "{\"confidence_threshold\":"+numeric(c.confidence_threshold)+",\"nms_threshold\":"+numeric(c.nms_threshold)+",\"area_ratio\":"+numeric(c.area_ratio)+'}';
}
std::string config_json(const WinchSnowConfig& c) {
    return "{\"model_config\":{\"Model_path\":"+json_string(c.model_config.Model_path)+",\"input_width\":"+std::to_string(c.model_config.input_width)+
        ",\"input_height\":"+std::to_string(c.model_config.input_height)+",\"warmup\":"+(c.model_config.warmup?"true":"false")+
        "},\"detect_config\":"+detect_config_json(c.detect_config)+",\"camera_channel\":"+std::to_string(c.camera_channel)+'}';
}
std::string result_json(const WinchSnowResult& r) {
    std::vector<std::string> snow, objects;
    for (const auto& d : r.snow_detections) snow.push_back(box_json(d));
    for (const auto& o : r.objects) objects.push_back("{\"box\":"+box_json(o.box)+",\"ice_ratio\":"+numeric(o.ice_ratio)+",\"valid\":"+(o.valid?"true":"false")+'}');
    return "{\"result_code\":"+std::to_string(int(r.result_code))+",\"alert_level\":"+std::to_string(int(r.alert_level))+
        ",\"snow_detections\":"+array(snow)+",\"inference_time_ms\":"+std::to_string(r.inference_time_ms)+
        ",\"camera_channel\":"+std::to_string(r.camera_channel)+",\"objects\":"+array(objects)+'}';
}
std::string default_output() {
    auto name = utc(); std::replace(name.begin(),name.end(),':','-');
    return "records/run_"+name+'_'+std::to_string(getpid());
}
struct Recording::Capture {
    int saved_out=-1,saved_err=-1;
    explicit Capture(const std::filesystem::path& path) {
        std::cout.flush(); std::cerr.flush(); std::fflush(nullptr);
        const int log=open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL,0644);
        if (log<0) throw std::runtime_error("cannot create run.log");
        saved_out=dup(STDOUT_FILENO); saved_err=dup(STDERR_FILENO);
        if (saved_out<0 || saved_err<0) {
            close(log); if(saved_out>=0)close(saved_out); if(saved_err>=0)close(saved_err);
            throw std::runtime_error("cannot capture console");
        }
        if (dup2(log,STDOUT_FILENO)<0 || dup2(log,STDERR_FILENO)<0) {
            dup2(saved_out,STDOUT_FILENO); dup2(saved_err,STDERR_FILENO);
            close(saved_out);close(saved_err);close(log);throw std::runtime_error("cannot redirect console");
        }
        close(log);
    }
    ~Capture() {
        std::cout.flush();std::cerr.flush();std::fflush(nullptr);
        dup2(saved_out,STDOUT_FILENO);dup2(saved_err,STDERR_FILENO);close(saved_out);close(saved_err);
    }
};
Recording::Recording(const std::filesystem::path& output,int argc,char** argv) : directory_(std::filesystem::absolute(output)),started_(utc()) {
    if (!directory_.parent_path().empty()) std::filesystem::create_directories(directory_.parent_path());
    if (!std::filesystem::create_directory(directory_)) throw std::runtime_error("output directory already exists: "+directory_.string());
    std::vector<std::string> arguments;
    std::string shell_command;
    for(int i=0;i<argc;++i) {
        arguments.push_back(json_string(argv[i]));
        shell_command+=shell_quote(argv[i])+" ";
    }
    command_=array(arguments);
    write_text(directory_/"command.txt","cd "+shell_quote(std::filesystem::current_path().string())+"\n"+shell_command+"\n");
    std::error_code ignored;
    const auto executable=std::filesystem::read_symlink("/proc/self/exe",ignored);
    const auto version=executable.parent_path().parent_path()/"VERSION.json";
    if(!ignored && std::filesystem::exists(version)) std::filesystem::copy_file(version,directory_/"sdk_version.json");
    capture_=std::make_unique<Capture>(directory_/"run.log");
    std::cout << "Temporary contract: result_code=business winch detection; snow_detections=winch candidates before confidence/NMS; no snow model.\n";
    std::cout << "Started " << started_ << '\n';
}
Recording::~Recording() = default;
void Recording::save_config(const std::filesystem::path& source) {
    if(std::filesystem::is_regular_file(source)) std::filesystem::copy_file(source,directory_/"config.yaml");
}
void Recording::save_input(const Image& image) { png(directory_/"input.png",image); }
void Recording::call(const std::string& method,const std::string& input,const std::string& output,uint32_t status,double milliseconds) {
    calls_.push_back("{\"method\":"+json_string(method)+",\"time_utc\":"+json_string(utc())+",\"input\":"+input+
        ",\"output\":"+output+",\"return_code\":"+std::to_string(status)+",\"return_hex\":"+json_string(hex(status))+",\"elapsed_ms\":"+numeric(milliseconds)+'}');
    std::cout << method << " return=" << hex(status) << " input=" << input << " output=" << output << std::endl;
}
void Recording::released(double milliseconds) {
    void_call("release", "null", milliseconds);
}
void Recording::void_call(const std::string& method, const std::string& input, double milliseconds) {
    calls_.push_back("{\"method\":"+json_string(method)+",\"time_utc\":"+json_string(utc())+",\"input\":"+input+",\"output\":null,\"return_type\":\"void\",\"completed\":true,\"elapsed_ms\":"+numeric(milliseconds)+'}');
    std::cout << method << " completed (void) input=" << input << std::endl;
}
void Recording::frame(const Image& image,const ImageData& input,const WinchSnowConfig& config,const WinchSnowResult& result,uint32_t status,double milliseconds) {
    std::ostringstream stem;stem << "frame_" << std::setw(6) << std::setfill('0') << frames_.size()+1;
    const auto id=stem.str();
    const std::string params="{\"data\":\"input.png\",\"width\":"+std::to_string(input.width)+",\"height\":"+std::to_string(input.height)+
        ",\"channels\":"+std::to_string(input.channels)+",\"timestamp_ms\":"+std::to_string(input.timestamp_ms)+'}';
    call("detect",params,result_json(result),status,milliseconds);
    Image drawn=image;annotate(drawn,result,status,config.detect_config.area_ratio);
    png(directory_/(id+".png"),drawn);
    if(frames_.empty()) std::filesystem::copy_file(directory_/(id+".png"),directory_/"annotated.png");
    const auto detail="{\"detections_target\":\"winch\",\"detections_stage\":\"pre_confidence_pre_nms\",\"frame_id\":"+json_string(id)+",\"input\":"+params+",\"config\":"+config_json(config)+
        ",\"return_code\":"+std::to_string(status)+",\"elapsed_ms\":"+numeric(milliseconds)+",\"result\":"+result_json(result)+'}';
    write_text(directory_/(id+".json"),detail+"\n");
    frames_.push_back("{\"frame_id\":"+json_string(id)+",\"parameters\":"+json_string(id+".json")+",\"image\":"+json_string(id+".png")+'}');
}
void Recording::finish(int exit_code,const std::string& error) {
    std::cout << "Finished " << utc() << " exit_code=" << exit_code << std::endl;
    write_text(directory_/"result.json","{\"schema_version\":1,\"detections_target\":\"winch\",\"detections_stage\":\"pre_confidence_pre_nms\",\"started_utc\":"+json_string(started_)+",\"finished_utc\":"+json_string(utc())+
        ",\"command\":"+command_+",\"exit_code\":"+std::to_string(exit_code)+",\"error\":"+json_string(error)+",\"calls\":"+array(calls_)+",\"frames\":"+array(frames_)+"}\n");
    capture_.reset();
}
} // namespace demo

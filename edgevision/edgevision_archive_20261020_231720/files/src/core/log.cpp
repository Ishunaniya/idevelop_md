#include "evs/log.h"

#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>

namespace evs {
namespace {

// Basename of __FILE__: full paths waste half of a log line.
const char* base_name(const char* f) {
    if (!f) return "";
    const char* slash = std::strrchr(f, '/');
#if defined(_WIN32)
    const char* bs = std::strrchr(f, '\\');
    if (!slash || (bs && bs > slash)) slash = bs;
#endif
    return slash ? slash + 1 : f;
}

void stamp_now(char* buf, std::size_t cap) {
    const int64_t ms = static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    const std::time_t sec = static_cast<std::time_t>(ms / 1000);
    std::tm tm_s{};
#if defined(_WIN32)
    localtime_s(&tm_s, &sec);
#else
    localtime_r(&sec, &tm_s);
#endif
    std::snprintf(buf, cap, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                  tm_s.tm_year + 1900, tm_s.tm_mon + 1, tm_s.tm_mday,
                  tm_s.tm_hour, tm_s.tm_min, tm_s.tm_sec,
                  static_cast<int>(ms % 1000));
}

} // anonymous namespace

const char* log_level_name(LogLevel l) {
    switch (l) {
        case LogLevel::Off:   return "OFF";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Trace: return "TRACE";
    }
    return "?";
}

LogLevel log_level_from_string(const std::string& s, LogLevel def) {
    std::string v;
    v.reserve(s.size());
    for (char c : s) v.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    v.erase(v.find_last_not_of(" \t\r\n") + 1);
    const std::size_t first = v.find_first_not_of(" \t\r\n");
    if (first != std::string::npos) v.erase(0, first);
    if (v.empty()) return def;
    if (v == "off" || v == "0")   return LogLevel::Off;
    if (v == "error" || v == "1") return LogLevel::Error;
    if (v == "warn" || v == "warning" || v == "2") return LogLevel::Warn;
    if (v == "info" || v == "3")  return LogLevel::Info;
    if (v == "debug" || v == "4") return LogLevel::Debug;
    if (v == "trace" || v == "5") return LogLevel::Trace;
    return def;
}

Logger::Logger()
    : level_(LogLevel::Info), file_(nullptr), ring_cap_(256), ring_pos_(0) {
    // Environment overrides keep a deployed image debuggable without rebuild.
    if (const char* env = std::getenv("EVS_LOG_LEVEL")) {
        level_ = log_level_from_string(env, level_);
    }
    if (const char* p = std::getenv("EVS_LOG_FILE")) {
        set_file(p);
    }
}

Logger::~Logger() {
    if (file_) {
        std::fclose(static_cast<FILE*>(file_));
        file_ = nullptr;
    }
}

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::set_level(LogLevel l) {
    std::lock_guard<std::mutex> lk(mtx_);
    level_ = l;
}

LogLevel Logger::level() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return level_;
}

void Logger::set_sink(LogSink sink) {
    std::lock_guard<std::mutex> lk(mtx_);
    sink_ = std::move(sink);
}

void Logger::reset_sink() {
    std::lock_guard<std::mutex> lk(mtx_);
    sink_ = nullptr;
}

bool Logger::set_file(const std::string& path) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (file_) {
        std::fclose(static_cast<FILE*>(file_));
        file_ = nullptr;
        file_path_.clear();
    }
    if (path.empty()) return true;
    FILE* fp = std::fopen(path.c_str(), "a");
    if (!fp) return false;
    file_ = fp;
    file_path_ = path;
    return true;
}

std::string Logger::file() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return file_path_;
}

void Logger::set_ring_capacity(std::size_t n) {
    std::lock_guard<std::mutex> lk(mtx_);
    ring_cap_ = n;
    if (ring_.size() > ring_cap_) ring_.resize(ring_cap_);
    if (ring_cap_ == 0) ring_.clear();
    ring_pos_ = 0;
}

std::size_t Logger::ring_capacity() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return ring_cap_;
}

std::vector<std::string> Logger::tail(std::size_t n) const {
    std::lock_guard<std::mutex> lk(mtx_);
    if (n == 0 || ring_.empty()) return {};
    const std::size_t take = n < ring_.size() ? n : ring_.size();
    return std::vector<std::string>(ring_.end() - static_cast<std::ptrdiff_t>(take),
                                    ring_.end());
}

void Logger::clear_ring() {
    std::lock_guard<std::mutex> lk(mtx_);
    ring_.clear();
    ring_pos_ = 0;
}

void Logger::write(LogLevel level, const char* file, int line, const char* fmt, ...) {
    std::va_list ap;
    va_start(ap, fmt);
    writev(level, file, line, fmt, ap);
    va_end(ap);
}

void Logger::writev(LogLevel level, const char* file, int line, const char* fmt, std::va_list ap) {
    try {
        if (level == LogLevel::Off || !fmt)
            return;

        LogLevel active{};
        LogSink sink;
        FILE* fp = nullptr;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            active = level_;
            if (static_cast<int>(level) > static_cast<int>(active))
                return;
            sink = sink_;
            fp = static_cast<FILE*>(file_);
        }

        char stamp[32]{};
        stamp_now(stamp, sizeof(stamp));

        char body[1024];
        std::va_list ap2;
        va_copy(ap2, ap);
        const int n = std::vsnprintf(body, sizeof(body), fmt, ap2);
        va_end(ap2);
        if (n < 0)
            return;

        const bool truncated = static_cast<std::size_t>(n) >= sizeof(body);
        char head[192];
        std::snprintf(head, sizeof(head), "%s [%s] %s:%d ", stamp, log_level_name(level),
                      base_name(file), line);

        // Named `text` on purpose: `line` is the source-line parameter and would
        // otherwise be shadowed (GCC/Clang reject shadowing a parameter outright).
        std::string text;
        text.reserve(std::strlen(head) + sizeof(body) + 3);
        text += head;
        text += body;
        if (truncated)
            text += "...(truncated)";
        text += '\n';

        // Ring buffer (bounded memory on a headless device).
        {
            std::lock_guard<std::mutex> lk(mtx_);
            if (ring_cap_ > 0) {
                ring_.push_back(text);
                if (ring_.size() > ring_cap_)
                    ring_.erase(ring_.begin());
            }
            ++ring_pos_; // frames counter for future diagnostics
        }

        if (fp) {
            std::fwrite(text.data(), 1, text.size(), fp);
            std::fflush(fp);
        }
        // Sinks run outside the lock so a host callback may query the logger
        // (e.g. tail()) without deadlocking.
        if (sink) {
            sink(level, text);
        } else if (!fp) {
            std::fwrite(text.data(), 1, text.size(), stderr);
            std::fflush(stderr);
        }
    } catch (...) {
        // Logging is auxiliary: formatting, buffering or host callbacks must not
        // turn an already committed algorithm operation into a reported failure.
        // Do not log from this handler: that could allocate or recurse again.
    }
}

} // namespace evs

#pragma once
#include <cstdarg>
#include <cstddef>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace evs {

// Verbosity ladder. Level grows with chattiness; a record is emitted when its
// level <= the active level. Off silences everything.
enum class LogLevel : int {
    Off = 0,
    Error = 1,
    Warn = 2,
    Info = 3,
    Debug = 4,
    Trace = 5,
};

const char* log_level_name(LogLevel l);

// Accepts "error"/"warn"/"info"/"debug"/"trace" (case-insensitive) or a number 0..5.
LogLevel log_level_from_string(const std::string& s, LogLevel def = LogLevel::Info);

// A sink receives the fully formatted line (timestamp + level + source).
// It MUST NOT call back into Logger from inside itself in a way that would
// require the same lock twice in one thread (Logger releases the lock before
// invoking sinks, so logger queries are safe).
using LogSink = std::function<void(LogLevel level, const std::string& line)>;

// Process-wide logger. Writing a record contains formatting/buffering/sink
// exceptions. Configuration/query methods may allocate and are not no-throw.
class Logger {
public:
    static Logger& instance();

    void set_level(LogLevel l);
    LogLevel level() const;

    // Install a host sink (syslog, Qt logging, IPC bridge...). Empty sink ->
    // default stderr output.
    //
    // The sink is stored globally: if it captures objects by reference, the
    // host MUST call set_sink()/reset_sink() before those objects die, or every
    // later log line will be delivered to a dangling reference.
    void set_sink(LogSink sink);
    void reset_sink();

    // Optional file sink appended in addition to whatever set_sink installed.
    // Empty path disables it. Returns false when the file cannot be opened.
    bool set_file(const std::string& path);
    std::string file() const;

    // Ring buffer of formatted lines. Used by the debug snapshot so an operator
    // can pull the last N lines over IPC without waiting for reproduction.
    void set_ring_capacity(std::size_t n);
    std::size_t ring_capacity() const;
    std::vector<std::string> tail(std::size_t n) const;
    void clear_ring();

    void write(LogLevel level, const char* file, int line, const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
        __attribute__((format(printf, 5, 6)))
#endif
        ;

    void writev(LogLevel level, const char* file, int line, const char* fmt,
                std::va_list ap);

private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    mutable std::mutex mtx_;
    LogLevel level_;
    LogSink sink_;
    // FILE* is kept as void* so this header stays free of <cstdio> churn;
    // only the implementation touches it.
    void* file_;
    std::string file_path_;
    std::vector<std::string> ring_;
    std::size_t ring_cap_;
    std::size_t ring_pos_;
};

} // namespace evs

// Compile-time floor. Build with -DEVS_LOG_LEVEL=3 to strip trace/debug
// strings and their formatting cost out of a release image.
#if !defined(EVS_LOG_LEVEL)
#  if defined(NDEBUG)
#    define EVS_LOG_LEVEL 3 // Info
#  else
#    define EVS_LOG_LEVEL 5 // Trace
#  endif
#endif

#define EVS_LOG_AT(level, ...)                                                                     \
    do {                                                                                           \
        if (static_cast<int>(level) <= (EVS_LOG_LEVEL)) {                                          \
            try {                                                                                  \
                ::evs::Logger::instance().write(level, __FILE__, __LINE__, __VA_ARGS__);           \
            } catch (...) {                                                                        \
                /* Also isolate first-use logger construction/argument errors. */                  \
            }                                                                                      \
        }                                                                                          \
    } while (0)

#define EVS_LOGE(...) EVS_LOG_AT(::evs::LogLevel::Error, __VA_ARGS__)
#define EVS_LOGW(...) EVS_LOG_AT(::evs::LogLevel::Warn, __VA_ARGS__)
#define EVS_LOGI(...) EVS_LOG_AT(::evs::LogLevel::Info, __VA_ARGS__)
#define EVS_LOGD(...) EVS_LOG_AT(::evs::LogLevel::Debug, __VA_ARGS__)
#define EVS_LOGT(...) EVS_LOG_AT(::evs::LogLevel::Trace, __VA_ARGS__)

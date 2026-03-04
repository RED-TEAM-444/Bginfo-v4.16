#pragma once
// ============================================================
//  logger.hpp  –  Thread-safe, levelled logger
// ============================================================
#include <string>
#include <fstream>
#include <mutex>
#include <sstream>
#include <memory>
#include <functional>

namespace nra {

enum class LogLevel : int { DEBUG = 0, INFO = 1, WARN = 2, ERR = 3, NONE = 4 };

class Logger {
public:
    // Singleton access
    static Logger& instance();

    // Configuration
    void setConsoleLevel(LogLevel lvl);
    void setFileLevel(LogLevel lvl);
    bool openLogFile(const std::string& path);
    void enableColour(bool on);

    // Logging methods
    void log(LogLevel lvl, const std::string& msg,
             const char* file = nullptr, int line = 0);

    void debug(const std::string& msg,
               const char* file = nullptr, int line = 0);
    void info (const std::string& msg,
               const char* file = nullptr, int line = 0);
    void warn (const std::string& msg,
               const char* file = nullptr, int line = 0);
    void error(const std::string& msg,
               const char* file = nullptr, int line = 0);

    // Flush and close
    void flush();
    ~Logger();

private:
    Logger();
    Logger(const Logger&)            = delete;
    Logger& operator=(const Logger&) = delete;

    std::mutex      mtx_;
    LogLevel        consoleLevel_{ LogLevel::INFO };
    LogLevel        fileLevel_   { LogLevel::DEBUG };
    std::ofstream   fileStream_;
    bool            colourOn_    { true };

    static const char* levelStr(LogLevel l);
    static const char* levelColour(LogLevel l);
    std::string timestamp() const;
};

// ── Convenience macros ───────────────────────────────────────
#define LOG_DEBUG(msg) nra::Logger::instance().debug((msg), __FILE__, __LINE__)
#define LOG_INFO(msg)  nra::Logger::instance().info ((msg), __FILE__, __LINE__)
#define LOG_WARN(msg)  nra::Logger::instance().warn ((msg), __FILE__, __LINE__)
#define LOG_ERROR(msg) nra::Logger::instance().error((msg), __FILE__, __LINE__)

// Stream-style helper
struct LogStream {
    LogLevel            lvl;
    std::ostringstream  oss;
    const char*         file;
    int                 line;
    LogStream(LogLevel l, const char* f, int ln) : lvl(l), file(f), line(ln) {}
    ~LogStream() { Logger::instance().log(lvl, oss.str(), file, line); }
    template<typename T> LogStream& operator<<(const T& v) { oss << v; return *this; }
};

#define LOG_S(level) nra::LogStream(nra::LogLevel::level, __FILE__, __LINE__)

} // namespace nra

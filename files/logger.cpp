// ============================================================
//  logger.cpp
// ============================================================
#include "logger.hpp"
#include "common.hpp"
#include <iostream>
#include <iomanip>
#include <ctime>
#include <sstream>

namespace nra {

bool colour::enabled = true;

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

Logger::Logger() = default;

Logger::~Logger() {
    flush();
    if (fileStream_.is_open()) fileStream_.close();
}

void Logger::setConsoleLevel(LogLevel lvl) {
    std::lock_guard<std::mutex> lk(mtx_);
    consoleLevel_ = lvl;
}

void Logger::setFileLevel(LogLevel lvl) {
    std::lock_guard<std::mutex> lk(mtx_);
    fileLevel_ = lvl;
}

bool Logger::openLogFile(const std::string& path) {
    std::lock_guard<std::mutex> lk(mtx_);
    fileStream_.open(path, std::ios::app);
    return fileStream_.is_open();
}

void Logger::enableColour(bool on) {
    colour::enabled = on;
    colourOn_ = on;
}

std::string Logger::timestamp() const {
    auto now  = std::time(nullptr);
    auto* tm  = std::localtime(&now);
    char  buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
    return buf;
}

const char* Logger::levelStr(LogLevel l) {
    switch (l) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERR:   return "ERROR";
        default:              return "?????";
    }
}

const char* Logger::levelColour(LogLevel l) {
    if (!colour::enabled) return "";
    switch (l) {
        case LogLevel::DEBUG: return "\033[36m";   // cyan
        case LogLevel::INFO:  return "\033[32m";   // green
        case LogLevel::WARN:  return "\033[33m";   // yellow
        case LogLevel::ERR:   return "\033[31m";   // red
        default:              return "";
    }
}

void Logger::log(LogLevel lvl, const std::string& msg,
                 const char* file, int line) {
    std::lock_guard<std::mutex> lk(mtx_);
    std::string ts = timestamp();

    // ── Console output ───────────────────────────────────────
    if (lvl >= consoleLevel_) {
        std::ostream& con = (lvl >= LogLevel::WARN) ? std::cerr : std::cout;
        con << levelColour(lvl)
            << "[" << ts << "] "
            << "[" << levelStr(lvl) << "] "
            << msg;
        if (file && lvl == LogLevel::DEBUG)
            con << "  (" << file << ":" << line << ")";
        con << colour::reset() << "\n";
    }

    // ── File output (plain text) ─────────────────────────────
    if (fileStream_.is_open() && lvl >= fileLevel_) {
        fileStream_ << "[" << ts << "] [" << levelStr(lvl) << "] " << msg;
        if (file && lvl <= LogLevel::DEBUG)
            fileStream_ << "  (" << file << ":" << line << ")";
        fileStream_ << "\n";
    }
}

void Logger::debug(const std::string& msg, const char* f, int l)
{ log(LogLevel::DEBUG, msg, f, l); }
void Logger::info (const std::string& msg, const char* f, int l)
{ log(LogLevel::INFO,  msg, f, l); }
void Logger::warn (const std::string& msg, const char* f, int l)
{ log(LogLevel::WARN,  msg, f, l); }
void Logger::error(const std::string& msg, const char* f, int l)
{ log(LogLevel::ERR,   msg, f, l); }

void Logger::flush() {
    std::lock_guard<std::mutex> lk(mtx_);
    std::cout.flush();
    std::cerr.flush();
    if (fileStream_.is_open()) fileStream_.flush();
}

} // namespace nra

#pragma once
// ============================================================
//  common.hpp  –  Shared types, constants, error hierarchy
//  Network Relationship Analyzer  (production build)
// ============================================================
#include <string>
#include <stdexcept>
#include <cstdint>
#include <limits>
#include <optional>
#include <chrono>

namespace nra {

// ── Version ────────────────────────────────────────────────
constexpr const char* VERSION       = "2.0.0";
constexpr const char* BUILD_DATE    = __DATE__;

// ── Numeric aliases ─────────────────────────────────────────
using NodeId   = int32_t;
using EdgeId   = int64_t;
using Weight   = double;

constexpr NodeId  INVALID_NODE = -1;
constexpr Weight  INF_WEIGHT   = std::numeric_limits<Weight>::infinity();
constexpr Weight  DEFAULT_W    = 1.0;

// ── Exception hierarchy ─────────────────────────────────────
struct NraException : public std::runtime_error {
    explicit NraException(const std::string& msg) : std::runtime_error(msg) {}
};
struct IoException       : public NraException { using NraException::NraException; };
struct ParseException    : public NraException { using NraException::NraException; };
struct AlgorithmException: public NraException { using NraException::NraException; };
struct ConfigException   : public NraException { using NraException::NraException; };

// ── Timing helper ────────────────────────────────────────────
struct Timer {
    using Clock = std::chrono::high_resolution_clock;
    Clock::time_point start = Clock::now();
    [[nodiscard]] double elapsedMs() const {
        return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    }
    void reset() { start = Clock::now(); }
};

// ── ANSI colours (can be disabled) ──────────────────────────
namespace colour {
    extern bool enabled;
    inline const char* reset()    { return enabled ? "\033[0m"  : ""; }
    inline const char* bold()     { return enabled ? "\033[1m"  : ""; }
    inline const char* red()      { return enabled ? "\033[31m" : ""; }
    inline const char* green()    { return enabled ? "\033[32m" : ""; }
    inline const char* yellow()   { return enabled ? "\033[33m" : ""; }
    inline const char* blue()     { return enabled ? "\033[34m" : ""; }
    inline const char* magenta()  { return enabled ? "\033[35m" : ""; }
    inline const char* cyan()     { return enabled ? "\033[36m" : ""; }
    inline const char* bgRed()    { return enabled ? "\033[41m" : ""; }
}

} // namespace nra

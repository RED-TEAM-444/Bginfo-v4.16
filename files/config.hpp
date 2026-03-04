#pragma once
// ============================================================
//  config.hpp  –  INI-style configuration manager
// ============================================================
#include "common.hpp"
#include <string>
#include <unordered_map>
#include <optional>
#include <vector>

namespace nra {

class Config {
public:
    static Config& instance();

    // Load from file or command-line args
    bool   loadFile(const std::string& path);
    void   loadDefaults();
    void   set(const std::string& section, const std::string& key,
               const std::string& value);

    // Typed getters (return default if missing)
    [[nodiscard]] std::string  getString (const std::string& section,
                                          const std::string& key,
                                          const std::string& def = "") const;
    [[nodiscard]] int          getInt    (const std::string& section,
                                          const std::string& key,
                                          int def = 0)                  const;
    [[nodiscard]] double       getDouble (const std::string& section,
                                          const std::string& key,
                                          double def = 0.0)             const;
    [[nodiscard]] bool         getBool   (const std::string& section,
                                          const std::string& key,
                                          bool def = false)             const;

    void dump() const; // print all keys for diagnostics

private:
    Config() { loadDefaults(); }
    Config(const Config&)            = delete;
    Config& operator=(const Config&) = delete;

    // storage: section -> key -> value
    std::unordered_map<std::string,
        std::unordered_map<std::string, std::string>> data_;

    static std::string normalise(const std::string& s);
};

} // namespace nra

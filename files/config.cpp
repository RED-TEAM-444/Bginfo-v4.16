// ============================================================
//  config.cpp  –  INI-style config manager
// ============================================================
#include "config.hpp"
#include "logger.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace nra {

Config& Config::instance() {
    static Config inst;
    return inst;
}

std::string Config::normalise(const std::string& s) {
    std::string r;
    for (char c : s) r += (char)std::tolower((unsigned char)c);
    return r;
}

void Config::loadDefaults() {
    // [graph]
    set("graph", "directed",          "false");
    set("graph", "weighted",          "false");
    set("graph", "allow_self_loops",  "false");
    set("graph", "allow_multi_edges", "false");

    // [csv]
    set("csv", "source_col",   "0");
    set("csv", "target_col",   "1");
    set("csv", "weight_col",   "-1");
    set("csv", "delimiter",    ",");
    set("csv", "has_header",   "true");

    // [algorithms]
    set("algorithms", "pagerank_iter",    "100");
    set("algorithms", "pagerank_damping", "0.85");
    set("algorithms", "pagerank_tol",     "1e-6");
    set("algorithms", "eigen_iter",       "100");
    set("algorithms", "lp_max_iter",      "100");
    set("algorithms", "louvain_max_iter", "100");
    set("algorithms", "link_pred_topk",   "20");

    // [anomaly]
    set("anomaly", "density_threshold",  "0.55");
    set("anomaly", "zscore_threshold",   "2.5");
    set("anomaly", "min_cluster_size",   "3");

    // [output]
    set("output", "format",      "text");
    set("output", "colour",      "true");
    set("output", "verbose",     "false");
    set("output", "top_k",       "20");
    set("output", "log_file",    "nra.log");
    set("output", "log_level",   "info");
}

bool Config::loadFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        LOG_WARN("Config file not found: " + path + " – using defaults.");
        return false;
    }
    std::string line, section;
    int lineNo = 0;
    while (std::getline(f, line)) {
        ++lineNo;
        // strip CR
        if (!line.empty() && line.back() == '\r') line.pop_back();
        // strip comments & blank lines
        auto commentPos = line.find('#');
        if (commentPos != std::string::npos) line = line.substr(0, commentPos);
        // trim
        auto trim = [](std::string& s) {
            s.erase(0, s.find_first_not_of(" \t"));
            auto p = s.find_last_not_of(" \t");
            if (p != std::string::npos) s.erase(p+1);
        };
        trim(line);
        if (line.empty()) continue;

        // section header [name]
        if (line.front() == '[' && line.back() == ']') {
            section = normalise(line.substr(1, line.size()-2));
            continue;
        }
        // key = value
        auto eq = line.find('=');
        if (eq == std::string::npos) {
            LOG_WARN("Malformed config line " + std::to_string(lineNo) + ": " + line);
            continue;
        }
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq+1);
        trim(key); trim(val);
        if (!section.empty() && !key.empty())
            set(section, normalise(key), val);
    }
    LOG_INFO("Config loaded from: " + path);
    return true;
}

void Config::set(const std::string& section, const std::string& key,
                 const std::string& value) {
    data_[normalise(section)][normalise(key)] = value;
}

std::string Config::getString(const std::string& section,
                               const std::string& key,
                               const std::string& def) const {
    auto si = data_.find(normalise(section));
    if (si == data_.end()) return def;
    auto ki = si->second.find(normalise(key));
    return (ki == si->second.end()) ? def : ki->second;
}

int Config::getInt(const std::string& section, const std::string& key,
                   int def) const {
    auto s = getString(section, key);
    if (s.empty()) return def;
    try { return std::stoi(s); } catch (...) { return def; }
}

double Config::getDouble(const std::string& section, const std::string& key,
                          double def) const {
    auto s = getString(section, key);
    if (s.empty()) return def;
    try { return std::stod(s); } catch (...) { return def; }
}

bool Config::getBool(const std::string& section, const std::string& key,
                     bool def) const {
    auto s = getString(section, key);
    if (s.empty()) return def;
    auto n = normalise(s);
    if (n == "true"  || n == "1" || n == "yes") return true;
    if (n == "false" || n == "0" || n == "no")  return false;
    return def;
}

void Config::dump() const {
    for (auto& [sec, kvs] : data_) {
        std::cout << "[" << sec << "]\n";
        for (auto& [k, v] : kvs)
            std::cout << "  " << k << " = " << v << "\n";
    }
}

} // namespace nra

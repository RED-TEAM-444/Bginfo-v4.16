// ============================================================
//  csv_parser.cpp  –  RFC-4180 compliant CSV parser
// ============================================================
#include "csv_parser.hpp"
#include "logger.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <charconv>

namespace nra {

// ─── CsvRow helpers ──────────────────────────────────────────
bool CsvRow::has(size_t idx) const noexcept {
    return idx < fields.size();
}

const std::string& CsvRow::get(size_t idx) const {
    if (idx >= fields.size())
        throw ParseException("CsvRow: index " + std::to_string(idx) +
                             " out of range (size=" +
                             std::to_string(fields.size()) + ")");
    return fields[idx];
}

std::optional<double> CsvRow::getDouble(size_t idx) const noexcept {
    if (idx >= fields.size()) return std::nullopt;
    try {
        size_t pos;
        double v = std::stod(fields[idx], &pos);
        if (pos == fields[idx].size()) return v;
    } catch (...) {}
    return std::nullopt;
}

// ─── CsvParser ───────────────────────────────────────────────
CsvParser::CsvParser(CsvSchema schema) : schema_(schema) {}

void CsvParser::stripBOM(std::string& line) const {
    // UTF-8 BOM: 0xEF 0xBB 0xBF
    if (line.size() >= 3 &&
        (unsigned char)line[0] == 0xEF &&
        (unsigned char)line[1] == 0xBB &&
        (unsigned char)line[2] == 0xBF)
        line.erase(0, 3);
}

std::vector<std::string>
CsvParser::parseLine(const std::string& line) const {
    std::vector<std::string> result;
    std::string field;
    bool inQuote = false;
    char delim   = schema_.delimiter;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (inQuote) {
            if (c == '"') {
                // peek next
                if (i + 1 < line.size() && line[i+1] == '"') {
                    field += '"'; ++i;  // escaped quote
                } else {
                    inQuote = false;
                }
            } else {
                field += c;
            }
        } else {
            if (c == '"') {
                inQuote = true;
            } else if (c == delim) {
                // trim whitespace around field
                auto s = field.find_first_not_of(" \t");
                auto e = field.find_last_not_of(" \t");
                result.push_back((s == std::string::npos) ? "" :
                                  field.substr(s, e - s + 1));
                field.clear();
            } else {
                field += c;
            }
        }
    }
    // last field
    auto s = field.find_first_not_of(" \t");
    auto e = field.find_last_not_of(" \t");
    result.push_back((s == std::string::npos) ? "" :
                      field.substr(s, e - s + 1));
    return result;
}

CsvStats CsvParser::parse(const std::string& path,
                           std::function<void(const CsvRow&)> callback) {
    CsvStats stats;
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open())
        throw IoException("Cannot open CSV: " + path);

    headers_.clear();
    std::string line;
    bool firstLine = true;
    size_t lineNo  = 0;

    while (std::getline(f, line)) {
        ++lineNo;
        // strip CR
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (firstLine) { stripBOM(line); firstLine = false; }

        if (line.empty() || line.find_first_not_of(" \t") == std::string::npos) {
            ++stats.blankRows;
            continue;
        }
        // skip comment lines
        if (line[0] == '#') { ++stats.skippedRows; continue; }

        auto fields = parseLine(line);
        if (fields.size() < 2) {
            ++stats.malformedRows;
            LOG_WARN("CSV line " + std::to_string(lineNo) +
                     " has < 2 fields, skipping.");
            continue;
        }

        // header row
        if (schema_.hasHeader && lineNo == 1) {
            headers_ = fields;
            ++stats.skippedRows;
            continue;
        }

        CsvRow row;
        row.fields     = std::move(fields);
        row.lineNumber = lineNo;
        ++stats.totalRows;
        callback(row);
    }
    LOG_INFO("CSV parsed: " + std::to_string(stats.totalRows) +
             " rows, " + std::to_string(stats.malformedRows) +
             " malformed, file=" + path);
    return stats;
}

std::pair<CsvStats, std::vector<CsvRow>>
CsvParser::parseAll(const std::string& path) {
    std::vector<CsvRow> rows;
    auto stats = parse(path, [&](const CsvRow& r){ rows.push_back(r); });
    return {stats, std::move(rows)};
}

std::vector<CsvRow> CsvParser::preview(const std::string& path,
                                        size_t maxRows) {
    std::vector<CsvRow> rows;
    try {
        parse(path, [&](const CsvRow& r) {
            if (rows.size() < maxRows) rows.push_back(r);
        });
    } catch (...) {}
    return rows;
}

} // namespace nra

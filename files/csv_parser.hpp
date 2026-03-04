#pragma once
// ============================================================
//  csv_parser.hpp  –  RFC-4180 compliant CSV parser
//  Handles: quoted fields, embedded commas, \r\n, BOM,
//           custom delimiters, type inference
// ============================================================
#include "common.hpp"
#include <string>
#include <vector>
#include <fstream>
#include <functional>
#include <optional>

namespace nra {

struct CsvRow {
    std::vector<std::string> fields;
    size_t                   lineNumber{0};

    [[nodiscard]] bool        has(size_t idx) const noexcept;
    [[nodiscard]] const std::string& get(size_t idx) const;
    [[nodiscard]] std::optional<double> getDouble(size_t idx) const noexcept;
    [[nodiscard]] size_t size() const noexcept { return fields.size(); }
};

struct CsvSchema {
    int    sourceCol  { 0 };   // column index for edge source
    int    targetCol  { 1 };   // column index for edge target
    int    weightCol  { -1 };  // -1 = none
    char   delimiter  { ',' };
    bool   hasHeader  { true };
};

struct CsvStats {
    size_t totalRows     {0};
    size_t skippedRows   {0};
    size_t malformedRows {0};
    size_t blankRows     {0};
};

class CsvParser {
public:
    explicit CsvParser(CsvSchema schema = {});

    // Parse entire file; callback receives each data row
    [[nodiscard]] CsvStats parse(
        const std::string& path,
        std::function<void(const CsvRow&)> rowCallback);

    // Parse and collect all rows
    [[nodiscard]] std::pair<CsvStats, std::vector<CsvRow>>
        parseAll(const std::string& path);

    // Peek first N lines (preview)
    [[nodiscard]] std::vector<CsvRow>
        preview(const std::string& path, size_t maxRows = 5);

    const std::vector<std::string>& headers() const { return headers_; }

private:
    CsvSchema schema_;
    std::vector<std::string> headers_;

    std::vector<std::string> parseLine(const std::string& line) const;
    void stripBOM(std::string& line) const;
};

} // namespace nra

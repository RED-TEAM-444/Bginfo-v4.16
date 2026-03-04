#pragma once
// ============================================================
//  reporter.hpp  –  Multi-format output (TXT / CSV / JSON)
// ============================================================
#include "common.hpp"
#include "graph.hpp"
#include "algorithms.hpp"
#include <string>
#include <ostream>
#include <memory>

namespace nra {

enum class ReportFormat { TEXT, CSV, JSON };

struct ReportOptions {
    ReportFormat format        { ReportFormat::TEXT };
    bool         includeColour { true  };
    bool         verbose       { false };
    int          topK          { 20    };
    std::string  outputPath;           // empty = stdout
};

class Reporter {
public:
    explicit Reporter(ReportOptions opts = {});

    // ── Section writers ──────────────────────────────────────
    void writeHeader     (const Graph& g);
    void writeSummary    (const Graph& g);
    void writeCentrality (const Graph& g,
                          const algo::CentralityResult& cr);
    void writePath       (const Graph& g,
                          const algo::PathResult& pr,
                          NodeId src, NodeId dst);
    void writeCommunities(const Graph& g,
                          const algo::CommunityResult& cr);
    void writeAnomalies  (const Graph& g,
                          const algo::AnomalyResult& ar);
    void writeLinkPredictions(const Graph& g,
                              const std::vector<algo::LinkPrediction>& lp);
    void writeSCC        (const Graph& g,
                          const algo::SccResult& sr);
    void writeKCore      (const Graph& g,
                          const algo::KCoreResult& kr);
    void writeTriangles  (const Graph& g,
                          int64_t global,
                          const std::unordered_map<NodeId,int>& perNode);
    void writeNodeDetail (const Graph& g, NodeId u,
                          const algo::CentralityResult& cr);
    void writeFooter     ();
    void flush();

private:
    ReportOptions opts_;
    std::unique_ptr<std::ostream>  fileStream_;
    std::ostream*                  out_ { nullptr };

    // Format-specific helpers
    void textSep(char c = '-', int w = 60);
    void jsonKV(const std::string& key, const std::string& val, bool last=false);
    void jsonKV(const std::string& key, double val,             bool last=false);
    void jsonKV(const std::string& key, int64_t val,            bool last=false);

    std::string riskTag(int level) const;
    std::string scoreBar(double score, int width = 20) const;
};

} // namespace nra

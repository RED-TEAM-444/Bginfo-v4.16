// ============================================================
//  reporter.cpp  –  Multi-format output engine
// ============================================================
#include "reporter.hpp"
#include "logger.hpp"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <map>

namespace nra {

namespace c = colour;

Reporter::Reporter(ReportOptions opts) : opts_(std::move(opts)) {
    if (!opts_.outputPath.empty()) {
        auto fs = std::make_unique<std::ofstream>(opts_.outputPath);
        if (!fs->is_open())
            throw IoException("Cannot open report file: " + opts_.outputPath);
        fileStream_ = std::move(fs);
        out_ = fileStream_.get();
    } else {
        out_ = &std::cout;
    }
    // disable colour when writing to file
    if (fileStream_) opts_.includeColour = false;
}

void Reporter::flush() {
    if (out_) out_->flush();
}

void Reporter::textSep(char c, int w) {
    *out_ << "  ";
    for (int i = 0; i < w; ++i) *out_ << c;
    *out_ << "\n";
}

std::string Reporter::riskTag(int level) const {
    switch(level) {
        case 2: return std::string(c::bgRed()) + std::string(c::bold()) +
                       " HIGH RISK " + c::reset();
        case 1: return std::string(c::yellow()) + std::string(c::bold()) +
                       " MODERATE  " + c::reset();
        default:return std::string(c::green()) +
                       " LOW       " + c::reset();
    }
}

std::string Reporter::scoreBar(double score, int width) const {
    int filled = (int)std::round(score / 100.0 * width);
    filled = std::max(0, std::min(width, filled));
    std::string bar = "[";
    for (int i = 0; i < filled; ++i)  bar += '#';
    for (int i = filled; i < width; ++i) bar += '-';
    bar += "]";
    return bar;
}

// ── Header ────────────────────────────────────────────────────
void Reporter::writeHeader(const Graph& g) {
    if (opts_.format == ReportFormat::JSON) {
        *out_ << "{\n  \"report\": {\n";
        return;
    }
    *out_ << c::bold() << c::blue()
          << "\n  ╔══════════════════════════════════════════════════════╗\n"
          << "  ║        NETWORK RELATIONSHIP ANALYZER v" << VERSION << "       ║\n"
          << "  ║             Production Analysis Report               ║\n"
          << "  ╚══════════════════════════════════════════════════════╝\n"
          << c::reset() << "\n";
}

// ── Summary ───────────────────────────────────────────────────
void Reporter::writeSummary(const Graph& g) {
    if (opts_.format == ReportFormat::JSON) {
        *out_ << "    \"summary\": {\n";
        jsonKV("nodes",    (int64_t)g.nodeCount());
        jsonKV("edges",    (int64_t)g.edgeCount());
        jsonKV("directed", (int64_t)(g.options().directed ? 1 : 0));
        jsonKV("density",  g.density());
        jsonKV("avg_degree", g.avgDegree(), true);
        *out_ << "    },\n";
        return;
    }

    // Find top node by degree
    NodeId topNode = INVALID_NODE; int topDeg = 0;
    g.forEachNode([&](NodeId u, const NodeMeta&){
        if (g.degree(u) > topDeg) { topDeg = g.degree(u); topNode = u; }
    });

    textSep('-');
    *out_ << c::bold() << "  GRAPH SUMMARY\n" << c::reset();
    textSep('-');
    *out_ << c::cyan() << "  Nodes          : " << c::bold()
          << g.nodeCount() << c::reset() << "\n";
    *out_ << c::cyan() << "  Edges          : " << c::bold()
          << g.edgeCount() << c::reset() << "\n";
    *out_ << c::cyan() << "  Directed       : " << c::bold()
          << (g.options().directed ? "Yes" : "No") << c::reset() << "\n";
    *out_ << c::cyan() << "  Weighted       : " << c::bold()
          << (g.options().weighted ? "Yes" : "No") << c::reset() << "\n";
    *out_ << c::cyan() << "  Density        : " << c::bold()
          << std::fixed << std::setprecision(4) << g.density() << c::reset() << "\n";
    *out_ << c::cyan() << "  Avg Degree     : " << c::bold()
          << std::setprecision(2) << g.avgDegree() << c::reset() << "\n";
    if (topNode != INVALID_NODE)
        *out_ << c::cyan() << "  Highest Degree : " << c::bold()
              << g.meta(topNode).name << " (" << topDeg << ")"
              << c::reset() << "\n";
}

// ── Centrality ────────────────────────────────────────────────
void Reporter::writeCentrality(const Graph& g,
                                const algo::CentralityResult& cr) {
    if (opts_.format == ReportFormat::CSV) {
        *out_ << "name,degree,degree_centrality,betweenness,"
                 "closeness,pagerank,eigenvector\n";
        // Collect and sort by betweenness
        auto nodes = g.allNodes();
        std::sort(nodes.begin(), nodes.end(),
            [&](NodeId a, NodeId b){
                return cr.betweenness.at(a) > cr.betweenness.at(b); });
        for (NodeId u : nodes) {
            *out_ << "\"" << g.meta(u).name << "\","
                  << g.degree(u)  << ","
                  << std::fixed << std::setprecision(6)
                  << cr.degree.at(u)      << ","
                  << cr.betweenness.at(u) << ","
                  << cr.closeness.at(u)   << ","
                  << cr.pageRank.at(u)    << ","
                  << cr.eigenvector.at(u) << "\n";
        }
        return;
    }
    if (opts_.format == ReportFormat::JSON) {
        *out_ << "    \"centrality\": [\n";
        auto nodes = g.allNodes();
        std::sort(nodes.begin(), nodes.end(),
            [&](NodeId a, NodeId b){
                return cr.betweenness.at(a) > cr.betweenness.at(b); });
        for (size_t i = 0; i < nodes.size(); ++i) {
            NodeId u = nodes[i];
            *out_ << "      {\"name\":\"" << g.meta(u).name
                  << "\",\"degree\":" << g.degree(u)
                  << ",\"betweenness\":" << std::fixed << std::setprecision(6)
                  << cr.betweenness.at(u)
                  << ",\"pagerank\":" << cr.pageRank.at(u)
                  << "}" << (i+1 < nodes.size() ? "," : "") << "\n";
        }
        *out_ << "    ],\n";
        return;
    }

    textSep('-');
    *out_ << c::bold() << "  CENTRALITY RANKINGS  (top "
          << opts_.topK << ")\n" << c::reset();
    textSep('-');
    *out_ << c::bold() << std::left
          << std::setw(4) << "#"
          << std::setw(22) << "Name"
          << std::setw(7) << "Deg"
          << std::setw(12) << "Betweenness"
          << std::setw(11) << "Closeness"
          << std::setw(11) << "PageRank"
          << "Eigenvec\n" << c::reset();
    textSep('-');

    auto nodes = g.allNodes();
    std::sort(nodes.begin(), nodes.end(),
        [&](NodeId a, NodeId b){
            return cr.betweenness.at(a) > cr.betweenness.at(b); });

    int shown = 0;
    for (NodeId u : nodes) {
        if (++shown > opts_.topK) break;
        double bc = cr.betweenness.at(u);
        double cc = cr.closeness.at(u);
        double pr = cr.pageRank.at(u);
        double ev = cr.eigenvector.at(u);

        // highlight top-3 in bold
        const char* clr = (shown <= 3) ? c::yellow() : c::reset();
        *out_ << std::left << std::setw(4) << shown
              << clr << c::bold() << std::setw(22) << g.meta(u).name << c::reset()
              << std::setw(7) << g.degree(u)
              << std::fixed << std::setprecision(2)
              << std::setw(12) << bc
              << std::setw(11) << std::setprecision(4) << cc
              << std::setw(11) << pr
              << ev << "\n";
    }
    *out_ << c::cyan() << "  Computed in " << cr.computeMs
          << " ms\n" << c::reset();
}

// ── Path ──────────────────────────────────────────────────────
void Reporter::writePath(const Graph& g, const algo::PathResult& pr,
                          NodeId src, NodeId dst) {
    textSep('-');
    *out_ << c::bold() << "  SHORTEST PATH: "
          << g.meta(src).name << " → " << g.meta(dst).name
          << "\n" << c::reset();
    textSep('-');
    if (!pr.found) {
        *out_ << c::red() << "  ✗ No path exists between these nodes.\n"
              << c::reset();
        return;
    }
    *out_ << c::green() << "  ✓ Found! "
          << c::bold() << pr.hops << c::reset() << c::green() << " hop(s)";
    if (g.options().weighted)
        *out_ << ", total weight = " << std::fixed << std::setprecision(4)
              << pr.totalWeight;
    *out_ << "  (" << pr.computeMs << " ms)\n\n" << c::reset();

    *out_ << "  ";
    for (size_t i = 0; i < pr.path.size(); ++i) {
        if (i) *out_ << c::cyan() << " ──► " << c::reset();
        *out_ << c::bold() << g.meta(pr.path[i]).name << c::reset();
    }
    *out_ << "\n";
}

// ── Communities ───────────────────────────────────────────────
void Reporter::writeCommunities(const Graph& g,
                                 const algo::CommunityResult& cr) {
    textSep('-');
    *out_ << c::bold() << "  COMMUNITIES  ("
          << cr.communities.size() << " found, Q="
          << std::fixed << std::setprecision(4) << cr.totalModularity
          << ")\n" << c::reset();
    textSep('-');

    int ci = 1;
    for (const auto& comm : cr.communities) {
        *out_ << "  [" << c::bold() << ci++ << c::reset() << "]  "
              << c::yellow() << comm.members.size() << " members"
              << c::reset()
              << "  density=" << std::setprecision(3) << comm.density << "\n";
        *out_ << "  Members: ";
        int sh = 0;
        for (NodeId m : comm.members) {
            if (sh++) *out_ << ", ";
            *out_ << g.meta(m).name;
            if (sh >= 10 && (int)comm.members.size() > 10) {
                *out_ << " ...+" << comm.members.size()-10 << " more";
                break;
            }
        }
        *out_ << "\n\n";
    }
}

// ── Anomalies ─────────────────────────────────────────────────
void Reporter::writeAnomalies(const Graph& g,
                               const algo::AnomalyResult& ar) {
    textSep('=');
    *out_ << c::bold() << c::red()
          << "  ANOMALY DETECTION REPORT\n" << c::reset();
    textSep('=');

    // Suspicious clusters
    *out_ << c::bold() << "\n  Suspicious Clusters: "
          << ar.clusters.size() << "\n" << c::reset();

    if (ar.clusters.empty()) {
        *out_ << c::green() << "  ✓ No suspicious clusters detected.\n"
              << c::reset();
    } else {
        for (size_t i = 0; i < ar.clusters.size(); ++i) {
            const auto& sc = ar.clusters[i];
            *out_ << "\n  Cluster #" << (i+1)
                  << "  " << riskTag(sc.riskLevel)
                  << "  score=" << c::bold()
                  << std::fixed << std::setprecision(1) << sc.suspicionScore
                  << c::reset()
                  << "  " << scoreBar(sc.suspicionScore) << "\n";
            *out_ << "  Size=" << sc.members.size()
                  << "  density=" << std::setprecision(3) << sc.density
                  << "  avgDeg=" << std::setprecision(1) << sc.avgDegree << "\n";
            *out_ << "  Flags: " << c::yellow() << sc.reasons << c::reset() << "\n";
            *out_ << "  Members: ";
            int sh = 0;
            for (NodeId m : sc.members) {
                if (sh++) *out_ << ", ";
                *out_ << c::bold() << g.meta(m).name << c::reset();
                if (++sh > 8) { *out_ << " ..."; break; }
            }
            *out_ << "\n";
        }
    }

    // Anomalous nodes (high z-score)
    *out_ << c::bold() << "\n  Anomalous Nodes (high degree z-score): "
          << ar.anomalousNodes.size() << "\n" << c::reset();
    int sh = 0;
    for (NodeId n : ar.anomalousNodes) {
        if (++sh > opts_.topK) break;
        *out_ << "    ▸ " << c::bold() << std::left << std::setw(20)
              << g.meta(n).name << c::reset()
              << "  deg=" << g.degree(n)
              << "  z=" << std::fixed << std::setprecision(2)
              << ar.nodeAnomalyScore.at(n) << "\n";
    }

    *out_ << c::cyan() << "\n  Computed in " << ar.computeMs
          << " ms\n" << c::reset();
}

// ── Link Predictions ──────────────────────────────────────────
void Reporter::writeLinkPredictions(const Graph& g,
    const std::vector<algo::LinkPrediction>& lp) {
    textSep('-');
    *out_ << c::bold() << "  HIDDEN RELATIONSHIP PREDICTIONS  (top "
          << opts_.topK << ")\n" << c::reset();
    textSep('-');

    if (lp.empty()) {
        *out_ << "  (none found – graph may be fully connected)\n";
        return;
    }
    for (size_t i = 0; i < lp.size(); ++i) {
        const auto& lnk = lp[i];
        *out_ << "  " << std::setw(3) << (i+1) << ".  "
              << c::bold() << std::left << std::setw(18) << g.meta(lnk.u).name
              << c::reset() << " ↔  "
              << c::bold() << std::setw(18) << g.meta(lnk.v).name << c::reset()
              << "  common=" << c::yellow() << lnk.commonCount << c::reset()
              << "  jaccard=" << std::fixed << std::setprecision(3) << lnk.jaccard
              << "  AA=" << std::setprecision(2) << lnk.adamicAdar
              << "  score=" << c::green() << std::setprecision(4)
              << lnk.combined << c::reset() << "\n";
        if (opts_.verbose && !lnk.commonNames.empty()) {
            *out_ << "         Via: ";
            for (size_t k = 0; k < lnk.commonNames.size() && k < 5; ++k) {
                if (k) *out_ << ", ";
                *out_ << c::cyan() << lnk.commonNames[k] << c::reset();
            }
            if (lnk.commonNames.size() > 5)
                *out_ << " ...+" << lnk.commonNames.size()-5;
            *out_ << "\n";
        }
    }
}

// ── SCC ───────────────────────────────────────────────────────
void Reporter::writeSCC(const Graph& g, const algo::SccResult& sr) {
    textSep('-');
    *out_ << c::bold() << "  STRONGLY CONNECTED COMPONENTS  ("
          << sr.components.size() << ")\n" << c::reset();
    textSep('-');
    int ci = 1;
    for (const auto& comp : sr.components) {
        *out_ << "  SCC " << c::bold() << ci++ << c::reset()
              << "  [" << comp.size() << " nodes]  ";
        int sh = 0;
        for (NodeId n : comp) {
            if (sh++) *out_ << ", ";
            *out_ << g.meta(n).name;
            if (++sh > 8) { *out_ << " ..."; break; }
        }
        *out_ << "\n";
    }
    *out_ << c::cyan() << "  Largest SCC: " << sr.largestSize << " nodes\n"
          << c::reset();
}

// ── K-core ────────────────────────────────────────────────────
void Reporter::writeKCore(const Graph& g, const algo::KCoreResult& kr) {
    textSep('-');
    *out_ << c::bold() << "  K-CORE DECOMPOSITION  (max core = "
          << kr.maxCore << ")\n" << c::reset();
    textSep('-');

    // group by core number
    std::map<int, std::vector<NodeId>> byCore;
    g.forEachNode([&](NodeId u, const NodeMeta&){
        byCore[kr.coreness.at(u)].push_back(u);
    });
    for (auto it = byCore.rbegin(); it != byCore.rend(); ++it) {
        *out_ << "  Core " << c::bold() << std::setw(3) << it->first
              << c::reset() << "  [" << it->second.size() << " nodes]: ";
        int sh = 0;
        for (NodeId n : it->second) {
            if (sh++) *out_ << ", ";
            *out_ << g.meta(n).name;
            if (++sh >= 8 && (int)it->second.size() > 8) {
                *out_ << " ...+" << it->second.size()-8;
                break;
            }
        }
        *out_ << "\n";
    }
}

// ── Triangles ─────────────────────────────────────────────────
void Reporter::writeTriangles(const Graph& g, int64_t global,
    const std::unordered_map<NodeId, int>& perNode) {
    textSep('-');
    *out_ << c::bold() << "  TRIANGLE COUNT\n" << c::reset();
    textSep('-');
    *out_ << "  Total triangles: " << c::bold() << global << c::reset() << "\n";
    if (!perNode.empty() && opts_.verbose) {
        *out_ << "\n  Per-node triangle participation:\n";
        std::vector<std::pair<NodeId,int>> sorted(perNode.begin(), perNode.end());
        std::sort(sorted.begin(), sorted.end(),
            [](const auto& a, const auto& b){ return a.second > b.second; });
        int sh = 0;
        for (const auto& [u, cnt] : sorted) {
            if (++sh > opts_.topK) break;
            if (cnt == 0) break;
            *out_ << "    " << std::left << std::setw(22) << g.meta(u).name
                  << "  triangles=" << cnt << "\n";
        }
    }
}

// ── Node Detail ───────────────────────────────────────────────
void Reporter::writeNodeDetail(const Graph& g, NodeId u,
                                const algo::CentralityResult& cr) {
    textSep('-');
    *out_ << c::bold() << "  NODE DETAIL: " << g.meta(u).name
          << "\n" << c::reset();
    textSep('-');
    *out_ << "  Degree         : " << g.degree(u) << "\n";
    if (g.options().directed)
        *out_ << "  In / Out       : " << g.inDegree(u) << " / " << g.outDegree(u) << "\n";
    *out_ << "  Betweenness    : " << std::fixed << std::setprecision(4)
          << cr.betweenness.at(u) << "\n";
    *out_ << "  Closeness      : " << cr.closeness.at(u) << "\n";
    *out_ << "  PageRank       : " << cr.pageRank.at(u) << "\n";
    *out_ << "  Eigenvector    : " << cr.eigenvector.at(u) << "\n";

    const auto& nbrs = g.neighbours(u);
    *out_ << "\n  Direct neighbours (" << nbrs.size() << "):\n";
    for (const auto& e : nbrs) {
        *out_ << "    ▸ " << c::bold() << std::left << std::setw(22)
              << g.meta(e.to).name << c::reset();
        if (g.options().weighted)
            *out_ << "  w=" << e.weight;
        *out_ << "\n";
    }
}

// ── Footer ────────────────────────────────────────────────────
void Reporter::writeFooter() {
    if (opts_.format == ReportFormat::JSON) {
        *out_ << "  }\n}\n";
        return;
    }
    textSep('=');
    *out_ << c::bold() << c::green()
          << "  Analysis complete.\n" << c::reset();
    textSep('=');
}

void Reporter::jsonKV(const std::string& k, const std::string& v, bool last) {
    *out_ << "      \"" << k << "\": \"" << v << "\"" << (last ? "" : ",") << "\n";
}
void Reporter::jsonKV(const std::string& k, double v, bool last) {
    *out_ << "      \"" << k << "\": " << std::fixed << std::setprecision(6)
          << v << (last ? "" : ",") << "\n";
}
void Reporter::jsonKV(const std::string& k, int64_t v, bool last) {
    *out_ << "      \"" << k << "\": " << v << (last ? "" : ",") << "\n";
}

} // namespace nra

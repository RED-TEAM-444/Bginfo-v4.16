// ============================================================
//  main.cpp  –  Interactive CLI + batch-mode entry point
//
//  Usage:
//    ./nra                               → interactive menu
//    ./nra --config config.ini           → load config then menu
//    ./nra --input net.csv --batch       → full batch analysis
//    ./nra --input net.csv --path A B    → just shortest path
//    ./nra --help                        → usage
// ============================================================
#include "common.hpp"
#include "logger.hpp"
#include "config.hpp"
#include "csv_parser.hpp"
#include "graph.hpp"
#include "algorithms.hpp"
#include "reporter.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <optional>
#include <functional>

using namespace nra;
namespace c = colour;

// ─── Helpers ─────────────────────────────────────────────────
static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static std::string prompt(const std::string& msg) {
    std::cout << c::yellow() << "  " << msg << c::reset();
    std::string s; std::getline(std::cin, s);
    return trim(s);
}

static std::string yesNo(bool v) { return v ? "Yes" : "No"; }

static void pause() {
    std::cout << c::cyan() << "\n  [Press ENTER to continue]" << c::reset();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

// ─── Application state ───────────────────────────────────────
struct AppState {
    Graph g;
    std::optional<algo::CentralityResult>   centrality;
    std::optional<algo::CommunityResult>    community;
    std::optional<algo::AnomalyResult>      anomaly;
    std::optional<algo::SccResult>          scc;
    std::optional<algo::KCoreResult>        kcore;
    bool loaded { false };
    std::string csvPath;
};

// ─── Graph loading ────────────────────────────────────────────
static bool loadGraph(AppState& st, const std::string& path,
                      bool directed, bool weighted,
                      int srcCol, int tgtCol, int wCol) {
    GraphOptions gopts;
    gopts.directed = directed;
    gopts.weighted = weighted;
    gopts.reserveNodes = 16384;
    gopts.reserveEdges = 65536;

    Graph newG(gopts);
    CsvSchema schema;
    schema.sourceCol = srcCol;
    schema.targetCol = tgtCol;
    schema.weightCol = wCol;
    schema.hasHeader = Config::instance().getBool("csv","has_header",true);

    CsvParser parser(schema);
    int edgesLoaded = 0;
    try {
        auto stats = parser.parse(path, [&](const CsvRow& row) {
            std::string src = row.get((size_t)srcCol);
            std::string tgt = row.get((size_t)tgtCol);
            if (src.empty() || tgt.empty()) return;
            double w = DEFAULT_W;
            if (weighted && wCol >= 0) {
                auto wv = row.getDouble((size_t)wCol);
                if (wv) w = *wv;
            }
            newG.addEdge(src, tgt, w);
            ++edgesLoaded;
        });
        LOG_INFO("Loaded " + std::to_string(stats.totalRows) + " rows, " +
                 std::to_string(edgesLoaded) + " edges, " +
                 std::to_string(stats.malformedRows) + " malformed.");
    } catch (const NraException& e) {
        LOG_ERROR(e.what());
        return false;
    }

    if (newG.nodeCount() == 0) {
        LOG_WARN("Graph is empty – check CSV columns and delimiter.");
        return false;
    }

    st.g          = std::move(newG);
    st.csvPath    = path;
    st.loaded     = true;
    st.centrality.reset();
    st.community.reset();
    st.anomaly.reset();
    st.scc.reset();
    st.kcore.reset();
    return true;
}

// ─── Ensure pre-computed data ─────────────────────────────────
static void ensureCentrality(AppState& st) {
    if (!st.centrality) {
        LOG_INFO("Computing centrality metrics...");
        st.centrality = algo::computeAllCentrality(st.g);
    }
}
static void ensureCommunity(AppState& st) {
    if (!st.community) {
        LOG_INFO("Running community detection...");
        bool louvain = Config::instance().getBool("algorithms","use_louvain",false);
        if (louvain) st.community = algo::louvainCommunity(st.g);
        else         st.community = algo::labelPropagation(st.g);
    }
}
static void ensureAnomaly(AppState& st) {
    ensureCommunity(st);
    if (!st.anomaly) {
        double dt = Config::instance().getDouble("anomaly","density_threshold",0.55);
        double zt = Config::instance().getDouble("anomaly","zscore_threshold",2.5);
        st.anomaly = algo::detectAnomalies(st.g, *st.community, dt, zt);
    }
}
static void ensureSCC(AppState& st) {
    if (!st.scc) {
        LOG_INFO("Computing SCCs (Tarjan)...");
        st.scc = algo::tarjanSCC(st.g);
    }
}
static void ensureKCore(AppState& st) {
    if (!st.kcore) {
        LOG_INFO("Computing k-core...");
        st.kcore = algo::kCoreDecomposition(st.g);
    }
}

// ─── Menu banner ──────────────────────────────────────────────
static void printBanner() {
    std::cout << c::bold() << c::blue()
<< "\n  ╔══════════════════════════════════════════════════════════╗\n"
<< "  ║         NETWORK RELATIONSHIP ANALYZER  v" << VERSION << "          ║\n"
<< "  ║    Graph Intelligence & Anomaly Detection Engine         ║\n"
<< "  ╚══════════════════════════════════════════════════════════╝\n"
              << c::reset();
}

static void printMenu(const AppState& st) {
    std::cout << c::bold()
              << "\n  ┌─ MAIN MENU ──────────────────────────────────────────┐\n";
    auto item = [](const char* num, const char* col, const char* desc) {
        std::cout << "  │  " << col << num << c::reset() << c::bold()
                  << "  " << std::left << std::setw(48) << desc << "│\n";
    };
    item("1",  c::green(),   "Load CSV file");
    item("2",  c::green(),   "Graph statistics & top nodes");
    item("3",  c::yellow(),  "Shortest path between two nodes");
    item("4",  c::yellow(),  "Node connections & 2nd-degree network");
    item("5",  c::cyan(),    "Predict hidden relationships");
    item("6",  c::red(),     "Detect suspicious clusters / anomalies");
    item("7",  c::magenta(), "Full centrality analysis");
    item("8",  c::magenta(), "Community detection");
    item("9",  c::magenta(), "Strongly connected components (SCC)");
    item("10", c::magenta(), "K-core decomposition");
    item("11", c::magenta(), "Triangle count");
    item("12", c::cyan(),    "Node deep-dive");
    item("13", c::green(),   "Run full analysis & export report");
    item("14", c::green(),   "Save / load graph snapshot");
    item("15", c::reset(),   "Settings");
    item("0",  c::reset(),   "Exit");
    std::cout << "  └──────────────────────────────────────────────────────┘\n"
              << c::reset();
    if (st.loaded)
        std::cout << c::green() << "  [Graph: " << st.g.nodeCount()
                  << " nodes, " << st.g.edgeCount() << " edges  — "
                  << st.csvPath << "]\n" << c::reset();
    else
        std::cout << c::red() << "  [No graph loaded – use option 1]\n"
                  << c::reset();
    std::cout << "  > ";
}

// ─── Individual menu handlers ─────────────────────────────────

static void handleLoad(AppState& st) {
    std::string path = prompt("CSV file path: ");
    if (path.empty()) { LOG_WARN("No path given."); return; }
    std::string d = prompt("Directed? [y/N]: ");
    std::string w = prompt("Weighted?  [y/N]: ");
    bool directed = (!d.empty() && (d[0]=='y'||d[0]=='Y'));
    bool weighted = (!w.empty() && (w[0]=='y'||w[0]=='Y'));
    int srcCol = 0, tgtCol = 1, wCol = -1;
    std::string sc = prompt("Source column index [0]: ");
    std::string tc = prompt("Target column index [1]: ");
    if (!sc.empty()) try { srcCol = std::stoi(sc); } catch(...) {}
    if (!tc.empty()) try { tgtCol = std::stoi(tc); } catch(...) {}
    if (weighted) {
        std::string wc = prompt("Weight column index [2]: ");
        wCol = 2;
        if (!wc.empty()) try { wCol = std::stoi(wc); } catch(...) {}
    }

    Timer t;
    std::cout << c::green() << "\n  Loading '" << path << "'...\n" << c::reset();
    if (loadGraph(st, path, directed, weighted, srcCol, tgtCol, wCol)) {
        std::cout << c::green() << "  ✓ Loaded in " << t.elapsedMs() << " ms\n"
                  << c::reset();
        ReportOptions ropt;
        Reporter rep(ropt);
        rep.writeSummary(st.g);
    }
}

static void handleStats(AppState& st) {
    if (!st.loaded) { LOG_WARN("Load a graph first."); return; }
    ReportOptions ropt; ropt.topK = 20;
    Reporter rep(ropt);
    rep.writeSummary(st.g);

    // Top nodes by degree
    auto nodes = st.g.allNodes();
    std::sort(nodes.begin(), nodes.end(),
        [&](NodeId a, NodeId b){ return st.g.degree(a) > st.g.degree(b); });
    std::cout << c::bold() << "\n  Top nodes by degree:\n" << c::reset();
    int sh = 0;
    for (NodeId u : nodes) {
        if (++sh > 15) break;
        std::cout << "  " << std::setw(3) << sh << ".  "
                  << c::bold() << std::left << std::setw(22)
                  << st.g.meta(u).name << c::reset()
                  << "  deg=" << c::yellow() << st.g.degree(u) << c::reset()
                  << "\n";
    }
}

static void handlePath(AppState& st) {
    if (!st.loaded) { LOG_WARN("Load a graph first."); return; }
    std::string srcName = prompt("Source node: ");
    std::string dstName = prompt("Target node: ");
    NodeId src = st.g.findNode(srcName);
    NodeId dst = st.g.findNode(dstName);
    if (src == INVALID_NODE) { LOG_ERROR("Source not found: " + srcName); return; }
    if (dst == INVALID_NODE) { LOG_ERROR("Target not found: " + dstName); return; }

    std::string algo = prompt("Algorithm [bfs/dijkstra/bellman]: ");
    algo::PathResult pr;
    if (algo == "dijkstra" || (algo.empty() && st.g.options().weighted))
        pr = algo::dijkstra(st.g, src, dst);
    else if (algo == "bellman")
        pr = algo::bellmanFord(st.g, src, dst);
    else
        pr = algo::bfs(st.g, src, dst);

    ReportOptions ropt;
    Reporter rep(ropt);
    rep.writePath(st.g, pr, src, dst);
}

static void handleNodeDetail(AppState& st) {
    if (!st.loaded) { LOG_WARN("Load a graph first."); return; }
    std::string name = prompt("Node name: ");
    NodeId u = st.g.findNode(name);
    if (u == INVALID_NODE) { LOG_ERROR("Node not found: " + name); return; }
    ensureCentrality(st);
    ReportOptions ropt; ropt.verbose = true;
    Reporter rep(ropt);
    rep.writeNodeDetail(st.g, u, *st.centrality);

    // 2nd degree
    std::unordered_map<NodeId, int> second;
    std::unordered_set<NodeId> direct;
    for (const auto& e : st.g.neighbours(u)) direct.insert(e.to);
    for (NodeId d : direct) {
        for (const auto& e : st.g.neighbours(d)) {
            if (e.to != u && !direct.count(e.to)) second[e.to]++;
        }
    }
    if (!second.empty()) {
        std::vector<std::pair<NodeId,int>> sv(second.begin(), second.end());
        std::sort(sv.begin(), sv.end(),[](auto& a,auto& b){ return a.second>b.second; });
        std::cout << c::cyan() << "\n  2nd-degree reachable ("
                  << second.size() << "):\n" << c::reset();
        int sh = 0;
        for (auto& [v, cnt] : sv) {
            std::cout << "    ◦ " << std::left << std::setw(22)
                      << st.g.meta(v).name << "  via " << cnt << " node(s)\n";
            if (++sh >= 10 && (int)sv.size() > 10) {
                std::cout << "    ... +" << sv.size()-10 << " more\n"; break;
            }
        }
    }
}

static void handleHiddenLinks(AppState& st) {
    if (!st.loaded) { LOG_WARN("Load a graph first."); return; }
    std::cout << c::cyan() << "\n  Computing hidden link predictions...\n" << c::reset();
    int topK = Config::instance().getInt("algorithms","link_pred_topk",20);
    auto lp = algo::predictLinks(st.g, topK);
    ReportOptions ropt; ropt.topK = topK; ropt.verbose = true;
    Reporter rep(ropt);
    rep.writeLinkPredictions(st.g, lp);
}

static void handleAnomalies(AppState& st) {
    if (!st.loaded) { LOG_WARN("Load a graph first."); return; }
    std::string dt = prompt("Density threshold [0.55]: ");
    std::string zt = prompt("Z-score threshold [2.50]: ");
    double dens = 0.55, zsc = 2.5;
    try { if (!dt.empty()) dens = std::stod(dt); } catch(...) {}
    try { if (!zt.empty()) zsc  = std::stod(zt); } catch(...) {}

    ensureCommunity(st);
    st.anomaly = algo::detectAnomalies(st.g, *st.community, dens, zsc);

    ReportOptions ropt; ropt.topK = 20;
    Reporter rep(ropt);
    rep.writeAnomalies(st.g, *st.anomaly);
}

static void handleCentrality(AppState& st) {
    if (!st.loaded) { LOG_WARN("Load a graph first."); return; }
    ensureCentrality(st);
    ReportOptions ropt;
    ropt.topK = 25;
    Reporter rep(ropt);
    rep.writeCentrality(st.g, *st.centrality);
}

static void handleCommunity(AppState& st) {
    if (!st.loaded) { LOG_WARN("Load a graph first."); return; }
    std::string algo = prompt("Algorithm [lp/louvain]: ");
    if (algo == "louvain")
        st.community = algo::louvainCommunity(st.g);
    else
        st.community = algo::labelPropagation(st.g);
    ReportOptions ropt;
    Reporter rep(ropt);
    rep.writeCommunities(st.g, *st.community);
}

static void handleSCC(AppState& st) {
    if (!st.loaded) { LOG_WARN("Load a graph first."); return; }
    ensureSCC(st);
    ReportOptions ropt;
    Reporter rep(ropt);
    rep.writeSCC(st.g, *st.scc);
}

static void handleKCore(AppState& st) {
    if (!st.loaded) { LOG_WARN("Load a graph first."); return; }
    ensureKCore(st);
    ReportOptions ropt; ropt.verbose = true;
    Reporter rep(ropt);
    rep.writeKCore(st.g, *st.kcore);
}

static void handleTriangles(AppState& st) {
    if (!st.loaded) { LOG_WARN("Load a graph first."); return; }
    std::cout << c::cyan() << "\n  Counting triangles...\n" << c::reset();
    Timer t;
    auto [global, perNode] = algo::countTriangles(st.g);
    std::cout << c::cyan() << "  Done in " << t.elapsedMs() << " ms\n" << c::reset();
    ReportOptions ropt; ropt.verbose = true; ropt.topK = 20;
    Reporter rep(ropt);
    rep.writeTriangles(st.g, global, perNode);
}

static void handleFullReport(AppState& st) {
    if (!st.loaded) { LOG_WARN("Load a graph first."); return; }
    std::string outPath = prompt("Output file (default: report_<graph>.txt): ");
    if (outPath.empty()) outPath = "report_" +
        st.csvPath.substr(st.csvPath.rfind('/')+1) + ".txt";
    std::string fmtStr = prompt("Format [text/csv/json]: ");
    ReportFormat fmt = ReportFormat::TEXT;
    if (fmtStr == "csv")  fmt = ReportFormat::CSV;
    if (fmtStr == "json") fmt = ReportFormat::JSON;

    std::cout << c::cyan() << "\n  Running full analysis...\n" << c::reset();

    ensureCentrality(st);
    ensureCommunity(st);
    ensureAnomaly(st);
    ensureSCC(st);
    ensureKCore(st);

    auto lp = algo::predictLinks(st.g,
        Config::instance().getInt("algorithms","link_pred_topk",20));
    auto [tri, perNode] = algo::countTriangles(st.g);

    ReportOptions ropt;
    ropt.format     = fmt;
    ropt.topK       = 25;
    ropt.verbose    = true;
    ropt.outputPath = outPath;
    Reporter rep(ropt);

    rep.writeHeader(st.g);
    rep.writeSummary(st.g);
    rep.writeCentrality(st.g, *st.centrality);
    rep.writeCommunities(st.g, *st.community);
    rep.writeAnomalies(st.g, *st.anomaly);
    rep.writeLinkPredictions(st.g, lp);
    rep.writeSCC(st.g, *st.scc);
    rep.writeKCore(st.g, *st.kcore);
    rep.writeTriangles(st.g, tri, perNode);
    rep.writeFooter();
    rep.flush();

    std::cout << c::green() << "  ✓ Report saved: " << outPath << "\n" << c::reset();
}

static void handleSnapshot(AppState& st) {
    if (!st.loaded) { LOG_WARN("Load a graph first."); return; }
    std::string op = prompt("[s]ave or [l]oad snapshot? ");
    if (op.empty()) return;
    std::string path = prompt("Snapshot file path: ");
    if (path.empty()) return;
    try {
        if (op[0]=='s') {
            st.g.saveSnapshot(path);
            std::cout << c::green() << "  ✓ Snapshot saved.\n" << c::reset();
        } else {
            GraphOptions gopts;
            Graph newG(gopts);
            newG.loadSnapshot(path);
            st.g = std::move(newG); st.loaded = true;
            st.centrality.reset(); st.community.reset();
            st.anomaly.reset(); st.scc.reset(); st.kcore.reset();
            std::cout << c::green() << "  ✓ Snapshot loaded: "
                      << st.g.nodeCount() << " nodes, "
                      << st.g.edgeCount() << " edges.\n" << c::reset();
        }
    } catch (const NraException& e) {
        LOG_ERROR(e.what());
    }
}

static void handleSettings() {
    std::cout << c::bold() << "\n  Current Configuration:\n" << c::reset();
    Config::instance().dump();
    std::string sec = prompt("\nSection to update (blank=skip): ");
    if (sec.empty()) return;
    std::string key = prompt("Key: ");
    std::string val = prompt("Value: ");
    Config::instance().set(sec, key, val);
    std::cout << c::green() << "  ✓ Updated.\n" << c::reset();
}

// ─── Batch mode ───────────────────────────────────────────────
static int runBatch(const std::string& csvPath,
                    const std::string& outPath,
                    bool directed, bool weighted) {
    AppState st;
    if (!loadGraph(st, csvPath, directed, weighted, 0, 1,
                   weighted ? 2 : -1)) {
        std::cerr << "Failed to load: " << csvPath << "\n";
        return 1;
    }
    ensureCentrality(st);
    ensureCommunity(st);
    ensureAnomaly(st);
    ensureSCC(st);
    ensureKCore(st);
    auto lp = algo::predictLinks(st.g, 20);
    auto [tri, perNode] = algo::countTriangles(st.g);

    ReportOptions ropt;
    ropt.topK = 25; ropt.verbose = true;
    ropt.outputPath = outPath;
    Reporter rep(ropt);
    rep.writeHeader(st.g);
    rep.writeSummary(st.g);
    rep.writeCentrality(st.g, *st.centrality);
    rep.writeCommunities(st.g, *st.community);
    rep.writeAnomalies(st.g, *st.anomaly);
    rep.writeLinkPredictions(st.g, lp);
    rep.writeSCC(st.g, *st.scc);
    rep.writeKCore(st.g, *st.kcore);
    rep.writeTriangles(st.g, tri, perNode);
    rep.writeFooter();
    rep.flush();
    std::cout << "Report written to: " << outPath << "\n";
    return 0;
}

// ─── Help ─────────────────────────────────────────────────────
static void printHelp(const char* prog) {
    std::cout << "Usage:\n"
              << "  " << prog << "                        Interactive menu\n"
              << "  " << prog << " --config FILE          Load config.ini\n"
              << "  " << prog << " --input FILE --batch   Full batch analysis\n"
              << "  " << prog << " --input FILE --output R [--directed] [--weighted]\n"
              << "  " << prog << " --help\n\n"
              << "Options:\n"
              << "  --input   FILE   Input CSV (required for --batch)\n"
              << "  --output  FILE   Report output file [default: report.txt]\n"
              << "  --config  FILE   Config INI file\n"
              << "  --directed       Treat graph as directed\n"
              << "  --weighted       Use third column as edge weight\n"
              << "  --batch          Run full analysis non-interactively\n"
              << "  --no-colour      Disable ANSI colours\n"
              << "  --log-level LVL  debug|info|warn|error\n"
              << "  --log-file FILE  Append logs to file\n";
}

// ─── main ────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    // ── Parse CLI args ───────────────────────────────────────
    std::string inputFile, outputFile, configFile, logFile;
    std::string logLevel = "info";
    bool isBatch = false, directed = false, weighted = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if      (a == "--help")       { printHelp(argv[0]); return 0; }
        else if (a == "--batch")      isBatch   = true;
        else if (a == "--directed")   directed  = true;
        else if (a == "--weighted")   weighted  = true;
        else if (a == "--no-colour")  { colour::enabled = false; }
        else if (a == "--input"  && i+1 < argc) inputFile  = argv[++i];
        else if (a == "--output" && i+1 < argc) outputFile = argv[++i];
        else if (a == "--config" && i+1 < argc) configFile = argv[++i];
        else if (a == "--log-file"  && i+1 < argc) logFile   = argv[++i];
        else if (a == "--log-level" && i+1 < argc) logLevel  = argv[++i];
    }

    // ── Configure logger ─────────────────────────────────────
    auto& log = Logger::instance();
    if (!logFile.empty())  log.openLogFile(logFile);
    log.enableColour(colour::enabled);
    if      (logLevel == "debug") log.setConsoleLevel(LogLevel::DEBUG);
    else if (logLevel == "warn")  log.setConsoleLevel(LogLevel::WARN);
    else if (logLevel == "error") log.setConsoleLevel(LogLevel::ERR);
    else                          log.setConsoleLevel(LogLevel::INFO);

    // ── Load config ──────────────────────────────────────────
    auto& cfg = Config::instance();
    if (!configFile.empty()) cfg.loadFile(configFile);
    else cfg.loadFile("config.ini");  // silently skipped if absent

    LOG_INFO(std::string("Network Analyzer v") + VERSION + " starting.");

    // ── Batch mode ───────────────────────────────────────────
    if (isBatch) {
        if (inputFile.empty()) {
            LOG_ERROR("--batch requires --input FILE");
            return 1;
        }
        if (outputFile.empty()) outputFile = "report.txt";
        return runBatch(inputFile, outputFile, directed, weighted);
    }

    // ── Interactive mode ─────────────────────────────────────
    printBanner();
    LOG_INFO("Interactive mode. Type '0' to exit.");

    AppState st;
    // pre-load if --input given without --batch
    if (!inputFile.empty()) {
        if (!outputFile.empty()) directed = directed, weighted = weighted;
        if (loadGraph(st, inputFile, directed, weighted, 0, 1,
                      weighted ? 2 : -1)) {
            std::cout << c::green() << "  Graph pre-loaded.\n" << c::reset();
        }
    }

    std::string choice;
    while (true) {
        printMenu(st);
        std::getline(std::cin, choice);
        choice = trim(choice);
        if (choice.empty()) continue;

        try {
            if      (choice == "1")  handleLoad(st);
            else if (choice == "2")  handleStats(st);
            else if (choice == "3")  handlePath(st);
            else if (choice == "4")  handleNodeDetail(st);
            else if (choice == "5")  handleHiddenLinks(st);
            else if (choice == "6")  handleAnomalies(st);
            else if (choice == "7")  handleCentrality(st);
            else if (choice == "8")  handleCommunity(st);
            else if (choice == "9")  handleSCC(st);
            else if (choice == "10") handleKCore(st);
            else if (choice == "11") handleTriangles(st);
            else if (choice == "12") handleNodeDetail(st);
            else if (choice == "13") handleFullReport(st);
            else if (choice == "14") handleSnapshot(st);
            else if (choice == "15") handleSettings();
            else if (choice == "0") {
                std::cout << c::bold() << c::green()
                          << "\n  Goodbye!\n\n" << c::reset();
                break;
            } else {
                std::cout << c::red() << "  Invalid option.\n" << c::reset();
            }
        } catch (const NraException& e) {
            LOG_ERROR(std::string("Error: ") + e.what());
        } catch (const std::exception& e) {
            LOG_ERROR(std::string("Unexpected error: ") + e.what());
        }
    }
    return 0;
}

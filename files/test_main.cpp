// ============================================================
//  tests/test_main.cpp  –  Unit tests (no framework required)
// ============================================================
#include "graph.hpp"
#include "algorithms.hpp"
#include "csv_parser.hpp"
#include "logger.hpp"
#include <cassert>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>

using namespace nra;
using namespace nra::algo;

static int passed = 0, failed = 0;
#define ASSERT(cond, msg) \
    do { if (cond) { ++passed; std::cout << "  PASS  " << msg << "\n"; } \
         else      { ++failed; std::cout << "  FAIL  " << msg << "\n"; } } while(0)

// ─── Helpers ─────────────────────────────────────────────────
static Graph makeSimple(bool directed = false) {
    Graph g(GraphOptions{directed, false});
    // A-B-C-D-E triangle + chain
    g.addEdge("A","B"); g.addEdge("B","C"); g.addEdge("C","A");
    g.addEdge("C","D"); g.addEdge("D","E"); g.addEdge("A","E");
    return g;
}

static Graph makeWeighted() {
    Graph g(GraphOptions{false, true});
    g.addEdge("A","B",1.0); g.addEdge("B","C",2.0);
    g.addEdge("A","C",10.0); g.addEdge("C","D",1.0);
    return g;
}

// ─── Test suites ─────────────────────────────────────────────
static void testGraph() {
    std::cout << "\n[Graph]\n";
    Graph g(GraphOptions{false,false});
    NodeId a = g.addNode("Alice");
    NodeId b = g.addNode("Bob");
    g.addEdge(a, b);

    ASSERT(g.nodeCount() == 2,        "nodeCount == 2");
    ASSERT(g.edgeCount() == 1,        "edgeCount == 1");
    ASSERT(g.degree(a) == 1,          "degree(a) == 1");
    ASSERT(g.hasEdge(a, b),           "hasEdge(a,b)");
    ASSERT(g.hasEdge(b, a),           "hasEdge(b,a) undirected");
    ASSERT(!g.hasEdge(b, b),          "no self loop");
    ASSERT(g.findNode("Alice") == a,  "findNode Alice");
    ASSERT(g.findNode("Zed") == INVALID_NODE, "findNode missing");
}

static void testBFS() {
    std::cout << "\n[BFS]\n";
    Graph g = makeSimple();
    NodeId a = g.findNode("A"), e = g.findNode("E");
    auto pr = bfs(g, a, e);
    ASSERT(pr.found,        "BFS found path A→E");
    ASSERT(pr.hops == 1,    "BFS hops A→E == 1 (direct)");

    NodeId d = g.findNode("D");
    auto pr2 = bfs(g, a, d);
    ASSERT(pr2.found,       "BFS found A→D");
    ASSERT(pr2.hops <= 3,   "BFS hops A→D <= 3");
}

static void testDijkstra() {
    std::cout << "\n[Dijkstra]\n";
    Graph g = makeWeighted();
    NodeId a = g.findNode("A"), c = g.findNode("C");
    auto pr = dijkstra(g, a, c);
    ASSERT(pr.found,                         "Dijkstra found A→C");
    ASSERT(std::fabs(pr.totalWeight-3.0)<1e-9,"Dijkstra A→C weight==3 (via B)");
    ASSERT(pr.path.size() == 3,              "Dijkstra path length 3");
}

static void testBetweenness() {
    std::cout << "\n[Betweenness]\n";
    Graph g = makeSimple();
    auto bc = betweennessCentrality(g);
    NodeId c = g.findNode("C");
    // C is a bridge to D and E
    ASSERT(!bc.empty(),        "betweenness non-empty");
    ASSERT(bc.count(c) > 0,   "betweenness has C");
    ASSERT(bc[c] >= 0,         "betweenness(C) >= 0");
}

static void testPageRank() {
    std::cout << "\n[PageRank]\n";
    Graph g = makeSimple();
    auto pr = pageRank(g);
    double sum = 0;
    for (const auto& kv : pr) sum += kv.second;
    ASSERT(std::fabs(sum - 1.0) < 0.01, "PageRank sums to ~1.0");
}

static void testLabelPropagation() {
    std::cout << "\n[Community/LabelPropagation]\n";
    Graph g = makeSimple();
    auto cr = labelPropagation(g, 50);
    ASSERT(!cr.communities.empty(),     "communities non-empty");
    ASSERT(cr.assignment.size() == g.nodeCount(), "all nodes assigned");
}

static void testTarjan() {
    std::cout << "\n[Tarjan SCC]\n";
    Graph g(GraphOptions{true, false});
    g.addEdge("A","B"); g.addEdge("B","C"); g.addEdge("C","A");
    g.addEdge("C","D"); g.addEdge("D","E");
    auto sr = tarjanSCC(g);
    ASSERT(!sr.components.empty(),     "SCCs non-empty");
    // A-B-C form a cycle → same SCC
    ASSERT(sr.components.size() >= 3,  "At least 3 SCCs");
}

static void testKCore() {
    std::cout << "\n[K-core]\n";
    Graph g = makeSimple();
    auto kr = kCoreDecomposition(g);
    ASSERT(kr.maxCore >= 1, "max core >= 1");
    ASSERT(kr.coreness.size() == g.nodeCount(), "coreness for all nodes");
}

static void testTriangles() {
    std::cout << "\n[Triangles]\n";
    Graph g(GraphOptions{false, false});
    g.addEdge("A","B"); g.addEdge("B","C"); g.addEdge("C","A");
    auto [tri, perNode] = countTriangles(g);
    ASSERT(tri == 1, "triangle count == 1 in triangle graph");
}

static void testLinkPrediction() {
    std::cout << "\n[LinkPrediction]\n";
    Graph g = makeSimple();
    auto lp = predictLinks(g, 10);
    ASSERT(!lp.empty() || g.edgeCount() > 0, "link prediction ran");
}

static void testCsvParser() {
    std::cout << "\n[CsvParser]\n";
    // write temp file
    const std::string tmpPath = "/tmp/test_nra.csv";
    {
        std::ofstream f(tmpPath);
        f << "source,target,weight\n"
          << "Alice,Bob,1.5\n"
          << "Bob,Charlie,2.0\n"
          << "\"Alice, Jr\",Dave,3.0\n"; // quoted field
    }
    CsvParser parser;
    int rowCount = 0;
    auto stats = parser.parse(tmpPath, [&](const CsvRow& r){
        ++rowCount;
        ASSERT(r.size() >= 2, "row has >= 2 fields");
    });
    ASSERT(rowCount == 3, "parsed 3 data rows");
    ASSERT(stats.malformedRows == 0, "no malformed rows");
}

static void testAnomalyDetection() {
    std::cout << "\n[AnomalyDetection]\n";
    Graph g = makeSimple();
    // add dense clique
    g.addEdge("X","Y"); g.addEdge("Y","Z"); g.addEdge("Z","X");
    g.addEdge("X","W"); g.addEdge("Y","W"); g.addEdge("Z","W");

    auto cr = labelPropagation(g, 50);
    auto ar = detectAnomalies(g, cr, 0.5, 1.5);
    ASSERT(ar.nodeAnomalyScore.size() == g.nodeCount(),
           "anomaly scores for all nodes");
}

static void testSnapshot() {
    std::cout << "\n[Snapshot]\n";
    Graph g = makeSimple();
    const std::string snap = "/tmp/test_nra.snap";
    g.saveSnapshot(snap);

    GraphOptions opts; opts.directed = false;
    Graph g2(opts);
    g2.loadSnapshot(snap);
    ASSERT(g2.nodeCount() == g.nodeCount(), "snapshot nodeCount matches");
    ASSERT(g2.edgeCount() == g.edgeCount(), "snapshot edgeCount matches");
}

// ─── main ────────────────────────────────────────────────────
int main() {
    // suppress most log output during tests
    Logger::instance().setConsoleLevel(LogLevel::WARN);

    std::cout << "=== NRA Unit Tests ===\n";

    testGraph();
    testBFS();
    testDijkstra();
    testBetweenness();
    testPageRank();
    testLabelPropagation();
    testTarjan();
    testKCore();
    testTriangles();
    testLinkPrediction();
    testCsvParser();
    testAnomalyDetection();
    testSnapshot();

    std::cout << "\n==============================\n"
              << "  Passed: " << passed << "\n"
              << "  Failed: " << failed << "\n"
              << "==============================\n";
    return (failed == 0) ? 0 : 1;
}

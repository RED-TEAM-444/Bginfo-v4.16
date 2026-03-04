#pragma once
// ============================================================
//  algorithms.hpp  –  All graph analytics
//
//  Algorithms included:
//    Path      : BFS, Dijkstra, APSP (Floyd-Warshall subset)
//    Centrality: Degree, Betweenness (Brandes), Closeness,
//                PageRank, Eigenvector (power iteration)
//    Community : Label Propagation, Louvain (greedy modularity)
//    Structure : Tarjan SCC, Triangle count, k-core decomp.
//    Prediction: Jaccard, Adamic-Adar, Resource Allocation
//    Anomaly   : Z-score degree anomaly, density-based cluster
//                suspicion scoring
// ============================================================
#include "common.hpp"
#include "graph.hpp"
#include <vector>
#include <unordered_map>
#include <optional>
#include <string>
#include <functional>

namespace nra {
namespace algo {

// ─── Result structs ──────────────────────────────────────────

struct PathResult {
    std::vector<NodeId> path;
    Weight              totalWeight { 0.0 };
    int                 hops        { 0   };
    bool                found       { false };
    double              computeMs   { 0.0 };
};

struct CentralityResult {
    std::unordered_map<NodeId, double> degree;
    std::unordered_map<NodeId, double> betweenness;
    std::unordered_map<NodeId, double> closeness;
    std::unordered_map<NodeId, double> pageRank;
    std::unordered_map<NodeId, double> eigenvector;
    double computeMs { 0.0 };
};

struct Community {
    int                  id       { -1 };
    std::vector<NodeId>  members;
    double               density  { 0.0 };
    double               modularity{ 0.0 };
};

struct CommunityResult {
    std::vector<Community>          communities;
    std::unordered_map<NodeId, int> assignment;  // node → community id
    double                          totalModularity { 0.0 };
    double                          computeMs       { 0.0 };
};

struct LinkPrediction {
    NodeId  u            { INVALID_NODE };
    NodeId  v            { INVALID_NODE };
    double  jaccard      { 0.0 };
    double  adamicAdar   { 0.0 };
    double  resAlloc     { 0.0 };
    double  combined     { 0.0 };  // weighted score
    int     commonCount  { 0   };
    std::vector<std::string> commonNames;
};

struct SuspiciousCluster {
    int                  communityId  { -1  };
    std::vector<NodeId>  members;
    double               density      { 0.0 };
    double               avgDegree    { 0.0 };
    double               suspicionScore{ 0.0};
    int                  riskLevel    { 0   };  // 0=low 1=med 2=high
    std::string          reasons;
};

struct AnomalyResult {
    std::vector<SuspiciousCluster>         clusters;
    std::unordered_map<NodeId, double>     nodeAnomalyScore;
    std::vector<NodeId>                    anomalousNodes;
    double                                 computeMs { 0.0 };
};

struct SccResult {
    std::vector<std::vector<NodeId>> components;
    std::unordered_map<NodeId, int>  assignment;
    int                              largestSize { 0 };
    double                           computeMs   { 0.0 };
};

struct KCoreResult {
    std::unordered_map<NodeId, int>  coreness;
    int                              maxCore { 0 };
    double                           computeMs { 0.0 };
};

// ─── Path algorithms ─────────────────────────────────────────

// BFS – unweighted shortest path
[[nodiscard]] PathResult bfs(const Graph& g, NodeId src, NodeId dst);

// Dijkstra – weighted shortest path (non-negative weights)
[[nodiscard]] PathResult dijkstra(const Graph& g, NodeId src, NodeId dst);

// Bellman-Ford – handles negative weights, detects negative cycles
[[nodiscard]] PathResult bellmanFord(const Graph& g, NodeId src, NodeId dst);

// All distances from source (returns map node→dist)
[[nodiscard]] std::unordered_map<NodeId, double>
    allDistances(const Graph& g, NodeId src);

// ─── Centrality ───────────────────────────────────────────────

// Compute all centralities in one pass (more efficient)
[[nodiscard]] CentralityResult
    computeAllCentrality(const Graph& g,
                         int   pageRankIter     = 100,
                         double pageRankDamping  = 0.85,
                         double pageRankTol      = 1e-6,
                         int   eigenvectorIter   = 100,
                         double eigenvectorTol   = 1e-6);

// Individual methods if needed
[[nodiscard]] std::unordered_map<NodeId, double>
    degreeCentrality(const Graph& g);
[[nodiscard]] std::unordered_map<NodeId, double>
    betweennessCentrality(const Graph& g);
[[nodiscard]] std::unordered_map<NodeId, double>
    closenessCentrality(const Graph& g);
[[nodiscard]] std::unordered_map<NodeId, double>
    pageRank(const Graph& g, int iter = 100,
             double damping = 0.85, double tol = 1e-6);
[[nodiscard]] std::unordered_map<NodeId, double>
    eigenvectorCentrality(const Graph& g,
                          int iter = 100, double tol = 1e-6);

// ─── Community Detection ────────────────────────────────────

// Label Propagation (fast, approximate)
[[nodiscard]] CommunityResult
    labelPropagation(const Graph& g, int maxIter = 100,
                     uint64_t seed = 42);

// Greedy Louvain-style modularity optimisation
[[nodiscard]] CommunityResult
    louvainCommunity(const Graph& g, int maxIter = 100,
                     uint64_t seed = 42);

// ─── Structural ──────────────────────────────────────────────

// Tarjan's strongly connected components
[[nodiscard]] SccResult
    tarjanSCC(const Graph& g);

// K-core decomposition
[[nodiscard]] KCoreResult
    kCoreDecomposition(const Graph& g);

// Triangle count (global + per node)
[[nodiscard]] std::pair<int64_t, std::unordered_map<NodeId, int>>
    countTriangles(const Graph& g);

// ─── Link Prediction ─────────────────────────────────────────

// Top-K unconnected pairs most likely to connect
[[nodiscard]] std::vector<LinkPrediction>
    predictLinks(const Graph& g, int topK = 20);

// ─── Anomaly / Suspicion ─────────────────────────────────────

// Full anomaly analysis (clusters + node-level outliers)
[[nodiscard]] AnomalyResult
    detectAnomalies(const Graph& g,
                    const CommunityResult& communities,
                    double densityThreshold = 0.55,
                    double zScoreThreshold  = 2.5);

// ─── Utility ─────────────────────────────────────────────────

// Normalise a centrality map to [0, 1]
void normalise(std::unordered_map<NodeId, double>& map);

// Compute modularity of a given partition
[[nodiscard]] double modularity(
    const Graph& g,
    const std::unordered_map<NodeId, int>& assignment);

} // namespace algo
} // namespace nra

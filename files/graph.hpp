#pragma once
// ============================================================
//  graph.hpp  –  Core directed/undirected weighted graph
//  Uses compressed adjacency list; supports:
//    • Node & edge metadata (string key-value store)
//    • Bulk construction (reserve)
//    • Serialisation  (binary snapshot + restore)
//    • Const-correct query API
// ============================================================
#include "common.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <functional>
#include <iosfwd>
#include <memory>

namespace nra {

// ── Edge ────────────────────────────────────────────────────
struct Edge {
    NodeId  to     { INVALID_NODE };
    Weight  weight { DEFAULT_W    };
    EdgeId  id     { -1           };

    bool operator==(const Edge& o) const noexcept
    { return to == o.to && id == o.id; }
};

// ── Node metadata ────────────────────────────────────────────
struct NodeMeta {
    std::string name;
    std::unordered_map<std::string, std::string> attrs;
    // internal – position in topo sort, community, etc.
    mutable int32_t  community{ -1 };
    mutable double   score    {  0.0 };
};

// ── Graph build options ──────────────────────────────────────
struct GraphOptions {
    bool   directed  { false };
    bool   weighted  { false };
    bool   allowSelf { false };  // self-loops
    bool   allowMulti{ false };  // multi-edges
    size_t reserveNodes{ 1024 };
    size_t reserveEdges{ 4096 };
};

// ── Main Graph class ─────────────────────────────────────────
class Graph {
public:
    explicit Graph(GraphOptions opts = {});

    // ── Construction ─────────────────────────────────────────
    NodeId  addNode(const std::string& name);
    NodeId  getOrAdd(const std::string& name);
    bool    addEdge(NodeId u, NodeId v, Weight w = DEFAULT_W);
    bool    addEdge(const std::string& uName,
                    const std::string& vName,
                    Weight w = DEFAULT_W);
    void    setNodeAttr(NodeId u, const std::string& key,
                        const std::string& val);
    void    reserve(size_t nodes, size_t edges);

    // ── Query ─────────────────────────────────────────────────
    [[nodiscard]] NodeId  findNode(const std::string& name) const noexcept;
    [[nodiscard]] bool    hasNode(NodeId u)                 const noexcept;
    [[nodiscard]] bool    hasEdge(NodeId u, NodeId v)       const noexcept;
    [[nodiscard]] Weight  edgeWeight(NodeId u, NodeId v)    const noexcept;
    [[nodiscard]] const NodeMeta& meta(NodeId u)            const;
    [[nodiscard]] const std::vector<Edge>& neighbours(NodeId u) const;
    [[nodiscard]] std::vector<NodeId>     neighbourIds(NodeId u) const;

    // ── Metrics ───────────────────────────────────────────────
    [[nodiscard]] int    degree    (NodeId u) const noexcept;
    [[nodiscard]] int    inDegree  (NodeId u) const noexcept;
    [[nodiscard]] int    outDegree (NodeId u) const noexcept;
    [[nodiscard]] double density   ()         const noexcept;
    [[nodiscard]] double avgDegree ()         const noexcept;

    // ── Iteration ─────────────────────────────────────────────
    void forEachNode(std::function<void(NodeId, const NodeMeta&)> fn) const;
    void forEachEdge(std::function<void(NodeId, const Edge&)>     fn) const;

    // ── Size ──────────────────────────────────────────────────
    [[nodiscard]] size_t nodeCount() const noexcept { return adj_.size(); }
    [[nodiscard]] size_t edgeCount() const noexcept { return edgeCount_; }
    [[nodiscard]] bool   empty()     const noexcept { return adj_.empty(); }
    [[nodiscard]] const GraphOptions& options() const noexcept { return opts_; }
    [[nodiscard]] std::vector<NodeId> allNodes() const;

    // ── Serialisation ─────────────────────────────────────────
    void saveSnapshot(const std::string& path) const;
    void loadSnapshot(const std::string& path);

    // ── Statistics dump ───────────────────────────────────────
    void printSummary(std::ostream& os) const;

private:
    GraphOptions opts_;
    // adjacency list
    std::unordered_map<NodeId, std::vector<Edge>>  adj_;
    // reverse edges (for directed in-degree)
    std::unordered_map<NodeId, std::vector<Edge>>  radj_;
    // node metadata
    std::unordered_map<NodeId, NodeMeta>           meta_;
    // name → id mapping
    std::unordered_map<std::string, NodeId>        nameIdx_;

    size_t edgeCount_ { 0 };
    NodeId nextId_    { 0 };
    EdgeId nextEid_   { 0 };
};

} // namespace nra

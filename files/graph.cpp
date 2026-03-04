// ============================================================
//  graph.cpp  –  Core graph implementation
// ============================================================
#include "graph.hpp"
#include "logger.hpp"
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <ostream>
#include <iostream>

namespace nra {

Graph::Graph(GraphOptions opts) : opts_(std::move(opts)) {
    adj_.reserve(opts_.reserveNodes);
    meta_.reserve(opts_.reserveNodes);
    nameIdx_.reserve(opts_.reserveNodes);
}

NodeId Graph::addNode(const std::string& name) {
    auto it = nameIdx_.find(name);
    if (it != nameIdx_.end()) return it->second;
    NodeId id = nextId_++;
    nameIdx_[name] = id;
    meta_[id].name = name;
    adj_[id];   // ensure entry even with 0 edges
    if (opts_.directed) radj_[id];
    return id;
}

NodeId Graph::getOrAdd(const std::string& name) {
    return addNode(name);
}

bool Graph::addEdge(NodeId u, NodeId v, Weight w) {
    if (!hasNode(u) || !hasNode(v)) {
        LOG_WARN("addEdge: unknown node id");
        return false;
    }
    if (!opts_.allowSelf && u == v) return false;
    if (!opts_.allowMulti && hasEdge(u, v)) return false;

    EdgeId eid = nextEid_++;
    adj_[u].push_back({v, w, eid});
    if (opts_.directed) {
        radj_[v].push_back({u, w, eid});
    } else {
        adj_[v].push_back({u, w, eid});
    }
    ++edgeCount_;
    return true;
}

bool Graph::addEdge(const std::string& uName,
                    const std::string& vName, Weight w) {
    return addEdge(getOrAdd(uName), getOrAdd(vName), w);
}

void Graph::setNodeAttr(NodeId u, const std::string& key,
                         const std::string& val) {
    auto it = meta_.find(u);
    if (it != meta_.end()) it->second.attrs[key] = val;
}

void Graph::reserve(size_t nodes, size_t edges) {
    adj_.reserve(nodes);
    meta_.reserve(nodes);
    nameIdx_.reserve(nodes);
    (void)edges; // per-node vectors auto-grow
}

NodeId Graph::findNode(const std::string& name) const noexcept {
    auto it = nameIdx_.find(name);
    return (it == nameIdx_.end()) ? INVALID_NODE : it->second;
}

bool Graph::hasNode(NodeId u) const noexcept {
    return adj_.count(u) > 0;
}

bool Graph::hasEdge(NodeId u, NodeId v) const noexcept {
    auto it = adj_.find(u);
    if (it == adj_.end()) return false;
    for (const auto& e : it->second)
        if (e.to == v) return true;
    return false;
}

Weight Graph::edgeWeight(NodeId u, NodeId v) const noexcept {
    auto it = adj_.find(u);
    if (it == adj_.end()) return INF_WEIGHT;
    for (const auto& e : it->second)
        if (e.to == v) return e.weight;
    return INF_WEIGHT;
}

const NodeMeta& Graph::meta(NodeId u) const {
    auto it = meta_.find(u);
    if (it == meta_.end())
        throw AlgorithmException("meta(): node " + std::to_string(u) + " not found");
    return it->second;
}

const std::vector<Edge>& Graph::neighbours(NodeId u) const {
    auto it = adj_.find(u);
    if (it == adj_.end())
        throw AlgorithmException("neighbours(): node " + std::to_string(u) + " not found");
    return it->second;
}

std::vector<NodeId> Graph::neighbourIds(NodeId u) const {
    std::vector<NodeId> ids;
    auto it = adj_.find(u);
    if (it == adj_.end()) return ids;
    ids.reserve(it->second.size());
    for (const auto& e : it->second) ids.push_back(e.to);
    return ids;
}

int Graph::degree(NodeId u) const noexcept {
    auto it = adj_.find(u);
    return (it == adj_.end()) ? 0 : (int)it->second.size();
}

int Graph::inDegree(NodeId u) const noexcept {
    if (!opts_.directed) return degree(u);
    auto it = radj_.find(u);
    return (it == radj_.end()) ? 0 : (int)it->second.size();
}

int Graph::outDegree(NodeId u) const noexcept {
    return degree(u);
}

double Graph::density() const noexcept {
    size_t n = nodeCount();
    if (n < 2) return 0.0;
    double maxE = opts_.directed
                  ? (double)(n * (n - 1))
                  : (double)(n * (n - 1)) / 2.0;
    return (maxE > 0) ? (double)edgeCount_ / maxE : 0.0;
}

double Graph::avgDegree() const noexcept {
    if (adj_.empty()) return 0.0;
    double sum = 0;
    for (const auto& kv : adj_) sum += kv.second.size();
    return sum / adj_.size();
}

void Graph::forEachNode(
    std::function<void(NodeId, const NodeMeta&)> fn) const {
    for (const auto& kv : meta_) fn(kv.first, kv.second);
}

void Graph::forEachEdge(
    std::function<void(NodeId, const Edge&)> fn) const {
    for (const auto& kv : adj_)
        for (const auto& e : kv.second)
            fn(kv.first, e);
}

std::vector<NodeId> Graph::allNodes() const {
    std::vector<NodeId> ids;
    ids.reserve(adj_.size());
    for (const auto& kv : adj_) ids.push_back(kv.first);
    return ids;
}

// ── Serialisation ─────────────────────────────────────────────
// Simple binary format:
//  magic(4B) | nodeCount(4B) | edgeCount(8B)
//  for each node: id(4B) | nameLen(4B) | name(N)
//  for each edge: u(4B) | v(4B) | weight(8B)

void Graph::saveSnapshot(const std::string& path) const {
    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) throw IoException("Cannot write snapshot: " + path);

    const char magic[4] = {'N','R','A','1'};
    f.write(magic, 4);
    uint32_t nc = (uint32_t)nodeCount();
    uint64_t ec = (uint64_t)edgeCount_;
    f.write(reinterpret_cast<const char*>(&nc), 4);
    f.write(reinterpret_cast<const char*>(&ec), 8);

    // nodes
    for (const auto& kv : meta_) {
        uint32_t id  = (uint32_t)kv.first;
        uint32_t nlen= (uint32_t)kv.second.name.size();
        f.write(reinterpret_cast<const char*>(&id),   4);
        f.write(reinterpret_cast<const char*>(&nlen), 4);
        f.write(kv.second.name.data(), nlen);
    }
    // edges
    for (const auto& kv : adj_) {
        uint32_t u = (uint32_t)kv.first;
        for (const auto& e : kv.second) {
            uint32_t v = (uint32_t)e.to;
            double   w = e.weight;
            f.write(reinterpret_cast<const char*>(&u), 4);
            f.write(reinterpret_cast<const char*>(&v), 4);
            f.write(reinterpret_cast<const char*>(&w), 8);
        }
    }
    LOG_INFO("Snapshot saved: " + path);
}

void Graph::loadSnapshot(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) throw IoException("Cannot read snapshot: " + path);

    char magic[4]; f.read(magic, 4);
    if (magic[0]!='N'||magic[1]!='R'||magic[2]!='A'||magic[3]!='1')
        throw IoException("Invalid snapshot magic: " + path);

    uint32_t nc; uint64_t ec;
    f.read(reinterpret_cast<char*>(&nc), 4);
    f.read(reinterpret_cast<char*>(&ec), 8);
    reserve(nc + 64, ec + 64);

    for (uint32_t i = 0; i < nc; ++i) {
        uint32_t id, nlen;
        f.read(reinterpret_cast<char*>(&id),   4);
        f.read(reinterpret_cast<char*>(&nlen), 4);
        std::string name(nlen, '\0');
        f.read(&name[0], nlen);
        nameIdx_[name] = (NodeId)id;
        meta_[(NodeId)id].name = name;
        adj_[(NodeId)id];
        if ((NodeId)id >= nextId_) nextId_ = (NodeId)id + 1;
    }
    for (uint64_t i = 0; i < ec; ++i) {
        uint32_t u, v; double w;
        f.read(reinterpret_cast<char*>(&u), 4);
        f.read(reinterpret_cast<char*>(&v), 4);
        f.read(reinterpret_cast<char*>(&w), 8);
        EdgeId eid = nextEid_++;
        adj_[(NodeId)u].push_back({(NodeId)v, w, eid});
        ++edgeCount_;
    }
    LOG_INFO("Snapshot loaded: " + path +
             " (" + std::to_string(nc) + " nodes, " +
             std::to_string(ec) + " edges)");
}

void Graph::printSummary(std::ostream& os) const {
    os << "  Nodes    : " << nodeCount()  << "\n"
       << "  Edges    : " << edgeCount()  << "\n"
       << "  Directed : " << (opts_.directed ? "Yes" : "No") << "\n"
       << "  Weighted : " << (opts_.weighted ? "Yes" : "No") << "\n"
       << "  Density  : " << std::fixed << density()    << "\n"
       << "  Avg Deg  : " << avgDegree() << "\n";
}

} // namespace nra

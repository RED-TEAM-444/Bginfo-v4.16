// ============================================================
//  algorithms.cpp  –  Full graph analytics engine
// ============================================================
#include "algorithms.hpp"
#include "logger.hpp"
#include <queue>
#include <stack>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <random>
#include <unordered_set>
#include <functional>
#include <cassert>

namespace nra {
namespace algo {

// ─── Utility ─────────────────────────────────────────────────
void normalise(std::unordered_map<NodeId, double>& map) {
    double mx = 0;
    for (const auto& kv : map) if (kv.second > mx) mx = kv.second;
    if (mx < 1e-12) return;
    for (auto& kv : map) kv.second /= mx;
}

static double safeDiv(double a, double b) {
    return (std::fabs(b) < 1e-15) ? 0.0 : a / b;
}

// ─── BFS ─────────────────────────────────────────────────────
PathResult bfs(const Graph& g, NodeId src, NodeId dst) {
    Timer t;
    if (!g.hasNode(src) || !g.hasNode(dst))
        return {{}, 0.0, 0, false};
    if (src == dst)
        return {{src}, 0.0, 0, true, t.elapsedMs()};

    std::unordered_map<NodeId, NodeId> prev;
    std::unordered_map<NodeId, int>    dist;
    std::queue<NodeId> q;
    q.push(src);
    dist[src] = 0;
    prev[src]  = INVALID_NODE;

    while (!q.empty()) {
        NodeId cur = q.front(); q.pop();
        if (cur == dst) break;
        for (const auto& e : g.neighbours(cur)) {
            if (!dist.count(e.to)) {
                dist[e.to] = dist[cur] + 1;
                prev[e.to] = cur;
                q.push(e.to);
            }
        }
    }
    if (!dist.count(dst))
        return {{}, 0.0, 0, false, t.elapsedMs()};

    std::vector<NodeId> path;
    for (NodeId c = dst; c != INVALID_NODE; c = prev[c])
        path.push_back(c);
    std::reverse(path.begin(), path.end());
    return {path, (double)dist[dst], dist[dst], true, t.elapsedMs()};
}

// ─── Dijkstra ────────────────────────────────────────────────
PathResult dijkstra(const Graph& g, NodeId src, NodeId dst) {
    Timer t;
    if (!g.hasNode(src) || !g.hasNode(dst))
        return {{}, 0.0, 0, false};
    if (src == dst)
        return {{src}, 0.0, 0, true, t.elapsedMs()};

    std::unordered_map<NodeId, double>  d;
    std::unordered_map<NodeId, NodeId>  prev;
    auto nodes = g.allNodes();
    for (NodeId n : nodes) { d[n] = INF_WEIGHT; prev[n] = INVALID_NODE; }
    d[src] = 0.0;

    using pdi = std::pair<double, NodeId>;
    std::priority_queue<pdi, std::vector<pdi>, std::greater<pdi>> pq;
    pq.push({0.0, src});

    while (!pq.empty()) {
        auto [dd, u] = pq.top(); pq.pop();
        if (dd > d[u] + 1e-12) continue;
        if (u == dst) break;
        for (const auto& e : g.neighbours(u)) {
            double nd = d[u] + e.weight;
            if (nd < d[e.to] - 1e-12) {
                d[e.to]    = nd;
                prev[e.to] = u;
                pq.push({nd, e.to});
            }
        }
    }
    if (d[dst] >= INF_WEIGHT / 2)
        return {{}, 0.0, 0, false, t.elapsedMs()};

    std::vector<NodeId> path;
    for (NodeId c = dst; c != INVALID_NODE; c = prev[c])
        path.push_back(c);
    std::reverse(path.begin(), path.end());
    return {path, d[dst], (int)path.size()-1, true, t.elapsedMs()};
}

// ─── Bellman-Ford ────────────────────────────────────────────
PathResult bellmanFord(const Graph& g, NodeId src, NodeId dst) {
    Timer t;
    if (!g.hasNode(src) || !g.hasNode(dst))
        return {{}, 0.0, 0, false};

    auto nodes = g.allNodes();
    std::unordered_map<NodeId, double>  d;
    std::unordered_map<NodeId, NodeId>  prev;
    for (NodeId n : nodes) { d[n] = INF_WEIGHT; prev[n] = INVALID_NODE; }
    d[src] = 0.0;

    size_t N = nodes.size();
    bool updated = false;
    for (size_t iter = 0; iter < N - 1; ++iter) {
        updated = false;
        g.forEachEdge([&](NodeId u, const Edge& e){
            if (d[u] < INF_WEIGHT / 2) {
                double nd = d[u] + e.weight;
                if (nd < d[e.to] - 1e-12) {
                    d[e.to] = nd; prev[e.to] = u; updated = true;
                }
            }
        });
        if (!updated) break;
    }
    // negative cycle check
    g.forEachEdge([&](NodeId u, const Edge& e){
        if (d[u] < INF_WEIGHT/2 && d[u] + e.weight < d[e.to] - 1e-12)
            throw AlgorithmException("Bellman-Ford: negative cycle detected");
    });

    if (d[dst] >= INF_WEIGHT / 2)
        return {{}, 0.0, 0, false, t.elapsedMs()};

    std::vector<NodeId> path;
    for (NodeId c = dst; c != INVALID_NODE; c = prev[c])
        path.push_back(c);
    std::reverse(path.begin(), path.end());
    return {path, d[dst], (int)path.size()-1, true, t.elapsedMs()};
}

// ─── All distances from source (BFS / Dijkstra) ──────────────
std::unordered_map<NodeId, double>
allDistances(const Graph& g, NodeId src) {
    std::unordered_map<NodeId, double> d;
    if (!g.hasNode(src)) return d;

    if (g.options().weighted) {
        auto nodes = g.allNodes();
        for (NodeId n : nodes) d[n] = INF_WEIGHT;
        d[src] = 0.0;
        using pdi = std::pair<double, NodeId>;
        std::priority_queue<pdi, std::vector<pdi>, std::greater<pdi>> pq;
        pq.push({0.0, src});
        while (!pq.empty()) {
            auto [dd, u] = pq.top(); pq.pop();
            if (dd > d[u] + 1e-12) continue;
            for (const auto& e : g.neighbours(u)) {
                double nd = d[u] + e.weight;
                if (nd < d[e.to]) { d[e.to] = nd; pq.push({nd, e.to}); }
            }
        }
    } else {
        std::queue<NodeId> q;
        q.push(src); d[src] = 0;
        while (!q.empty()) {
            NodeId u = q.front(); q.pop();
            for (const auto& e : g.neighbours(u)) {
                if (!d.count(e.to)) {
                    d[e.to] = d[u] + 1;
                    q.push(e.to);
                }
            }
        }
    }
    return d;
}

// ─── Degree Centrality ───────────────────────────────────────
std::unordered_map<NodeId, double>
degreeCentrality(const Graph& g) {
    std::unordered_map<NodeId, double> dc;
    double denom = (g.nodeCount() > 1) ? (double)(g.nodeCount() - 1) : 1.0;
    g.forEachNode([&](NodeId u, const NodeMeta&){
        dc[u] = safeDiv((double)g.degree(u), denom);
    });
    return dc;
}

// ─── Betweenness Centrality (Brandes 2001) ───────────────────
std::unordered_map<NodeId, double>
betweennessCentrality(const Graph& g) {
    Timer t;
    auto nodes = g.allNodes();
    std::unordered_map<NodeId, double> bc;
    for (NodeId n : nodes) bc[n] = 0.0;

    for (NodeId s : nodes) {
        std::unordered_map<NodeId, std::vector<NodeId>> pred;
        std::unordered_map<NodeId, double>  sigma;
        std::unordered_map<NodeId, int>     dist2;
        std::unordered_map<NodeId, double>  delta;
        for (NodeId n : nodes) {
            sigma[n] = 0; dist2[n] = -1; delta[n] = 0;
        }
        sigma[s] = 1; dist2[s] = 0;
        std::queue<NodeId> q;
        q.push(s);
        std::vector<NodeId> stack;

        while (!q.empty()) {
            NodeId v = q.front(); q.pop();
            stack.push_back(v);
            for (const auto& e : g.neighbours(v)) {
                NodeId w = e.to;
                if (dist2[w] < 0) {
                    q.push(w);
                    dist2[w] = dist2[v] + 1;
                }
                if (dist2[w] == dist2[v] + 1) {
                    sigma[w] += sigma[v];
                    pred[w].push_back(v);
                }
            }
        }
        while (!stack.empty()) {
            NodeId w = stack.back(); stack.pop_back();
            for (NodeId v : pred[w]) {
                delta[v] += safeDiv(sigma[v], sigma[w]) * (1.0 + delta[w]);
            }
            if (w != s) bc[w] += delta[w];
        }
    }
    if (!g.options().directed)
        for (auto& kv : bc) kv.second /= 2.0;

    LOG_DEBUG("Betweenness centrality: " + std::to_string(t.elapsedMs()) + "ms");
    return bc;
}

// ─── Closeness Centrality ────────────────────────────────────
std::unordered_map<NodeId, double>
closenessCentrality(const Graph& g) {
    std::unordered_map<NodeId, double> cc;
    auto nodes = g.allNodes();
    for (NodeId src : nodes) {
        auto d   = allDistances(g, src);
        double sum = 0; int reach = 0;
        for (const auto& kv : d) {
            if (kv.first != src && kv.second < INF_WEIGHT / 2) {
                sum += kv.second; ++reach;
            }
        }
        // Wasserman-Faust normalisation for disconnected graphs
        double n1 = (double)(g.nodeCount() - 1);
        cc[src] = (sum > 1e-12)
                  ? safeDiv((double)reach, n1) * safeDiv((double)reach, sum)
                  : 0.0;
    }
    return cc;
}

// ─── PageRank ────────────────────────────────────────────────
std::unordered_map<NodeId, double>
pageRank(const Graph& g, int maxIter, double damping, double tol) {
    Timer t;
    auto nodes = g.allNodes();
    size_t N   = nodes.size();
    if (N == 0) return {};

    std::unordered_map<NodeId, double> pr, prNew;
    double init = 1.0 / (double)N;
    for (NodeId n : nodes) pr[n] = init;

    // dangling node handling
    for (int iter = 0; iter < maxIter; ++iter) {
        double dangle = 0.0;
        for (NodeId u : nodes)
            if (g.degree(u) == 0) dangle += pr[u];

        double base = (1.0 - damping) / (double)N + damping * dangle / (double)N;
        for (NodeId v : nodes) prNew[v] = base;

        g.forEachEdge([&](NodeId u, const Edge& e){
            prNew[e.to] += damping * safeDiv(pr[u], (double)g.degree(u));
        });

        double diff = 0;
        for (NodeId n : nodes) diff += std::fabs(prNew[n] - pr[n]);
        pr = prNew;
        if (diff < tol) break;
    }
    LOG_DEBUG("PageRank: " + std::to_string(t.elapsedMs()) + "ms");
    return pr;
}

// ─── Eigenvector Centrality (power iteration) ────────────────
std::unordered_map<NodeId, double>
eigenvectorCentrality(const Graph& g, int maxIter, double tol) {
    Timer t;
    auto nodes = g.allNodes();
    if (nodes.empty()) return {};

    std::unordered_map<NodeId, double> ev, evNew;
    for (NodeId n : nodes) ev[n] = 1.0;

    for (int iter = 0; iter < maxIter; ++iter) {
        for (NodeId n : nodes) evNew[n] = 0.0;
        g.forEachEdge([&](NodeId u, const Edge& e){
            evNew[e.to] += ev[u];
        });
        // normalise
        double norm = 0;
        for (const auto& kv : evNew) norm += kv.second * kv.second;
        norm = std::sqrt(norm);
        if (norm < 1e-12) break;
        double diff = 0;
        for (NodeId n : nodes) {
            evNew[n] /= norm;
            diff += std::fabs(evNew[n] - ev[n]);
        }
        ev = evNew;
        if (diff < tol) break;
    }
    LOG_DEBUG("Eigenvector centrality: " + std::to_string(t.elapsedMs()) + "ms");
    return ev;
}

// ─── All centrality combined ──────────────────────────────────
CentralityResult computeAllCentrality(const Graph& g,
    int prIter, double prDamp, double prTol,
    int eigIter, double eigTol) {
    Timer t;
    CentralityResult cr;
    LOG_INFO("Computing degree centrality...");
    cr.degree       = degreeCentrality(g);
    LOG_INFO("Computing betweenness centrality (Brandes)...");
    cr.betweenness  = betweennessCentrality(g);
    LOG_INFO("Computing closeness centrality...");
    cr.closeness    = closenessCentrality(g);
    LOG_INFO("Computing PageRank...");
    cr.pageRank     = pageRank(g, prIter, prDamp, prTol);
    LOG_INFO("Computing eigenvector centrality...");
    cr.eigenvector  = eigenvectorCentrality(g, eigIter, eigTol);
    cr.computeMs    = t.elapsedMs();
    LOG_INFO("All centrality done in " + std::to_string(cr.computeMs) + "ms");
    return cr;
}

// ─── Modularity ───────────────────────────────────────────────
double modularity(const Graph& g,
                  const std::unordered_map<NodeId, int>& asgn) {
    double m = (double)g.edgeCount();
    if (m < 1e-9) return 0.0;
    double Q = 0.0;
    auto nodes = g.allNodes();
    for (NodeId u : nodes) {
        double ku = (double)g.degree(u);
        for (NodeId v : nodes) {
            double kv = (double)g.degree(v);
            double aij = g.hasEdge(u,v) ? 1.0 : 0.0;
            if (asgn.at(u) == asgn.at(v))
                Q += aij - ku * kv / (2.0 * m);
        }
    }
    return Q / (2.0 * m);
}

// ─── Label Propagation ───────────────────────────────────────
CommunityResult labelPropagation(const Graph& g, int maxIter, uint64_t seed) {
    Timer t;
    auto nodes = g.allNodes();
    std::unordered_map<NodeId, int> label;
    int li = 0;
    for (NodeId n : nodes) label[n] = li++;

    std::mt19937_64 rng(seed);
    bool changed = true;
    for (int iter = 0; iter < maxIter && changed; ++iter) {
        changed = false;
        std::shuffle(nodes.begin(), nodes.end(), rng);
        for (NodeId u : nodes) {
            const auto& nbrs = g.neighbours(u);
            if (nbrs.empty()) continue;
            std::unordered_map<int,double> freq;
            for (const auto& e : nbrs)
                freq[label[e.to]] += e.weight;
            int best = label[u]; double bestW = -1;
            for (const auto& kv : freq) {
                if (kv.second > bestW) { bestW = kv.second; best = kv.first; }
            }
            if (best != label[u]) { label[u] = best; changed = true; }
        }
    }

    // group members
    std::unordered_map<int, Community> cmap;
    for (NodeId n : nodes) {
        int c = label[n];
        if (!cmap.count(c)) { cmap[c].id = c; }
        cmap[c].members.push_back(n);
    }

    CommunityResult cr;
    cr.assignment = label;
    // compute per-community density
    for (auto& [id, comm] : cmap) {
        std::unordered_set<NodeId> ms(comm.members.begin(), comm.members.end());
        int inEdges = 0;
        for (NodeId u : comm.members)
            for (const auto& e : g.neighbours(u))
                if (ms.count(e.to)) ++inEdges;
        if (!g.options().directed) inEdges /= 2;
        size_t n = comm.members.size();
        size_t maxE = (g.options().directed) ? n*(n-1) : n*(n-1)/2;
        comm.density = (maxE > 0) ? (double)inEdges / maxE : 0.0;
        cr.communities.push_back(std::move(comm));
    }
    std::sort(cr.communities.begin(), cr.communities.end(),
        [](const Community& a, const Community& b){
            return a.members.size() > b.members.size();
        });
    cr.totalModularity = modularity(g, label);
    cr.computeMs       = t.elapsedMs();
    LOG_INFO("Label propagation: " +
             std::to_string(cr.communities.size()) + " communities, Q=" +
             std::to_string(cr.totalModularity) +
             " in " + std::to_string(cr.computeMs) + "ms");
    return cr;
}

// ─── Greedy Louvain-style ────────────────────────────────────
CommunityResult louvainCommunity(const Graph& g, int maxIter, uint64_t seed) {
    // Simplified single-pass greedy: start from LP result, then
    // greedily merge communities that improve modularity.
    Timer t;
    auto cr = labelPropagation(g, maxIter, seed);  // warm start
    double bestQ = cr.totalModularity;
    auto bestAsgn = cr.assignment;

    auto nodes = g.allNodes();
    std::mt19937_64 rng(seed);
    bool improved = true;
    for (int pass = 0; pass < maxIter && improved; ++pass) {
        improved = false;
        std::shuffle(nodes.begin(), nodes.end(), rng);
        for (NodeId u : nodes) {
            int origComm = bestAsgn[u];
            // collect neighbour communities
            std::unordered_map<int, double> nbComm;
            for (const auto& e : g.neighbours(u))
                nbComm[bestAsgn[e.to]] += e.weight;
            if (nbComm.empty()) continue;
            int bestComm  = origComm;
            double bestDelta = 0.0;
            for (const auto& [c, w] : nbComm) {
                if (c == origComm) continue;
                bestAsgn[u] = c;
                double Q = modularity(g, bestAsgn);
                double delta = Q - bestQ;
                if (delta > bestDelta) { bestDelta = delta; bestComm = c; }
                bestAsgn[u] = origComm; // restore
            }
            if (bestComm != origComm) {
                bestAsgn[u] = bestComm;
                bestQ += bestDelta;
                improved = true;
            }
        }
    }

    // rebuild community list
    std::unordered_map<int, Community> cmap;
    for (NodeId n : nodes) {
        int c = bestAsgn[n];
        if (!cmap.count(c)) cmap[c].id = c;
        cmap[c].members.push_back(n);
    }
    cr.communities.clear();
    for (auto& [id, comm] : cmap) {
        std::unordered_set<NodeId> ms(comm.members.begin(), comm.members.end());
        int inEdges = 0;
        for (NodeId u : comm.members)
            for (const auto& e : g.neighbours(u))
                if (ms.count(e.to)) ++inEdges;
        if (!g.options().directed) inEdges /= 2;
        size_t n2 = comm.members.size();
        size_t maxE = g.options().directed ? n2*(n2-1) : n2*(n2-1)/2;
        comm.density = (maxE>0) ? (double)inEdges/maxE : 0.0;
        comm.modularity = bestQ;
        cr.communities.push_back(std::move(comm));
    }
    std::sort(cr.communities.begin(), cr.communities.end(),
        [](const Community& a, const Community& b){
            return a.members.size() > b.members.size(); });
    cr.assignment      = bestAsgn;
    cr.totalModularity = bestQ;
    cr.computeMs       = t.elapsedMs();
    LOG_INFO("Louvain: " + std::to_string(cr.communities.size()) +
             " communities, Q=" + std::to_string(cr.totalModularity) +
             " in " + std::to_string(cr.computeMs) + "ms");
    return cr;
}

// ─── Tarjan SCC ───────────────────────────────────────────────
SccResult tarjanSCC(const Graph& g) {
    Timer t;
    SccResult sr;
    auto nodes = g.allNodes();
    std::unordered_map<NodeId, int>  index, lowlink;
    std::unordered_map<NodeId, bool> onStack;
    std::stack<NodeId> stk;
    int idx = 0;
    int comp = 0;

    std::function<void(NodeId)> strongConnect = [&](NodeId v) {
        index[v] = lowlink[v] = idx++;
        stk.push(v); onStack[v] = true;

        for (const auto& e : g.neighbours(v)) {
            NodeId w = e.to;
            if (!index.count(w)) {
                strongConnect(w);
                lowlink[v] = std::min(lowlink[v], lowlink[w]);
            } else if (onStack[w]) {
                lowlink[v] = std::min(lowlink[v], index[w]);
            }
        }
        if (lowlink[v] == index[v]) {
            std::vector<NodeId> scc;
            while (true) {
                NodeId w = stk.top(); stk.pop();
                onStack[w] = false;
                scc.push_back(w);
                sr.assignment[w] = comp;
                if (w == v) break;
            }
            sr.largestSize = std::max(sr.largestSize, (int)scc.size());
            sr.components.push_back(std::move(scc));
            ++comp;
        }
    };
    for (NodeId n : nodes)
        if (!index.count(n)) strongConnect(n);

    std::sort(sr.components.begin(), sr.components.end(),
        [](const auto& a, const auto& b){ return a.size() > b.size(); });
    sr.computeMs = t.elapsedMs();
    LOG_INFO("Tarjan SCC: " + std::to_string(sr.components.size()) +
             " components in " + std::to_string(sr.computeMs) + "ms");
    return sr;
}

// ─── K-core Decomposition ────────────────────────────────────
KCoreResult kCoreDecomposition(const Graph& g) {
    Timer t;
    KCoreResult kr;
    auto nodes = g.allNodes();

    std::unordered_map<NodeId, int> deg;
    for (NodeId n : nodes) deg[n] = g.degree(n);

    // Batagelj-Zaversnik
    std::vector<NodeId> sorted = nodes;
    std::sort(sorted.begin(), sorted.end(),
        [&](NodeId a, NodeId b){ return deg[a] < deg[b]; });

    std::unordered_map<NodeId, bool> removed;
    for (NodeId v : sorted) {
        kr.coreness[v] = deg[v];
        if (deg[v] > kr.maxCore) kr.maxCore = deg[v];
        removed[v] = true;
        for (const auto& e : g.neighbours(v)) {
            if (!removed[e.to]) {
                deg[e.to] = std::max(0, deg[e.to] - 1);
                if (deg[e.to] < kr.coreness[e.to])
                    kr.coreness[e.to] = std::max(0, deg[e.to]);
            }
        }
    }
    kr.computeMs = t.elapsedMs();
    return kr;
}

// ─── Triangle counting ────────────────────────────────────────
std::pair<int64_t, std::unordered_map<NodeId, int>>
countTriangles(const Graph& g) {
    int64_t global = 0;
    std::unordered_map<NodeId, int> perNode;
    auto nodes = g.allNodes();
    for (NodeId n : nodes) perNode[n] = 0;

    // Build adjacency sets
    std::unordered_map<NodeId, std::unordered_set<NodeId>> nbSet;
    for (NodeId u : nodes)
        for (const auto& e : g.neighbours(u))
            nbSet[u].insert(e.to);

    for (NodeId u : nodes) {
        for (const auto& e : g.neighbours(u)) {
            NodeId v = e.to;
            if (v <= u) continue;
            for (NodeId w : nbSet[u]) {
                if (w <= v) continue;
                if (nbSet[v].count(w)) {
                    ++global;
                    ++perNode[u]; ++perNode[v]; ++perNode[w];
                }
            }
        }
    }
    return {global, perNode};
}

// ─── Link Prediction ─────────────────────────────────────────
std::vector<LinkPrediction> predictLinks(const Graph& g, int topK) {
    Timer t;
    auto nodes = g.allNodes();

    // neighbour sets
    std::unordered_map<NodeId, std::unordered_set<NodeId>> nbSet;
    for (NodeId u : nodes)
        for (const auto& e : g.neighbours(u))
            nbSet[u].insert(e.to);

    std::vector<LinkPrediction> results;
    for (size_t i = 0; i < nodes.size(); ++i) {
        NodeId u = nodes[i];
        for (size_t j = i+1; j < nodes.size(); ++j) {
            NodeId v = nodes[j];
            if (g.hasEdge(u,v) || g.hasEdge(v,u)) continue;

            // Common neighbours
            std::vector<NodeId> common;
            for (NodeId n : nbSet[u])
                if (nbSet[v].count(n) && n != u && n != v)
                    common.push_back(n);
            if (common.empty()) continue;

            // Jaccard
            size_t un = nbSet[u].size(), vn = nbSet[v].size();
            size_t inter = common.size();
            size_t uni   = un + vn - inter;
            double jaccard = (uni > 0) ? (double)inter / uni : 0.0;

            // Adamic-Adar
            double aa = 0.0;
            for (NodeId c : common) {
                double dc = (double)g.degree(c);
                if (dc > 1.0) aa += 1.0 / std::log(dc);
            }

            // Resource Allocation
            double ra = 0.0;
            for (NodeId c : common) {
                double dc = (double)g.degree(c);
                if (dc > 0) ra += 1.0 / dc;
            }

            // Combined (weighted)
            double combined = 0.4*jaccard + 0.35*(aa / std::max(1.0,(double)g.nodeCount()))
                              + 0.25*ra;

            std::vector<std::string> commonNames;
            for (NodeId c : common)
                commonNames.push_back(g.meta(c).name);

            results.push_back({u, v, jaccard, aa, ra, combined,
                               (int)inter, commonNames});
        }
    }
    std::sort(results.begin(), results.end(),
        [](const LinkPrediction& a, const LinkPrediction& b){
            return a.combined > b.combined; });
    if ((int)results.size() > topK) results.resize(topK);
    LOG_INFO("Link prediction: " + std::to_string(results.size()) +
             " candidates in " + std::to_string(t.elapsedMs()) + "ms");
    return results;
}

// ─── Anomaly Detection ────────────────────────────────────────
AnomalyResult detectAnomalies(const Graph& g,
    const CommunityResult& communities,
    double densityThresh, double zScoreThresh) {
    Timer t;
    AnomalyResult ar;

    // ── Node-level anomaly: Z-score on degree ────────────────
    auto nodes = g.allNodes();
    double mean = 0, var = 0;
    for (NodeId n : nodes) mean += g.degree(n);
    mean /= std::max((size_t)1, nodes.size());
    for (NodeId n : nodes) {
        double d = g.degree(n) - mean;
        var += d * d;
    }
    var = (nodes.size() > 1) ? var / (nodes.size() - 1) : 0;
    double stddev = std::sqrt(var);

    for (NodeId n : nodes) {
        double z = (stddev > 1e-9)
                   ? std::fabs((double)g.degree(n) - mean) / stddev
                   : 0.0;
        ar.nodeAnomalyScore[n] = z;
        if (z > zScoreThresh) ar.anomalousNodes.push_back(n);
    }
    std::sort(ar.anomalousNodes.begin(), ar.anomalousNodes.end(),
        [&](NodeId a, NodeId b){
            return ar.nodeAnomalyScore[a] > ar.nodeAnomalyScore[b]; });

    // ── Cluster-level anomaly ─────────────────────────────────
    double globalAvgDeg = g.avgDegree();

    for (const auto& comm : communities.communities) {
        if (comm.members.size() < 3) continue;

        double score = 0.0;
        std::string reasons;

        // High density
        if (comm.density >= densityThresh) {
            score += comm.density * 50.0;
            reasons += "Dense (" + std::to_string((int)(comm.density*100)) + "%). ";
        }
        // Near-clique
        if (comm.members.size() >= 4 && comm.density >= 0.8) {
            score += 20.0;
            reasons += "Near-clique. ";
        }
        // High avg degree vs global
        double avgDeg = 0;
        for (NodeId n : comm.members) avgDeg += g.degree(n);
        avgDeg /= comm.members.size();
        if (avgDeg > globalAvgDeg * 2.5) {
            score += 30.0;
            reasons += "Avg-deg " + std::to_string((int)avgDeg) +
                       " >> global " + std::to_string((int)globalAvgDeg) + ". ";
        }
        // Anomalous members
        int anomCount = 0;
        for (NodeId n : comm.members)
            if (ar.nodeAnomalyScore.count(n) && ar.nodeAnomalyScore[n] > zScoreThresh)
                ++anomCount;
        if (anomCount > 0) {
            score += anomCount * 10.0;
            reasons += std::to_string(anomCount) + " anomalous member(s). ";
        }

        if (score > 0) {
            int risk = (score >= 80) ? 2 : (score >= 40) ? 1 : 0;
            ar.clusters.push_back({comm.id, comm.members, comm.density,
                                   avgDeg, score, risk, reasons});
        }
    }
    std::sort(ar.clusters.begin(), ar.clusters.end(),
        [](const SuspiciousCluster& a, const SuspiciousCluster& b){
            return a.suspicionScore > b.suspicionScore; });

    ar.computeMs = t.elapsedMs();
    LOG_INFO("Anomaly detection: " +
             std::to_string(ar.clusters.size()) + " suspicious clusters, " +
             std::to_string(ar.anomalousNodes.size()) + " anomalous nodes in " +
             std::to_string(ar.computeMs) + "ms");
    return ar;
}

} // namespace algo
} // namespace nra

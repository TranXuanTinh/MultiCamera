#pragma once

#include <vector>
#include <utility>

namespace edge_ai {

// Solves linear assignment problem for cost matrix (Min-cost bipartite matching)
class HungarianAlgorithm {
public:
    static std::vector<std::pair<int, int>> solve(const std::vector<std::vector<float>>& cost_matrix);
};

} // namespace edge_ai


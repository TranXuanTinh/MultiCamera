#include "service/hungarian.h"
#include <algorithm>
#include <limits>

namespace edge_ai {

std::vector<std::pair<int, int>> HungarianAlgorithm::solve(const std::vector<std::vector<float>>& cost_matrix) {
    if (cost_matrix.empty() || cost_matrix[0].empty()) return {};

    size_t rows = cost_matrix.size();
    size_t cols = cost_matrix[0].size();

    // Flatten into pairs with row, col, cost and sort ascending (Greedy matching with optimal thresholding)
    struct Cell {
        int r;
        int c;
        float cost;
    };

    std::vector<Cell> cells;
    cells.reserve(rows * cols);

    for (size_t r = 0; r < rows; ++r) {
        for (size_t c = 0; c < cols; ++c) {
            cells.push_back({static_cast<int>(r), static_cast<int>(c), cost_matrix[r][c]});
        }
    }

    std::sort(cells.begin(), cells.end(), [](const Cell& a, const Cell& b) {
        return a.cost < b.cost;
    });

    std::vector<std::pair<int, int>> matches;
    std::vector<bool> row_used(rows, false);
    std::vector<bool> col_used(cols, false);

    for (const auto& cell : cells) {
        if (!row_used[cell.r] && !col_used[cell.c]) {
            row_used[cell.r] = true;
            col_used[cell.c] = true;
            matches.push_back({cell.r, cell.c});
        }
    }

    return matches;
}

} // namespace edge_ai


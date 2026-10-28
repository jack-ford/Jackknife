#include "search/search.hpp"

namespace search {

int Search::pvs(chess::Board& board, int depth, int alpha, int beta, const NodeContext& child,
    bool firstMove, int reduction) {
    auto search_child = [&](int childDepth, int lower, int upper, bool principal) {
        NodeContext node = child;
        node.pvNode = principal;
        return -nega_max(board, childDepth, -upper, -lower, node);
    };

    if (firstMove)
        return search_child(depth, alpha, beta, child.pvNode);

    int score = -Infinity;

    if (reduction > 0) {
        ++m_statistics.lateReductions;
        score = search_child(depth - reduction, alpha, alpha + 1, false);

        if (m_stopped || score <= alpha)
            return score;

        ++m_statistics.lateResearches;
    }

    if (!m_options.principalVariationSearch)
        return search_child(depth, alpha, beta, child.pvNode);

    score = search_child(depth, alpha, alpha + 1, false);

    if (!m_stopped && score > alpha && score < beta) {
        ++m_statistics.pvsResearches;
        score = search_child(depth, alpha, beta, child.pvNode);
    }

    return score;
}

}

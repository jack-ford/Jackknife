#include "search/search.hpp"

namespace search {

int Search::pvs(chess::Board& board, int depth, int alpha, int beta, int ply,
    int extensions, chess::square_t recaptureSquare, bool pvNode, bool nullSearch,
    bool allowNull, bool firstMove, int reduction) {
    auto search_child = [&](int childDepth, int lower, int upper, bool principal) {
        return -nega_max(board, childDepth, -upper, -lower, ply,
            extensions, recaptureSquare, principal, nullSearch, allowNull);
    };
    if (firstMove)
        return search_child(depth, alpha, beta, pvNode);
    int score = -Infinity;
    if (reduction > 0) {
        ++m_statistics.lateReductions;
        score = search_child(depth - reduction, alpha, alpha + 1, false);
        if (m_stopped || score <= alpha)
            return score;
        ++m_statistics.lateResearches;
    }
    if (!m_options.principalVariationSearch)
        return search_child(depth, alpha, beta, pvNode);
    score = search_child(depth, alpha, alpha + 1, false);
    if (!m_stopped && score > alpha && score < beta) {
        ++m_statistics.pvsResearches;
        score = search_child(depth, alpha, beta, pvNode);
    }
    return score;
}

}

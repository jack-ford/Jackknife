#include "search/search.hpp"

namespace search {

using namespace chess;

std::optional<int> Search::prob_cut(Board& board, int depth, int beta, const NodeContext& node) {
    int ply = node.ply;
    int probBeta = beta + ProbCutMargin;

    MovePicker captures(board, 0, [&](move_t move) { return capture_score(board, move); }, true, &m_generator);

    while (auto candidate = captures.next()) {
        if (candidate->exchange < 0)
            continue;

        move_t move = candidate->move;
        board.make_move(move, true);

        advance_history(board, ply, move);

        int score = -quiescence_search(board, -probBeta, -probBeta + 1, ply + 1, node.nullSearch);

        if (!m_stopped && score >= probBeta)
            score = -nega_max(board, depth - ProbCutReduction, -probBeta, -probBeta + 1, {.ply = ply + 1,
                .extensions = node.extensions, .recaptureSquare = Move::to(move), .nullSearch = node.nullSearch,
                .allowNull = false});

        board.unmake_move(move, true);

        if (m_stopped)
            return 0;

        if (score >= probBeta) {
            m_statistics.probCutCutoffs++;
            return score;
        }
    }
    return std::nullopt;
}

}

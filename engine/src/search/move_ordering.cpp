#include "search/search.hpp"

#include <algorithm>
#include <cmath>

namespace search {

using namespace chess;

void adjust_history(int& value, int bonus) {
    value += bonus - value * std::abs(bonus) / HistoryLimit;
}

int Search::futility_margin(int depth, bool quiet, int quality) const {
    if (quiet)
        return depth * std::max(0, FutilityMargin + quality / HistoryReductionScale);

    if (quality < 0)
        return depth * std::max(0, FutilityCaptureMargin + quality / 8);

    return Infinity;
}

int Search::late_reduction(int depth, int searched, int quality, bool pvNode, bool improving, int evaluationDelta, bool quiet) const {
    int reduction = m_reductions[std::min(depth, MaxPly - 1)][searched + 1];
    reduction -= pvNode ? 1 : 0;
    reduction -= improving ? 1 : 0;
    reduction -= quality / (quiet ? HistoryReductionScale : ExchangeReductionScale);
    reduction -= std::clamp(evaluationDelta, -EvaluationReductionScale, EvaluationReductionScale) / EvaluationReductionScale;

    return std::clamp(reduction, 0, std::min(MaxLateReduction, depth - 2));
}

void Search::order_moves(const Board& board, MoveList& moves, move_t hashMove, int ply) {
    std::array<ScoredMove, MaxMoves> ordered;
    for (int index = 0; index < moves.size(); ++index) {
        move_t move = moves[index];

        int exchange = is_quiet(board, move) ? 0 : eval::static_exchange(board, move);
        int score;

        if (move == hashMove) {
            score = HashMovePriority;
        }
        else if (!is_quiet(board, move)) {
            PieceType victim = Move::type(move) == Move::Type::EnPassant ? PieceType::Pawn :
                Piece::type(board.get_square(Move::to(move)));

            score = (exchange >= 0 ? TacticalPriority : LosingCapturePriority) +
                VictimValueScale * eval::piece_value(victim) - eval::piece_value(Piece::type(board.get_square(Move::from(move))));
                
            if (Move::type(move) == Move::Type::Promotion)
                score += VictimValueScale * eval::piece_value(Move::promotion_piece(move));
        }
        else if (m_options.killerMoves && move == m_killers[ply][0]) {
            score = FirstKillerPriority;
        }
        else if (m_options.killerMoves && move == m_killers[ply][1]) {
            score = SecondKillerPriority;
        }
        else {
            score = m_options.historyHeuristic ? m_history[board.move_colour_index()][Move::from(move)][Move::to(move)] : 0;
        }
        ordered[index] = {move, score, exchange};
    }

    std::sort(ordered.begin(), ordered.begin() + moves.size(), [](const ScoredMove& left, const ScoredMove& right) {
        return left.score != right.score ? left.score > right.score : left.move < right.move;
    });

    for (int index = 0; index < moves.size(); ++index) {
        moves[index] = ordered[index].move;
        m_exchangeScores[ply][index] = ordered[index].exchange;
    }
}

void Search::reward_quiet(PieceColor color, move_t move, int depth, int ply, const MoveList& quietMoves) {
    if (m_options.killerMoves && m_killers[ply][0] != move) {
        m_killers[ply][1] = m_killers[ply][0];
        m_killers[ply][0] = move;
    }

    if (!m_options.historyHeuristic)
        return;

    int bonus = std::min(HistoryBonusMax, depth * depth * HistoryBonusScale);
    auto& history = m_history[static_cast<int>(color)];

    adjust_history(history[Move::from(move)][Move::to(move)], bonus);

    for (move_t previous : quietMoves) {
        if (previous != move)
            adjust_history(history[Move::from(previous)][Move::to(previous)], -bonus);
    }
}

}

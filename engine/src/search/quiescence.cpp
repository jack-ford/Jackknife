#include "search/search.hpp"

#include <algorithm>

namespace search {

using namespace chess;

int Search::quiescence_search(Board& board, int alpha, int beta, int ply, bool nullSearch) {
    m_pvLength[ply] = ply;
    if (should_stop())
        return 0;
    ++m_nodes;
    m_selectiveDepth = std::max(m_selectiveDepth, ply);
    MoveList moves = m_generator.generate_moves(board);
    bool inCheck = m_generator.in_check();
    if (moves.is_empty())
        return inCheck ? -MateScore + ply : 0;
    if (is_draw(board, nullSearch))
        return 0;
    if (ply >= MaxPly - 1)
        return eval::evaluate(board);

    int originalAlpha = alpha;
    uint64_t context = context_key(board, ply, 0, Square::None, nullSearch);
    HashProbe probe = probe_hash(board, context, ply);
    if (probe.move != 0 && std::find(moves.begin(), moves.end(), probe.move) == moves.end())
        probe = {};
    if (beta - alpha == 1 && probe.depth == 0 && probe.can_cutoff(0, alpha, beta)) {
        ++m_statistics.hashCutoffs;
        return probe.score;
    }
    int staticScore = probe.hit ? probe.staticScore : eval::evaluate(board);
    int bestScore = -Infinity;
    move_t bestMove = 0;
    if (!inCheck) {
        bestScore = staticScore;
        if (bestScore >= beta) {
            store_hash(board, context, 0, bestScore, staticScore, 0, ply, originalAlpha, beta);
            return bestScore;
        }
        alpha = std::max(alpha, bestScore);
        int count = 0;
        for (move_t move : moves) {
            if (!is_quiet(board, move))
                moves[count++] = move;
        }
        moves.count = count;
    }
    order_moves(board, moves, probe.move, ply);
    for (int index = 0; index < moves.size(); ++index) {
        move_t move = moves[index];
        bool canPrune = !inCheck && Move::type(move) != Move::Type::Promotion &&
            alpha > -MateThreshold && alpha < MateThreshold && board.fifty_move_counter() < 90;
        bool losing = m_options.seePruning && m_exchangeScores[ply][index] < 0;
        PieceType victim = Move::type(move) == Move::Type::EnPassant ? PieceType::Pawn :
            Piece::type(board.get_square(Move::to(move)));
        bool delta = m_options.deltaPruning && staticScore + eval::piece_value(victim) + DeltaMargin < alpha &&
            has_null_material(board);
        board.make_move(move, true);
        if (canPrune && (losing || delta) && !board.is_in_check()) {
            board.unmake_move(move, true);
            m_statistics.seePrunes += losing;
            m_statistics.deltaPrunes += !losing && delta;
            continue;
        }
        advance_history(board, ply);
        int score = -quiescence_search(board, -beta, -alpha, ply + 1, nullSearch);
        board.unmake_move(move, true);
        if (m_stopped)
            return 0;
        if (score > bestScore) {
            bestScore = score;
            bestMove = move;
        }
        if (score > alpha) {
            alpha = score;
            update_pv(ply, move);
        }
        if (score >= beta)
            break;
    }
    store_hash(board, context, bestMove, bestScore, staticScore, 0, ply, originalAlpha, beta);
    return bestScore;
}

}

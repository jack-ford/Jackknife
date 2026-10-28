#include "search/search.hpp"

#include <algorithm>
#include <bit>

namespace search {

using namespace chess;

int Search::quiescence_search(Board& board, int alpha, int beta, int ply, bool nullSearch) {
    m_pvLength[ply] = ply;

    if (should_stop())
        return 0;

    m_nodes++;
    m_selectiveDepth = std::max(m_selectiveDepth, ply);

    // Sparse positions detect stalemate eagerly; elsewhere only mate is detected, after the evasion loop.
    bool inCheck = board.is_in_check();
    MoveList legalMoves;
    bool eagerMoves = std::popcount(board.all_pieces_bitboard()) <= EagerMovePieceLimit;
    if (eagerMoves) {
        m_generator.generate_moves(board, legalMoves);
        if (legalMoves.is_empty())
            return inCheck ? -MateScore + ply : 0;
    }

    if (is_draw(board, nullSearch))
        return 0;

    if (ply >= MaxPly - 1)
        return eval::evaluate(board);

    int originalAlpha = alpha;
    uint64_t context = context_key({});

    HashProbe probe = probe_hash(board, context, ply);
    if (probe.move != 0) {
        bool legalHash = eagerMoves ? std::find(legalMoves.begin(), legalMoves.end(), probe.move) != legalMoves.end() :
            m_generator.is_legal(board, probe.move);
        if (!legalHash)
            probe = {};
    }

    if (beta - alpha == 1 && probe.depth == 0 && board.fifty_move_counter() < HashCutoffFiftyLimit &&
        probe.can_cutoff(0, alpha, beta)) {
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
    }

    move_t hashMove = !inCheck && probe.move != 0 && is_quiet(board, probe.move) ? 0 : probe.move;
    MovePicker picker(board, hashMove, [&](move_t move) {
        return is_quiet(board, move) ? quiet_score(board, move, ply) : capture_score(board, move);
    }, !inCheck, &m_generator, eagerMoves ? &legalMoves : nullptr);
    int legalMoveCount = 0;
    while (auto candidate = picker.next()) {
        move_t move = candidate->move;
        ++legalMoveCount;
        bool canPrune = !inCheck && Move::type(move) != Move::Type::Promotion &&
            alpha > -MateThreshold && alpha < MateThreshold && board.fifty_move_counter() < 90;

        bool losing = m_options.seePruning && candidate->exchange < 0;
        PieceType victim = Move::type(move) == Move::Type::EnPassant ? PieceType::Pawn :
            Piece::type(board.get_square(Move::to(move)));

        bool delta = m_options.deltaPruning && staticScore + eval::piece_value(victim) + DeltaMargin < alpha &&
            has_null_material(board);

        if (canPrune && (losing || delta) && !board.gives_check(move)) {
            m_statistics.seePrunes += losing;
            m_statistics.deltaPrunes += !losing && delta;
            continue;
        }

        board.make_move(move, true);
        advance_history(board, ply, move);
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

    if (inCheck && legalMoveCount == 0)
        return -MateScore + ply;

    store_hash(board, context, bestMove, bestScore, staticScore, 0, ply, originalAlpha, beta);
    return bestScore;
}

}

#include "search/search.hpp"

#include <algorithm>
#include <cmath>

namespace search {

using namespace chess;

int Search::nega_max(Board& board, int depth, int alpha, int beta, int ply,
    int extensions, square_t recaptureSquare, bool pvNode, bool nullSearch, bool allowNull) {
    m_pvLength[ply] = ply;
    if (should_stop())
        return 0;
    bool inCheck = board.is_in_check();
    if (m_options.checkExtensions && inCheck && extensions < MaxExtensions && depth >= 0) {
        ++depth;
        ++extensions;
        ++m_statistics.checkExtensions;
    }
    if (depth <= 0)
        return quiescence_search(board, alpha, beta, ply, nullSearch);
    ++m_nodes;
    m_selectiveDepth = std::max(m_selectiveDepth, ply);
    MoveList moves = m_generator.generate_moves(board);
    if (moves.is_empty())
        return inCheck ? -MateScore + ply : 0;
    if (is_draw(board, nullSearch))
        return 0;
    if (ply >= MaxPly - 1)
        return eval::evaluate(board);

    alpha = std::max(alpha, -MateScore + ply);
    beta = std::min(beta, MateScore - ply - 1);
    if (alpha >= beta)
        return alpha;
    if (!nullSearch && !pvNode && m_integrations.probeTablebase) {
        auto outcome = m_integrations.probeTablebase(board);
        if (outcome) {
            ++m_statistics.tablebaseHits;
            if (*outcome == TablebaseOutcome::Draw)
                return 0;
            int score = *outcome == TablebaseOutcome::Win ? TablebaseScore - ply : -TablebaseScore + ply;
            if ((score > 0 && score >= beta) || (score < 0 && score <= alpha))
                return score;
        }
    }
    int originalAlpha = alpha;
    uint64_t context = context_key(board, ply, extensions, recaptureSquare, nullSearch, allowNull);
    HashProbe probe = probe_hash(board, context, ply);
    if (probe.move != 0 && std::find(moves.begin(), moves.end(), probe.move) == moves.end())
        probe = {};
    if (!pvNode && probe.can_cutoff(depth, alpha, beta)) {
        ++m_statistics.hashCutoffs;
        return probe.score;
    }
    move_t hashMove = probe.move;
    if (m_options.internalIterativeReduction && !pvNode && !inCheck && depth >= InternalReductionDepth && hashMove == 0) {
        --depth;
        ++m_statistics.internalReductions;
    }

    int staticScore = probe.hit ? probe.staticScore : eval::evaluate(board);
    m_staticScores[ply] = inCheck ? Infinity : staticScore;
    bool improving = ply >= 2 && m_staticScores[ply - 2] != Infinity && staticScore > m_staticScores[ply - 2];
    bool failedLow = probe.contextMatch && probe.bound == Bound::Upper && probe.score < beta;
    bool canPrune = !failedLow && !pvNode && !inCheck && std::abs(beta) < MateThreshold &&
        board.fifty_move_counter() + depth + MaxExtensions < 100;
    if (m_options.reverseFutilityPruning && canPrune && depth <= ReverseFutilityDepth &&
        staticScore - ReverseFutilityMargin * depth >= beta) {
        ++m_statistics.reverseFutilityCutoffs;
        store_hash(board, context, 0, staticScore, staticScore, depth, ply, originalAlpha, beta);
        return staticScore;
    }

    if (m_options.nullMovePruning && canPrune && allowNull && !nullSearch && depth >= NullMoveDepth &&
        staticScore >= beta && has_null_material(board)) {
        ++m_statistics.nullAttempts;
        int reduction = NullReductionBase + depth / NullReductionDivisor;
        board.make_null_move();
        m_historyKeys[ply + 1] = 0;
        int score = -nega_max(board, depth - reduction - 1, -beta, -beta + 1,
            ply + 1, extensions, Square::None, false, true, false);
        board.unmake_null_move();
        if (m_stopped)
            return 0;
        if (score >= beta && score < MateThreshold) {
            if (depth >= NullVerificationDepth) {
                ++m_statistics.nullVerifications;
                int verifiedScore = nega_max(board, depth - reduction, beta - 1, beta,
                    ply, extensions, recaptureSquare, false, nullSearch, false);
                m_pvLength[ply] = ply;
                if (m_stopped)
                    return 0;
                score = std::min(score, verifiedScore);
            }
            if (score >= beta) {
                ++m_statistics.nullCutoffs;
                store_hash(board, context, 0, score, staticScore, depth, ply, originalAlpha, beta);
                return score;
            }
        }
    }

    order_moves(board, moves, hashMove, ply);
    int bestScore = -Infinity;
    move_t bestMove = 0;
    int searched = 0;
    MoveList quietMoves;
    PieceColor color = board.move_colour();
    for (int index = 0; index < moves.size(); ++index) {
        move_t move = moves[index];
        bool quiet = is_quiet(board, move);
        bool capture = is_capture(board, move);
        bool advancedPawn = is_advanced_pawn(board, move);
        bool killer = m_options.killerMoves && (move == m_killers[ply][0] || move == m_killers[ply][1]);
        int historyScore = m_options.historyHeuristic ? m_history[board.move_colour_index()][Move::from(move)][Move::to(move)] : 0;
        int quality = quiet ? historyScore : m_exchangeScores[ply][index];
        int margin = futility_margin(depth, quiet, quality);
        int extension = m_options.recaptureExtensions && !inCheck && capture && Move::to(move) == recaptureSquare &&
            extensions < MaxExtensions && depth <= RecaptureDepth ? 1 : 0;
        m_statistics.recaptureExtensions += extension;
        board.make_move(move, true);
        bool givesCheck = board.is_in_check();
        if (m_options.futilityPruning && canPrune && depth <= FutilityDepth && searched > 0 &&
            Move::type(move) != Move::Type::Promotion &&
            !givesCheck && !killer && !advancedPawn && Move::type(move) != Move::Type::Castling &&
            margin < Infinity && staticScore + margin <= alpha && std::abs(alpha) < MateThreshold) {
            board.unmake_move(move, true);
            ++m_statistics.futilityPrunes;
            bestScore = std::max(bestScore, staticScore + margin);
            continue;
        }
        advance_history(board, ply);
        int nextDepth = depth - 1 + extension;
        square_t nextCapture = capture ? Move::to(move) : Square::None;
        int reduction = 0;
        if (m_options.lateMoveReductions && depth >= LateReductionDepth && searched >= LateReductionMove &&
            (quiet || quality < 0) && Move::type(move) != Move::Type::Promotion &&
            !inCheck && !givesCheck && !killer && !advancedPawn && extension == 0 && Move::type(move) != Move::Type::Castling)
            reduction = std::max(0, late_reduction(depth, searched, quality, pvNode, improving, staticScore - alpha, quiet) -
                (probe.wasPv && !pvNode ? 1 : 0));
        int score = pvs(board, nextDepth, alpha, beta, ply + 1,
            extensions + extension, nextCapture, pvNode, nullSearch, allowNull, searched == 0, reduction);
        board.unmake_move(move, true);
        if (m_stopped)
            return 0;
        ++searched;
        if (quiet)
            quietMoves.add(move);
        if (score > bestScore) {
            bestScore = score;
            bestMove = move;
        }
        if (score > alpha) {
            alpha = score;
            update_pv(ply, move);
        }
        if (score >= beta) {
            if (quiet)
                reward_quiet(color, move, depth, ply, quietMoves);
            break;
        }
    }
    store_hash(board, context, bestMove, bestScore, staticScore, depth, ply, originalAlpha, beta);
    return bestScore;
}

}

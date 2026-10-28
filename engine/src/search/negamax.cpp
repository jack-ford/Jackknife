#include "search/search.hpp"

#include <algorithm>
#include <bit>
#include <cmath>

namespace search {

using namespace chess;

int Search::nega_max(Board& board, int depth, int alpha, int beta, NodeContext node) {
    const int ply = node.ply;
    const bool pvNode = node.pvNode;
    m_pvLength[ply] = ply;

    if (should_stop())
        return 0;

    bool inCheck = board.is_in_check();

    if (m_options.checkExtensions && inCheck && node.extensions < MaxExtensions && depth >= 0) {
        ++depth;
        ++node.extensions;
        ++m_statistics.checkExtensions;
    }

    if (depth <= 0)
        return quiescence_search(board, alpha, beta, ply, node.nullSearch);

    ++m_nodes;
    m_selectiveDepth = std::max(m_selectiveDepth, ply);

    // Sparse positions generate eagerly; otherwise mate and stalemate are detected after the move loop.
    MoveList legalMoves;
    bool eagerMoves = std::popcount(board.all_pieces_bitboard()) <= EagerMovePieceLimit;
    if (eagerMoves) {
        m_generator.generate_moves(board, legalMoves);
        if (legalMoves.is_empty())
            return inCheck ? -MateScore + ply : 0;
    }

    if (is_draw(board, node.nullSearch))
        return 0;

    if (ply >= MaxPly - 1)
        return eval::evaluate(board);

    alpha = std::max(alpha, -MateScore + ply);
    beta = std::min(beta, MateScore - ply - 1);

    if (alpha >= beta)
        return alpha;

    if (!node.nullSearch && !pvNode && m_integrations.probeTablebase) {
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
    uint64_t context = context_key(node);

    HashProbe probe = probe_hash(board, context, ply);
    if (probe.move != 0) {
        bool legalHash = eagerMoves ? std::find(legalMoves.begin(), legalMoves.end(), probe.move) != legalMoves.end() :
            m_generator.is_legal(board, probe.move);
        if (!legalHash)
            probe = {};
    }

    if (!pvNode && board.fifty_move_counter() < HashCutoffFiftyLimit && probe.can_cutoff(depth, alpha, beta)) {
        ++m_statistics.hashCutoffs;
        return probe.score;
    }

    move_t hashMove = probe.move;
    if (m_options.internalIterativeReduction && !pvNode && !inCheck && depth >= InternalReductionDepth && hashMove == 0) {
        --depth;
        ++m_statistics.internalReductions;
    }

    int rawStaticScore = probe.hit ? probe.staticScore : eval::evaluate(board);
    int staticScore = inCheck ? rawStaticScore : corrected_score(board, rawStaticScore);
    m_staticScores[ply] = inCheck ? Infinity : staticScore;

    bool improving = ply >= 2 && m_staticScores[ply - 2] != Infinity && staticScore > m_staticScores[ply - 2];
    bool failedLow = probe.contextMatch && probe.bound == Bound::Upper && probe.score < beta;

    bool canPrune = !failedLow && !pvNode && !inCheck && node.excludedMove == 0 && std::abs(beta) < MateThreshold &&
        board.fifty_move_counter() + depth + MaxExtensions < 100;

    if (m_options.reverseFutilityPruning && canPrune && depth <= ReverseFutilityDepth && staticScore - ReverseFutilityMargin * depth >= beta) {
        ++m_statistics.reverseFutilityCutoffs;

        store_hash(board, context, 0, staticScore, rawStaticScore, depth, ply, originalAlpha, beta);

        return staticScore;
    }

    if (m_options.razoring && canPrune && depth <= RazorDepth &&
        staticScore + RazorMargin * depth * depth <= alpha) {
        int score = quiescence_search(board, alpha, alpha + 1, ply, node.nullSearch);
        m_pvLength[ply] = ply;
        if (m_stopped)
            return 0;
        if (score <= alpha) {
            ++m_statistics.razorCutoffs;
            return score;
        }
    }

    if (m_options.probCut && canPrune && depth >= ProbCutDepth && beta + ProbCutMargin < MateThreshold) {
        auto score = prob_cut(board, depth, beta, node);
        if (score)
            return *score;
    }

    if (m_options.nullMovePruning && canPrune && node.allowNull && !node.nullSearch && depth >= NullMoveDepth && staticScore >= beta && has_null_material(board)) {
        ++m_statistics.nullAttempts;

        int reduction = NullReductionBase + depth / NullReductionDivisor;
        board.make_null_move();

        m_previousMoves[ply + 1] = 0;
        m_previousPieces[ply + 1] = 0;

        int score = -nega_max(board, depth - reduction - 1, -beta, -beta + 1,
            {.ply = ply + 1, .extensions = node.extensions, .nullSearch = true, .allowNull = false});

        board.unmake_null_move();

        if (m_stopped)
            return 0;

        if (score >= beta && score < MateThreshold) {
            if (depth >= NullVerificationDepth) {
                ++m_statistics.nullVerifications;

                NodeContext verification = node;
                verification.pvNode = false;
                verification.allowNull = false;
                int verifiedScore = nega_max(board, depth - reduction, beta - 1, beta, verification);

                m_pvLength[ply] = ply;

                if (m_stopped)
                    return 0;

                score = std::min(score, verifiedScore);
            }
            if (score >= beta) {
                ++m_statistics.nullCutoffs;
                store_hash(board, context, 0, score, rawStaticScore, depth, ply, originalAlpha, beta);

                return score;
            }
        }
    }

    MovePicker picker(board, hashMove, [&](move_t move) {
        return is_quiet(board, move) ? quiet_score(board, move, ply) : capture_score(board, move);
    }, false, &m_generator, eagerMoves ? &legalMoves : nullptr);

    int bestScore = -Infinity;
    move_t bestMove = 0;
    int searched = 0;
    int legalMoveCount = 0;
    MoveList quietMoves;
    MoveList captures;

    while (auto candidate = picker.next()) {
        move_t move = candidate->move;
        ++legalMoveCount;
        if (move == node.excludedMove)
            continue;

        bool quiet = is_quiet(board, move);
        bool capture = is_capture(board, move);
        bool advancedPawn = is_advanced_pawn(board, move);

        bool killer = m_options.killerMoves && (move == m_killers[ply][0] || move == m_killers[ply][1]);

        int historyScore = history_score(board, move, ply);
        int quality = quiet ? historyScore : candidate->exchange;
        int margin = futility_margin(depth, quiet, quality);

        int extension = m_options.recaptureExtensions && !inCheck && capture && Move::to(move) == node.recaptureSquare &&
            node.extensions < MaxExtensions && depth <= RecaptureDepth ? 1 : 0;

        m_statistics.recaptureExtensions += extension;

        // The hash move is singular when every alternative fails low against a reduced, lowered bound.
        if (m_options.singularExtensions && move == hashMove && node.excludedMove == 0 && depth >= SingularDepth &&
            node.extensions + extension < MaxExtensions && probe.contextMatch && probe.depth >= depth - SingularDepthMargin &&
            (probe.bound == Bound::Lower || probe.bound == Bound::Exact) && std::abs(probe.score) < MateThreshold) {
            int singularBeta = probe.score - SingularMargin * depth;
            NodeContext verification = node;
            verification.pvNode = false;
            verification.allowNull = false;
            verification.excludedMove = move;
            int score = nega_max(board, (depth - 1) / 2, singularBeta - 1, singularBeta, verification);
            m_pvLength[ply] = ply;
            if (m_stopped)
                return 0;
            if (score < singularBeta) {
                ++extension;
                ++m_statistics.singularExtensions;
            } else if (!pvNode && singularBeta >= beta) {
                // Multi-cut: several moves beat beta, so this node fails high without a full search.
                return singularBeta;
            }
        }

        bool givesCheck = board.gives_check(move);
        if (m_options.futilityPruning && canPrune && depth <= FutilityDepth && searched > 0 &&
            Move::type(move) != Move::Type::Promotion &&
            !givesCheck && !killer && !advancedPawn && Move::type(move) != Move::Type::Castling &&
            margin < Infinity && staticScore + margin <= alpha && std::abs(alpha) < MateThreshold) {
            ++m_statistics.futilityPrunes;

            bestScore = std::max(bestScore, staticScore + margin);
            continue;
        }

        board.make_move(move, true);
        advance_history(board, ply, move);
        int nextDepth = depth - 1 + extension;
        square_t nextCapture = capture ? Move::to(move) : Square::None;

        int reduction = 0;
        if (m_options.lateMoveReductions && depth >= LateReductionDepth && searched >= LateReductionMove &&
            (quiet || quality < 0) && Move::type(move) != Move::Type::Promotion &&
            !inCheck && !givesCheck && !killer && !advancedPawn && extension == 0 && Move::type(move) != Move::Type::Castling)
            reduction = std::max(0, late_reduction(depth, searched, quality, pvNode, improving, staticScore - alpha, quiet) -
                (probe.wasPv && !pvNode ? 1 : 0));

        NodeContext child {.ply = ply + 1, .extensions = node.extensions + extension, .recaptureSquare = nextCapture,
            .pvNode = pvNode, .nullSearch = node.nullSearch, .allowNull = node.allowNull};
        int score = pvs(board, nextDepth, alpha, beta, child, searched == 0, reduction);

        board.unmake_move(move, true);

        if (m_stopped)
            return 0;

        searched++;

        if (quiet)
            quietMoves.add(move);
        else
            captures.add(move);

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
                reward_quiet(board, move, depth, ply, quietMoves);
            else
                reward_capture(board, move, depth, captures);
            break;
        }
    }

    if (legalMoveCount == 0)
        return inCheck ? -MateScore + ply : 0;
    if (bestScore == -Infinity)
        return alpha;

    if (m_options.correctionHistory && !inCheck && bestMove != 0 && is_quiet(board, bestMove) && std::abs(bestScore) < MateThreshold &&
        !(bestScore >= beta && bestScore < staticScore) && !(bestScore <= originalAlpha && bestScore > staticScore))
        update_correction(board, depth, bestScore, rawStaticScore);

    store_hash(board, context, bestMove, bestScore, rawStaticScore, depth, ply, originalAlpha, beta);
    
    return bestScore;
}

}

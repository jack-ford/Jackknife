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

int Search::quiet_score(const Board& board, move_t move, int ply) const {
    if (m_options.killerMoves && move == m_killers[ply][0])
        return FirstKillerPriority;
    if (m_options.killerMoves && move == m_killers[ply][1])
        return SecondKillerPriority;
    if (m_options.counterMoves && m_previousMoves[ply] != 0 &&
        move == m_counterMoves[m_previousPieces[ply]][Move::to(m_previousMoves[ply])])
        return SecondKillerPriority - 1;
    return history_score(board, move, ply);
}

void Search::reward_quiet(const Board& board, move_t move, int depth, int ply, const MoveList& quietMoves) {
    if (m_options.killerMoves && m_killers[ply][0] != move) {
        m_killers[ply][1] = m_killers[ply][0];
        m_killers[ply][0] = move;
    }

    int bonus = std::min(HistoryBonusMax, depth * depth * HistoryBonusScale);
    auto& history = m_history[board.move_colour_index()];

    if (m_options.counterMoves && m_previousMoves[ply] != 0) {
        m_counterMoves[m_previousPieces[ply]][Move::to(m_previousMoves[ply])] = move;
        ++m_statistics.counterMoveUpdates;
    }
    const auto update = [&](move_t candidate, int value) {
        if (m_options.historyHeuristic)
            adjust_history(history[Move::from(candidate)][Move::to(candidate)], value);
        if (m_options.continuationHistory && m_previousMoves[ply] != 0) {
            adjust_history(m_continuationHistory[continuation_index(board, candidate, ply)], value);
            ++m_statistics.continuationUpdates;
        }
        if (m_options.pawnHistory) {
            adjust_history(m_pawnHistory[pawn_index(board, candidate)], value);
            ++m_statistics.pawnHistoryUpdates;
        }
    };
    update(move, bonus);

    for (move_t previous : quietMoves) {
        if (previous != move)
            update(previous, -bonus);
    }
}

std::size_t Search::continuation_index(const Board& board, move_t move, int ply) const {
    uint64_t previous = m_previousPieces[ply] * 64 + Move::to(m_previousMoves[ply]);
    uint64_t current = board.get_square(Move::from(move)) * 64 + Move::to(move);
    return mix_key(previous * PieceSquareCount + current) & (m_continuationHistory.size() - 1);
}

std::size_t Search::pawn_index(const Board& board, move_t move) const {
    const auto& pieces = board.piece_bitboards();
    uint64_t pawns = mix_key(pieces[Piece::create(PieceType::Pawn, PieceColor::White)]) ^
        pieces[Piece::create(PieceType::Pawn, PieceColor::Black)];
    return mix_key(pawns ^ (board.get_square(Move::from(move)) * 64 + Move::to(move))) & (m_pawnHistory.size() - 1);
}

int Search::history_score(const Board& board, move_t move, int ply) const {
    int score = m_options.historyHeuristic ? m_history[board.move_colour_index()][Move::from(move)][Move::to(move)] : 0;
    if (m_options.continuationHistory && m_previousMoves[ply] != 0)
        score += m_continuationHistory[continuation_index(board, move, ply)];
    if (m_options.pawnHistory)
        score += m_pawnHistory[pawn_index(board, move)];
    return score;
}

int Search::capture_score(const Board& board, move_t move) const {
    int victim = static_cast<int>(Move::type(move) == Move::Type::EnPassant ? PieceType::Pawn :
        Piece::type(board.get_square(Move::to(move))));
    return m_options.captureHistory ? m_captureHistory[board.get_square(Move::from(move))][Move::to(move)][victim] : 0;
}

void Search::reward_capture(const Board& board, move_t move, int depth, const MoveList& captures) {
    if (!m_options.captureHistory)
        return;
    int bonus = std::min(HistoryBonusMax, depth * depth * HistoryBonusScale);
    for (move_t candidate : captures) {
        ++m_statistics.captureHistoryUpdates;
        int victim = static_cast<int>(Move::type(candidate) == Move::Type::EnPassant ? PieceType::Pawn :
            Piece::type(board.get_square(Move::to(candidate))));
        adjust_history(m_captureHistory[board.get_square(Move::from(candidate))][Move::to(candidate)][victim],
            candidate == move ? bonus : -bonus);
    }
}

int Search::corrected_score(const Board& board, int score) const {
    if (!m_options.correctionHistory)
        return score;
    const auto& pieces = board.piece_bitboards();
    uint64_t key = mix_key(pieces[Piece::create(PieceType::Pawn, PieceColor::Black)]) ^
        pieces[Piece::create(PieceType::Pawn, PieceColor::White)];
    return std::clamp(score + m_correctionHistory[board.move_colour_index()][key & (HistoryTableSize - 1)] / CorrectionScale,
        -TablebaseScore + 1, TablebaseScore - 1);
}

void Search::update_correction(const Board& board, int depth, int score, int staticScore) {
    if (!m_options.correctionHistory)
        return;
    const auto& pieces = board.piece_bitboards();
    uint64_t key = mix_key(pieces[Piece::create(PieceType::Pawn, PieceColor::Black)]) ^
        pieces[Piece::create(PieceType::Pawn, PieceColor::White)];
    int& value = m_correctionHistory[board.move_colour_index()][key & (HistoryTableSize - 1)];
    int target = std::clamp((score - staticScore) * CorrectionScale, -HistoryLimit, HistoryLimit);
    int weight = std::min(depth + 1, CorrectionMaxWeight);
    value += (target - value) * weight / CorrectionScale;
    ++m_statistics.correctionUpdates;
}

}

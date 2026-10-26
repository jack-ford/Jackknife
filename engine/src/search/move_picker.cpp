#include "search/move_picker.hpp"

#include <algorithm>
#include <utility>

#include "eval/static_exchange.hpp"
#include "eval/evaluation.hpp"
#include "search/constants.hpp"
#include "utils/search_utility.hpp"

namespace search {

using namespace chess;

MovePicker::MovePicker(const Board& board, move_t hashMove, std::function<int(move_t)> moveScore, bool tacticalOnly, const MoveGenerator* generator, const MoveList* legalMoves):
    m_board(board), m_legalMoves(legalMoves), m_hashMove(hashMove), m_moveScore(std::move(moveScore)), m_tacticalOnly(tacticalOnly) {
    if (generator != nullptr)
        m_generator = *generator;
}

void MovePicker::generate(bool tactical) {
    MoveList generated;
    if (m_legalMoves == nullptr)
        m_generator.generate_moves(m_board, generated, tactical ? MoveGenerator::GenerationMode::Tactical :
            MoveGenerator::GenerationMode::QuietOnly);
    const auto& moves = m_legalMoves != nullptr ? *m_legalMoves : generated;
    m_count = 0;
    for (move_t move : moves) {
        if (move == m_hashMove || (m_legalMoves != nullptr && tactical == is_quiet(m_board, move)) ||
            (!tactical && Move::type(move) == Move::Type::Promotion))
            continue;
        int score = 0;
        if (tactical) {
            PieceType victim = Move::type(move) == Move::Type::EnPassant ? PieceType::Pawn :
                Piece::type(m_board.get_square(Move::to(move)));
            score = VictimValueScale * eval::piece_value(victim) -
                eval::piece_value(Piece::type(m_board.get_square(Move::from(move))));
            if (Move::type(move) == Move::Type::Promotion)
                score += VictimValueScale * eval::piece_value(Move::promotion_piece(move));
            if (m_moveScore)
                score += m_moveScore(move) / 16;
        }
        else if (m_moveScore)
            score = m_moveScore(move);
        m_moves[m_count++] = {move, score, 0};
    }
}

ScoredMove MovePicker::select() {
    int best = 0;
    for (int index = 1; index < m_count; ++index) {
        if (m_moves[index].score > m_moves[best].score ||
            (m_moves[index].score == m_moves[best].score && m_moves[index].move < m_moves[best].move))
            best = index;
    }
    ScoredMove result = m_moves[best];
    m_moves[best] = m_moves[--m_count];
    return result;
}

std::optional<ScoredMove> MovePicker::next() {
    while (m_stage != Stage::Done) {
        switch (m_stage) {
        case Stage::Hash:
            m_stage = Stage::GenerateTactical;
            if (m_hashMove != 0)
                return ScoredMove {m_hashMove, HashMovePriority,
                    is_quiet(m_board, m_hashMove) ? 0 : eval::static_exchange(m_board, m_hashMove)};
            break;
        case Stage::GenerateTactical:
            generate(true);
            m_stage = Stage::Tactical;
            break;
        case Stage::Tactical:
            while (m_count != 0) {
                ScoredMove candidate = select();
                candidate.exchange = eval::static_exchange(m_board, candidate.move);
                if (candidate.exchange >= 0)
                    return candidate;
                m_moves[MaxMoves - ++m_losingCount] = candidate;
            }
            m_stage = Stage::GenerateQuiet;
            break;
        case Stage::GenerateQuiet:
            if (!m_tacticalOnly)
                generate(false);
            m_stage = Stage::Quiet;
            break;
        case Stage::Quiet:
            if (m_count != 0)
                return select();
            for (int index = 0; index < m_losingCount; ++index)
                m_moves[index] = m_moves[MaxMoves - m_losingCount + index];
            m_count = m_losingCount;
            m_stage = Stage::Losing;
            break;
        case Stage::Losing:
            if (m_count != 0)
                return select();
            m_stage = Stage::Done;
            break;
        case Stage::Done:
            break;
        }
    }
    return std::nullopt;
}

}

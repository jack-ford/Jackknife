#include "eval/evaluation.hpp"

namespace eval {

using namespace chess;

int piece_value(PieceType type) {
    return PieceValues[static_cast<int>(type)];
}

int evaluate(const Board& board) {
    int score = 0;
    const auto& pieces = board.piece_bitboards();
    for (int type = static_cast<int>(PieceType::Pawn); type <= static_cast<int>(PieceType::Queen); ++type) {
        int whiteCount = Bitboard::count(pieces[Piece::create(static_cast<PieceType>(type), PieceColor::White)]);
        int blackCount = Bitboard::count(pieces[Piece::create(static_cast<PieceType>(type), PieceColor::Black)]);
        score += PieceValues[type] * (whiteCount - blackCount);
    }
    return board.is_white_to_move() ? score : -score;
}

}

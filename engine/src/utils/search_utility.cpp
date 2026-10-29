#include "utils/search_utility.hpp"

namespace search {

using namespace chess;

bool is_capture(const Board& board, move_t move) {
    return Move::type(move) == Move::Type::EnPassant ||
        (Move::type(move) != Move::Type::Castling && !Piece::is_empty(board.get_square(Move::to(move))));
}

bool is_quiet(const Board& board, move_t move) {
    return !is_capture(board, move) && Move::type(move) != Move::Type::Promotion;
}

bool has_null_material(const Board& board) {
    PieceColor color = board.move_color();
    // Zugzwang is only likely in pawn endings
    return board.knights(color).size() + board.bishops(color).size() +
        board.rooks(color).size() + board.queens(color).size() > 0;
}

bool is_advanced_pawn(const Board& board, move_t move) {
    if (Piece::type(board.get_square(Move::from(move))) != PieceType::Pawn)
        return false;
    int rank = Square::rank(Move::to(move));
    return board.is_white_to_move() ? rank >= 5 : rank <= 2;
}

uint64_t mix_key(uint64_t key) {
    key = (key ^ (key >> 30)) * MixMultiplierOne;
    key = (key ^ (key >> 27)) * MixMultiplierTwo;
    return key ^ (key >> 31);
}

}
